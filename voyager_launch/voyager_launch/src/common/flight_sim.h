// flight_sim.h - FlightSim: the launch itself (gravity, thrust, air drag, Earth spin, stage events).
// No OpenGL in here, so it can be tested alone. Frame: inertial world (Earth centre = origin, +Y = spin axis).
// The "stack" is everything still attached; its pose is the pose of the rocket's base (local origin).
#pragma once
#include <vector>
#include "frames.h"
#include "vehicle_specs.h"

// Things that happen during the flight. The drawing code reacts to them (e.g. a stage falls away).
enum FlightEventType {
    EVENT_LIFTOFF, EVENT_STAGE1_IGNITION, EVENT_SRM_JETTISON, EVENT_STAGE1_SEPARATION, EVENT_SHROUD_JETTISON,
    EVENT_STAGE2_SEPARATION, EVENT_CENTAUR_IGNITION_1, EVENT_CENTAUR_CUTOFF_1, EVENT_CENTAUR_IGNITION_2,
    EVENT_VOYAGER_SEPARATION, EVENT_KICK_IGNITION, EVENT_KICK_BURNOUT, EVENT_KICK_SEPARATION, EVENT_BOOMS_DEPLOYING, EVENT_MISSION_COMPLETE
};
struct FlightEvent { FlightEventType type; double time; };

// Description: short readable name of an event (for the console / title bar).
inline const char* event_name(FlightEventType t) {
    switch (t) {
        case EVENT_LIFTOFF: return "Liftoff";
        case EVENT_STAGE1_IGNITION: return "Core stage 1 ignition";
        case EVENT_SRM_JETTISON: return "Side boosters jettisoned";
        case EVENT_STAGE1_SEPARATION: return "Stage 1 separation, Stage 2 ignition";
        case EVENT_SHROUD_JETTISON: return "Payload shroud jettisoned";
        case EVENT_STAGE2_SEPARATION: return "Stage 2 separation";
        case EVENT_CENTAUR_IGNITION_1: return "Centaur burn 1";
        case EVENT_CENTAUR_CUTOFF_1: return "Parking orbit reached";
        case EVENT_CENTAUR_IGNITION_2: return "Centaur burn 2 (leave Earth orbit)";
        case EVENT_VOYAGER_SEPARATION: return "Voyager separates from Centaur";
        case EVENT_KICK_IGNITION: return "Kick motor ignition";
        case EVENT_KICK_BURNOUT: return "Kick motor burnout";
        case EVENT_KICK_SEPARATION: return "Kick motor dropped";
        case EVENT_BOOMS_DEPLOYING: return "Voyager deploys its booms";
        case EVENT_MISSION_COMPLETE: return "Launch sequence complete";
    }
    return "?";
}

class FlightSim {
public:
    // ---- state you can read ----
    double mission_time = COUNTDOWN_START_S;    // T+ seconds (negative = countdown)
    dvec3 position{0.0}, velocity{0.0};         // inertial; position = rocket base
    glm::dquat orientation{1.0, 0.0, 0.0, 0.0};
    double boom_deploy = 0.0;                   // 0 = stowed, 1 = fully out
    bool is_srm_burning = false, is_stage1_burning = false, is_stage2_burning = false, is_centaur_burning = false, is_kick_burning = false;
    bool is_srm_attached = true, is_stage1_attached = true, is_stage2_attached = true, is_shroud_attached = true, is_centaur_attached = true, is_kick_attached = true;
    bool has_lifted_off = false;
    double last_air_density = 1.225, last_dynamic_pressure = 0.0;
    int mission_index = 1;

    // Description: put everything back on the pad at T-10 s. In: mission index (0 = V1, 1 = V2).
    void reset(int mission) {
        *this = FlightSim();
        mission_index = mission;
        theta_launch_ = earth_angle_at_launch(MISSIONS[mission]);
        srm_prop_ = propellant_mass(SPEC_SRM_PAIR); stage1_prop_ = propellant_mass(SPEC_STAGE1);
        stage2_prop_ = propellant_mass(SPEC_STAGE2); centaur_prop_ = propellant_mass(SPEC_CENTAUR) * 0.0 + CENTAUR_PROPELLANT_KG;
        kick_prop_ = propellant_mass(SPEC_KICK);
        _hold_on_pad();
    }

    // Description: Earth's spin angle right now (it turns before and after liftoff).
    double earth_angle() const { return theta_launch_ + EARTH_SPIN_RATE * mission_time; }
    double altitude() const { return glm::length(position) - EARTH_RADIUS_M; }
    double speed() const { return glm::length(velocity); }                 // speed seen from space
    double speed_over_ground() const { return glm::length(velocity - earth_surface_velocity(position)); }

