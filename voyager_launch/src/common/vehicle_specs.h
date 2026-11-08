// vehicle_specs.h - numbers describing the Titan IIIE / Centaur D-1T / Voyager stack and the flight timeline.
// Pure data (no OpenGL). These are APPROXIMATE, rounded values, tuned so the simple simulation reaches orbit.
// Edit them freely; check real figures in NASA / Wikipedia / the Titan IIIE fact sheets for the report.
#pragma once

constexpr double G0 = 9.80665;

// One rocket stage: empty mass, thrust (vacuum), engine efficiency (Isp), burn time.
struct StageSpec {
    double dry_mass_kg;
    double thrust_vac_n;
    double isp_vac_s;
    double burn_time_s;     // fuel = thrust / (isp * g0) * burn_time
};
// Description: fuel mass of a stage. In: spec. Out: kg.
inline double propellant_mass(const StageSpec& s) { return s.thrust_vac_n / (s.isp_vac_s * G0) * s.burn_time_s; }
// Description: fuel burned per second at full thrust. In: spec. Out: kg/s.
inline double mass_flow(const StageSpec& s) { return s.thrust_vac_n / (s.isp_vac_s * G0); }

inline const StageSpec SPEC_SRM_PAIR = {30000.0, 10.0e6, 265.0, 115.0};   // two big side boosters (solid)
inline const StageSpec SPEC_STAGE1   = {8500.0,  2.35e6, 302.0, 148.0};   // Titan core stage 1
inline const StageSpec SPEC_STAGE2   = {4500.0,  0.46e6, 316.0, 205.0};   // Titan core stage 2
inline const StageSpec SPEC_CENTAUR  = {2800.0,  0.133e6, 444.0, 440.0};  // Centaur D-1T (burn time = total, split in two burns)
inline const StageSpec SPEC_KICK     = {130.0,   68.0e3, 290.0, 42.0};    // Star-37E kick motor on Voyager
constexpr double MASS_SHROUD_KG = 1800.0;
constexpr double MASS_VOYAGER_KG = 900.0;        // spacecraft + adapter

// Drag area (drag coefficient x frontal area, m^2) of the stack in different states.
constexpr double DRAG_AREA_CORE_M2 = 3.3;
constexpr double DRAG_AREA_SRM_PAIR_M2 = 6.6;
constexpr double DRAG_AREA_SHROUD_M2 = 7.0;      // wider shroud replaces the core's area while attached
constexpr double SEA_LEVEL_THRUST_LOSS = 0.07;   // thrust is ~7% lower in thick air

// ---- flight timeline (seconds after liftoff, "T+") ----
constexpr double T_PITCH_START_S = 8.0;          // fly straight up until here
// Pitch program: angle from straight-up (degrees) at each time (seconds). The rocket leans over slowly
// until the core stage is empty, then guidance takes over. Edit these to change how steeply it climbs.
struct PitchPoint { double time_s; double pitch_deg; };
inline const PitchPoint PITCH_TABLE[] = {
    {8.0, 0.0}, {20.0, 6.0}, {40.0, 17.0}, {70.0, 33.0}, {100.0, 50.0}, {130.0, 62.0}, {170.0, 72.0}, {220.0, 80.0}, {260.0, 84.0}};
constexpr double T_STAGE1_IGNITION_S = 105.0;    // core stage lights while boosters are still burning
constexpr double SRM_JETTISON_DELAY_S = 10.0;    // after boosters burn out
constexpr double STAGE_SEPARATION_DELAY_S = 1.5;
constexpr double CENTAUR_IGNITION_DELAY_S = 6.0; // after Stage 2 is dropped
constexpr double SHROUD_JETTISON_ALTITUDE_M = 115000.0;
constexpr double PARKING_ORBIT_ALTITUDE_M = 185000.0;
constexpr double COAST_DURATION_S = 200.0;       // Centaur coasts between its two burns
constexpr double VOYAGER_SEPARATION_DELAY_S = 4.0;
constexpr double KICK_IGNITION_DELAY_S = 8.0;
constexpr double KICK_SEPARATION_DELAY_S = 4.0;
constexpr double BOOM_DEPLOY_START_DELAY_S = 6.0;
constexpr double BOOM_DEPLOY_DURATION_S = 30.0;

// ---- steering numbers for the closed-loop (upper stage) guidance ----
constexpr double GUIDANCE_K_ALTITUDE = 2.0e-5;   // push up when too low
constexpr double GUIDANCE_K_VERTICAL_SPEED = 4.0e-3;  // push down when climbing too fast
constexpr double GUIDANCE_MAX_PITCH_RATIO = 0.7;