    // Description: run the simulation forward. In: seconds of simulated time. Out: events go to the queue.
    void advance(double seconds) {
        while (seconds > 1e-9) {
            double h = glm::min(seconds, MAX_SUBSTEP_S);
            _step(h);
            seconds -= h;
        }
    }

    // Description: hand over (and clear) the events that happened since the last call.
    std::vector<FlightEvent> take_events() { std::vector<FlightEvent> e; e.swap(events_); return e; }

    // Description: a comfortable time-speed-up for each part of the flight (used by "auto warp").
    double recommended_warp() const {
        if (!has_lifted_off) return 1.0;
        if (is_kick_burning || (boom_deploy > 0.0)) return 4.0;
        if (_is_coasting()) return 40.0;
        if (altitude() < 25000.0) return 1.5;
        if (is_centaur_burning) return 8.0;
        return 4.0;
    }
    bool is_mission_complete() const { return complete_; }
    double total_mass() const { return _total_mass(); }

private:
    static constexpr double MAX_SUBSTEP_S = 0.02;
    static constexpr double CENTAUR_PROPELLANT_KG = 13600.0;
    double theta_launch_ = 0.0;
    double srm_prop_ = 0, stage1_prop_ = 0, stage2_prop_ = 0, centaur_prop_ = 0, kick_prop_ = 0;
    double t_srm_burnout_ = -1, t_stage1_burnout_ = -1, t_stage2_burnout_ = -1, t_cutoff1_ = -1, t_centaur_end_ = -1,
           t_kick_burnout_ = -1, t_voyager_sep_ = -1, t_kick_sep_ = -1, t_deploy_start_ = -1;
    bool has_stage1_ignited_ = false, has_centaur_cutoff_1_ = false, has_centaur_ignited_2_ = false, has_voyager_separated_ = false,
         has_kick_ignited_ = false, has_kick_separated_ = false, complete_ = false, has_centaur_ignited_1_ = false;
    std::vector<FlightEvent> events_;

    void _emit(FlightEventType t) { events_.push_back({t, mission_time}); }
    bool _is_coasting() const { return has_centaur_cutoff_1_ && !has_centaur_ignited_2_; }

    // Description: before liftoff the rocket is simply carried around by Earth's spin.
    void _hold_on_pad() {
        SitePose pose = launch_site_pose(earth_angle(), ROCKET_BASE_HEIGHT_M);
        position = pose.position;
        velocity = earth_surface_velocity(position);
        orientation = glm::dquat(pose.basis);
    }

    double _total_mass() const {
        double m = MASS_VOYAGER_KG;
        if (is_srm_attached) m += SPEC_SRM_PAIR.dry_mass_kg + srm_prop_;
        if (is_stage1_attached) m += SPEC_STAGE1.dry_mass_kg + stage1_prop_;
        if (is_stage2_attached) m += SPEC_STAGE2.dry_mass_kg + stage2_prop_;
        if (is_centaur_attached) m += SPEC_CENTAUR.dry_mass_kg + centaur_prop_;
        if (is_shroud_attached) m += MASS_SHROUD_KG;
        if (is_kick_attached) m += SPEC_KICK.dry_mass_kg + kick_prop_;
        return m;
    }
    double _drag_area() const {
        double a = is_shroud_attached ? DRAG_AREA_SHROUD_M2 : DRAG_AREA_CORE_M2;
        if (is_srm_attached) a += DRAG_AREA_SRM_PAIR_M2;
        return a;
    }
    // Description: the direction the rocket pushes (up, tipped over, gravity turn, or guided).
    dvec3 _thrust_direction(double thrust_accel) const {
        dvec3 up = glm::normalize(position);
        dvec3 east = glm::normalize(glm::cross(dvec3(0, 1, 0), up));
        dvec3 north = glm::cross(up, east);
        double az = deg_to_rad(LAUNCH_AZIMUTH_DEG);
        dvec3 heading = std::cos(az) * north + std::sin(az) * east;
        double t = mission_time;
        if (!has_lifted_off || t < T_PITCH_START_S) return up;
        dvec3 v_rel = velocity - earth_surface_velocity(position);
        if (has_voyager_separated_ || has_centaur_ignited_2_) return glm::length(velocity) > 1.0 ? glm::normalize(velocity) : up;   // prograde
        if (is_stage1_attached) {                                                                                                  // pitch program
            double pitch = _pitch_from_table(t);
            return std::cos(deg_to_rad(pitch)) * up + std::sin(deg_to_rad(pitch)) * heading;
        }
        // closed-loop upper stage guidance: climb to the parking altitude, level out, build speed
        double r = glm::length(position), vr = glm::dot(velocity, up);
        dvec3 v_h = velocity - up * vr;
        double vh = glm::length(v_h);
        dvec3 hdir = vh > 1.0 ? v_h / vh : heading;
        double g_eff = EARTH_GM / (r * r) - vh * vh / r;
        double ff = thrust_accel > 1e-6 ? g_eff / thrust_accel : 0.0;
        double s = ff + GUIDANCE_K_ALTITUDE * (PARKING_ORBIT_ALTITUDE_M - altitude()) - GUIDANCE_K_VERTICAL_SPEED * vr;
        s = glm::clamp(s, -GUIDANCE_MAX_PITCH_RATIO, GUIDANCE_MAX_PITCH_RATIO);
        return glm::normalize(hdir + up * s);
    }

    // Description: pitch angle from the table, blended smoothly between points. In: time. Out: degrees from vertical.
    static double _pitch_from_table(double t) {
        const int n = (int)(sizeof(PITCH_TABLE) / sizeof(PITCH_TABLE[0]));
        if (t <= PITCH_TABLE[0].time_s) return PITCH_TABLE[0].pitch_deg;
        for (int i = 1; i < n; i++)
            if (t <= PITCH_TABLE[i].time_s) {
                double f = (t - PITCH_TABLE[i - 1].time_s) / (PITCH_TABLE[i].time_s - PITCH_TABLE[i - 1].time_s);
                return PITCH_TABLE[i - 1].pitch_deg + f * (PITCH_TABLE[i].pitch_deg - PITCH_TABLE[i - 1].pitch_deg);
            }
        return PITCH_TABLE[n - 1].pitch_deg;
    }

    // Description: one small time step: events, thrust, gravity, drag, move.
    void _step(double h) {
        mission_time += h;
        if (mission_time < 0.0) { _hold_on_pad(); return; }
        if (!has_lifted_off) {
            has_lifted_off = true;
            velocity = earth_surface_velocity(position);
            is_srm_burning = true;
            _emit(EVENT_LIFTOFF);
        }
        _update_events();
        _burn_and_move(h);
    }

    void _update_events() {
        double t = mission_time;
        if (!has_stage1_ignited_ && t >= T_STAGE1_IGNITION_S) { has_stage1_ignited_ = true; is_stage1_burning = true; _emit(EVENT_STAGE1_IGNITION); }
        if (is_srm_attached && !is_srm_burning && t_srm_burnout_ > 0 && t >= t_srm_burnout_ + SRM_JETTISON_DELAY_S) { is_srm_attached = false; _emit(EVENT_SRM_JETTISON); }
        if (is_stage1_attached && !is_stage1_burning && t_stage1_burnout_ > 0 && t >= t_stage1_burnout_ + STAGE_SEPARATION_DELAY_S) {
            is_stage1_attached = false; is_stage2_burning = true; _emit(EVENT_STAGE1_SEPARATION);
        }
        if (is_shroud_attached && !is_srm_attached && altitude() > SHROUD_JETTISON_ALTITUDE_M) { is_shroud_attached = false; _emit(EVENT_SHROUD_JETTISON); }
        if (is_stage2_attached && !is_stage2_burning && t_stage2_burnout_ > 0 && t >= t_stage2_burnout_ + STAGE_SEPARATION_DELAY_S) { is_stage2_attached = false; _emit(EVENT_STAGE2_SEPARATION); }
        if (!has_centaur_ignited_1_ && t_stage2_burnout_ > 0 && !is_stage2_attached && t >= t_stage2_burnout_ + STAGE_SEPARATION_DELAY_S + CENTAUR_IGNITION_DELAY_S) {
            has_centaur_ignited_1_ = true; is_centaur_burning = true; _emit(EVENT_CENTAUR_IGNITION_1);
        }
        if (is_centaur_burning && !has_centaur_cutoff_1_ && _has_reached_orbit_speed()) {
            has_centaur_cutoff_1_ = true; is_centaur_burning = false; t_cutoff1_ = t; _emit(EVENT_CENTAUR_CUTOFF_1);
        }
        if (has_centaur_cutoff_1_ && !has_centaur_ignited_2_ && t >= t_cutoff1_ + COAST_DURATION_S && centaur_prop_ > 0) {
            has_centaur_ignited_2_ = true; is_centaur_burning = true; _emit(EVENT_CENTAUR_IGNITION_2);
        }
        if (t_centaur_end_ > 0 && !has_voyager_separated_ && t >= t_centaur_end_ + VOYAGER_SEPARATION_DELAY_S) {
            has_voyager_separated_ = true; is_centaur_attached = false; t_voyager_sep_ = t; _emit(EVENT_VOYAGER_SEPARATION);
        }
        if (has_voyager_separated_ && !has_kick_ignited_ && t >= t_voyager_sep_ + KICK_IGNITION_DELAY_S) { has_kick_ignited_ = true; is_kick_burning = true; _emit(EVENT_KICK_IGNITION); }
        if (t_kick_burnout_ > 0 && !has_kick_separated_ && t >= t_kick_burnout_ + KICK_SEPARATION_DELAY_S) { has_kick_separated_ = true; is_kick_attached = false; t_kick_sep_ = t; _emit(EVENT_KICK_SEPARATION); }
        if (has_kick_separated_ && t_deploy_start_ < 0 && t >= t_kick_sep_ + BOOM_DEPLOY_START_DELAY_S) { t_deploy_start_ = t; _emit(EVENT_BOOMS_DEPLOYING); }
        if (t_deploy_start_ > 0 && boom_deploy < 1.0) {
            boom_deploy = glm::min(1.0, (t - t_deploy_start_) / BOOM_DEPLOY_DURATION_S);
            if (boom_deploy >= 1.0 && !complete_) { complete_ = true; _emit(EVENT_MISSION_COMPLETE); }
        }
    }
    // Description: true when speed is enough for a circular orbit at this height and we are not climbing fast.
    bool _has_reached_orbit_speed() const {
        double r = glm::length(position), v_circ = std::sqrt(EARTH_GM / r);
        double vr = glm::dot(velocity, glm::normalize(position));
        return glm::length(velocity) >= v_circ && std::fabs(vr) < 150.0;
    }

    // Description: burn fuel, add up all pushes and pulls, and move the rocket.
    void _burn_and_move(double h) {
        double alt = altitude(), rho = air_density(alt);
        double thrust = 0.0, thrust_scale = 1.0 - SEA_LEVEL_THRUST_LOSS * glm::min(1.0, rho / SEA_LEVEL_DENSITY);
        if (is_srm_burning) { thrust += SPEC_SRM_PAIR.thrust_vac_n * thrust_scale; srm_prop_ -= mass_flow(SPEC_SRM_PAIR) * h; }
        if (is_stage1_burning) { thrust += SPEC_STAGE1.thrust_vac_n * thrust_scale; stage1_prop_ -= mass_flow(SPEC_STAGE1) * h; }
        if (is_stage2_burning) { thrust += SPEC_STAGE2.thrust_vac_n; stage2_prop_ -= mass_flow(SPEC_STAGE2) * h; }
        if (is_centaur_burning) { thrust += SPEC_CENTAUR.thrust_vac_n; centaur_prop_ -= mass_flow(SPEC_CENTAUR) * h; }
        if (is_kick_burning) { thrust += SPEC_KICK.thrust_vac_n; kick_prop_ -= mass_flow(SPEC_KICK) * h; }
        _check_burnouts();
        double mass = _total_mass();
        double accel_thrust = thrust / mass;
        dvec3 dir = _thrust_direction(accel_thrust);
        dvec3 gravity = -EARTH_GM * position / std::pow(glm::length(position), 3.0);
        dvec3 v_rel = velocity - earth_surface_velocity(position);       // wind speed felt by the rocket
        dvec3 drag = -0.5 * rho * _drag_area() / mass * glm::length(v_rel) * v_rel;   // zero in space (rho = 0)
        velocity += (dir * accel_thrust + gravity + drag) * h;
        position += velocity * h;
        last_air_density = rho;
        last_dynamic_pressure = 0.5 * rho * glm::dot(v_rel, v_rel);
        dvec3 axis = orientation * dvec3(0, 1, 0);
        orientation = glm::normalize(rotation_between(axis, dir) * orientation);   // point along the push, no roll
    }

    void _check_burnouts() {
        double t = mission_time;
        if (is_srm_burning && srm_prop_ <= 0) { srm_prop_ = 0; is_srm_burning = false; t_srm_burnout_ = t; }
        if (is_stage1_burning && stage1_prop_ <= 0) { stage1_prop_ = 0; is_stage1_burning = false; t_stage1_burnout_ = t; }
        if (is_stage2_burning && stage2_prop_ <= 0) { stage2_prop_ = 0; is_stage2_burning = false; t_stage2_burnout_ = t; }
        if (is_centaur_burning && centaur_prop_ <= 0) { centaur_prop_ = 0; is_centaur_burning = false; t_centaur_end_ = t; }
        if (is_kick_burning && kick_prop_ <= 0) { kick_prop_ = 0; is_kick_burning = false; t_kick_burnout_ = t; }
    }
};

// Description: pull on a free-flying piece (a dropped stage): gravity plus air drag (zero in space).
// In: world position, world velocity, (drag area / mass). Out: acceleration in m/s^2.
inline dvec3 free_flight_acceleration(const dvec3& position, const dvec3& velocity, double drag_area_over_mass) {
    double r = glm::length(position);
    dvec3 gravity = -EARTH_GM * position / (r * r * r);
    dvec3 v_rel = velocity - earth_surface_velocity(position);
    double rho = air_density(r - EARTH_RADIUS_M);
    return gravity - 0.5 * rho * drag_area_over_mass * glm::length(v_rel) * v_rel;
}
