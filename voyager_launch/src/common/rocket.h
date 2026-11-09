// rocket.h - Rocket: the Titan IIIE / Centaur launch vehicle with the Voyager spacecraft, built from code shapes.
// Rocket-local axes: +Y = along the rocket, origin = centre of the bottom of core stage 1. Units: metres.
// Dimensions are approximate (overall height ~48.7 m). Not made in Blender - everything is generated here.
#pragma once
#include <array>
#include <functional>
#include "flight_sim.h"
#include "renderer.h"

// ---- sizes (metres) ----
constexpr float CORE_RADIUS = 1.525f, SRM_RADIUS = 1.525f, SRM_OFFSET_X = 3.1f;
constexpr float Y_STAGE1_TOP = 21.5f, Y_INTERSTAGE_TOP = 23.5f, Y_STAGE2_TOP = 31.0f, Y_CENTAUR_TOP = 40.3f;
constexpr float Y_KICK_BOTTOM = 40.9f, Y_KICK_TOP = 42.6f, Y_VOYAGER_BASE = 42.6f;
constexpr float SRM_BODY_TOP = 25.5f, SRM_NOSE_TOP = 29.0f, SRM_NOZZLE_DEPTH = 2.0f;
constexpr float SHROUD_RADIUS = 2.14f;
constexpr int ROCKET_SLICES = 36;
// ---- colours ----
const glm::vec3 COLOR_WHITE(0.93f, 0.93f, 0.91f), COLOR_BLACK(0.05f), COLOR_SILVER(0.78f, 0.80f, 0.83f),
    COLOR_GOLD(0.85f, 0.66f, 0.22f), COLOR_DARK_METAL(0.25f, 0.26f, 0.28f), COLOR_NOZZLE(0.18f, 0.17f, 0.17f);

enum PieceId { PIECE_SRM_LEFT, PIECE_SRM_RIGHT, PIECE_STAGE1, PIECE_STAGE2, PIECE_CENTAUR, PIECE_SHROUD_LEFT,
               PIECE_SHROUD_RIGHT, PIECE_KICK_MOTOR, PIECE_COUNT };

struct Part { Mesh mesh; glm::mat4 local{1.0f}; Material material; };
struct FlameSpec { glm::vec3 position; float radius; float length; };

// One separable piece of the rocket. While attached it follows the vehicle; after release it flies alone.
struct Piece {
    std::vector<Part> parts;
    std::vector<FlameSpec> flames;
    glm::vec3 pivot{0.0f};            // piece centre in rocket space (where it spins from)
    double drag_area_m2 = 10.0, mass_kg = 5000.0;
    bool is_attached = true, is_visible = true, is_burning = false;
    dvec3 position{0.0}, velocity{0.0}, spin_axis{0, 0, 1};   // after release (world)
    glm::dquat orientation{1.0, 0.0, 0.0, 0.0};
    double spin_rate = 0.0;
};

// Acceleration felt by a free-flying piece: (position, velocity, drag_area/mass) -> m/s^2.
using AccelerationFunction = std::function<dvec3(const dvec3&, const dvec3&, double)>;

class Rocket {
public:
    double boom_deploy = 0.0;   // Voyager booms: 0 stowed .. 1 out
    bool show_voyager_only_when_open = false;

    // Description: build all meshes. In: mission index (stripe colour).
    void build(int mission_index) {
        accent_ = MISSIONS[mission_index].accent_color;
        for (auto& p : pieces_) p = Piece();
        cone_ = make_cylinder(1.0f, 0.0f, 1.0f, 24, false);
        unit_cylinder_ = make_cylinder(1, 1, 1, 12);
        unit_box_ = make_box(1, 1, 1);
        unit_sphere_ = make_sphere(12, 24);
        _build_srm(PIECE_SRM_LEFT, -1.0f);
        _build_srm(PIECE_SRM_RIGHT, 1.0f);
        _build_stage1();
        _build_stage2();
        _build_centaur();
        _build_shroud(PIECE_SHROUD_LEFT, true);
        _build_shroud(PIECE_SHROUD_RIGHT, false);
        _build_kick_motor();
        _build_voyager_static();
        reset();
    }

    // Description: attach everything again (back on the pad).
    void reset() {
        for (auto& p : pieces_) { p.is_attached = true; p.is_visible = true; p.is_burning = false; }
        boom_deploy = 0.0;
    }

    Piece& piece(PieceId id) { return pieces_[id]; }
    bool is_attached(PieceId id) const { return pieces_[id].is_attached; }
    void set_burning(PieceId id, bool burning) { if (pieces_[id].is_attached) pieces_[id].is_burning = burning; }

    // Description: let a piece go. It keeps the vehicle's speed plus a small push, and may tumble.
    // In: piece, vehicle pose, vehicle velocity, push direction (rocket space), push speed, spin axis (rocket space), spin rate.
    void detach(PieceId id, const WorldTransform& pose, const dvec3& vehicle_velocity, const glm::vec3& push_dir,
                double push_speed, const glm::vec3& spin_axis, double spin_rate) {
        Piece& p = pieces_[id];
        if (!p.is_attached) return;
        p.is_attached = false; p.is_burning = false;
        p.position = pose.position + pose.rotation * dvec3(p.pivot);
        p.velocity = vehicle_velocity + pose.rotation * (dvec3(glm::normalize(push_dir)) * push_speed);
        p.orientation = glm::dquat(pose.rotation);
        p.spin_axis = pose.rotation * glm::normalize(dvec3(spin_axis));
        p.spin_rate = spin_rate;
    }

    // Description: do what a flight event means for the drawing (drop the right pieces).
    // In: event, vehicle pose, vehicle velocity.
    void apply_event(FlightEventType type, const WorldTransform& pose, const dvec3& vel) {
        glm::vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
        switch (type) {
            case EVENT_SRM_JETTISON:
                detach(PIECE_SRM_LEFT, pose, vel, -X + 0.1f * Z, 9.0, Z, 0.25);
                detach(PIECE_SRM_RIGHT, pose, vel, X - 0.1f * Z, 9.0, Z, -0.25);
                break;
            case EVENT_STAGE1_SEPARATION: detach(PIECE_STAGE1, pose, vel, -Y, 3.5, X, 0.12); break;
            case EVENT_SHROUD_JETTISON:
                detach(PIECE_SHROUD_LEFT, pose, vel, -X, 5.0, Z, 0.7);
                detach(PIECE_SHROUD_RIGHT, pose, vel, X, 5.0, Z, -0.7);
                break;
            case EVENT_STAGE2_SEPARATION: detach(PIECE_STAGE2, pose, vel, -Y, 2.5, X, 0.08); break;
            case EVENT_VOYAGER_SEPARATION: detach(PIECE_CENTAUR, pose, vel, -Y, 2.0, X, 0.05); break;
            case EVENT_KICK_SEPARATION: detach(PIECE_KICK_MOTOR, pose, vel, -Y, 3.0, X, 0.2); break;
            default: break;
        }
    }

    // Description: copy the engine on/off flags from the flight simulation.
    void sync_engines(const FlightSim& sim) {
        set_burning(PIECE_SRM_LEFT, sim.is_srm_burning); set_burning(PIECE_SRM_RIGHT, sim.is_srm_burning);
        set_burning(PIECE_STAGE1, sim.is_stage1_burning); set_burning(PIECE_STAGE2, sim.is_stage2_burning);
        set_burning(PIECE_CENTAUR, sim.is_centaur_burning); set_burning(PIECE_KICK_MOTOR, sim.is_kick_burning);
    }

    // Description: move the released pieces (gravity, drag, spin). In: seconds, acceleration function, ground radius (0 = none).
    void update_detached(double dt, const AccelerationFunction& accel, double kill_radius) {
        int steps = (int)std::ceil(dt / 0.05);
        if (steps < 1) steps = 1;
        double h = dt / steps;
        for (auto& p : pieces_) {
            if (p.is_attached || !p.is_visible) continue;
            for (int i = 0; i < steps; i++) {
                p.velocity += accel(p.position, p.velocity, p.drag_area_m2 / p.mass_kg) * h;
                p.position += p.velocity * h;
                p.orientation = glm::normalize(glm::angleAxis(p.spin_rate * h, p.spin_axis) * p.orientation);
            }
            if (kill_radius > 0.0 && glm::length(p.position) < kill_radius) p.is_visible = false;
        }
    }

    // Description: draw the whole vehicle (attached pieces, released pieces, Voyager, flames).
    // In: renderer (local pass begun), frame, pose of the rocket base, camera altitude for flame size.
    void draw(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& pose, double flame_altitude,
              bool draw_flames = true, float max_flame_length = 0.0f) {
        for (int i = 0; i < PIECE_COUNT; i++) {
            Piece& p = pieces_[i];
            if (!p.is_visible) continue;
            WorldTransform wt = _piece_transform(p, pose);
            for (auto& part : p.parts)
                r.draw_mesh(part.mesh, make_relative_model(wt, glm::translate(glm::mat4(1.0f), -p.pivot) * part.local, ctx.camera_position), part.material);
        }
        _draw_voyager(r, ctx, pose);
        if (draw_flames) _draw_flames(r, ctx, pose, flame_altitude, max_flame_length);
    }

    // Description: world position of a point given in rocket space.
    static dvec3 point_in_world(const WorldTransform& pose, const glm::vec3& local) { return pose.position + pose.rotation * dvec3(local); }

private:
    std::array<Piece, PIECE_COUNT> pieces_;
    Mesh cone_, unit_cylinder_, unit_box_, unit_sphere_;
    std::vector<Part> voyager_static_;
    glm::vec3 accent_{1, 0, 0};

    // ---- small helpers ----
    static glm::mat4 _move(float x, float y, float z) { return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z)); }
    void _add(PieceId id, Mesh&& mesh, const glm::mat4& local, const Material& mat) { pieces_[id].parts.push_back({std::move(mesh), local, mat}); }
    static Material _mat(const glm::vec3& c, float spec = 0.3f, float amb = 0.25f) { return make_material(c, spec, amb); }
    WorldTransform _piece_transform(const Piece& p, const WorldTransform& pose) const {
        WorldTransform wt;
        if (p.is_attached) { wt.position = pose.position + pose.rotation * dvec3(p.pivot); wt.rotation = pose.rotation; }
        else { wt.position = p.position; wt.rotation = glm::mat3_cast(p.orientation); }
        return wt;
    }
    // Description: matrix that stretches a unit cylinder (y 0..1, radius 1) from point a to point b.
    static glm::mat4 _strut(const glm::vec3& a, const glm::vec3& b, float radius) {
        glm::vec3 d = b - a;
        float len = glm::max(glm::length(d), 1e-4f);
        glm::dquat q = rotation_between(dvec3(0, 1, 0), dvec3(d / len));
        return glm::translate(glm::mat4(1.0f), a) * glm::mat4(glm::mat3_cast(glm::quat(q))) * glm::scale(glm::mat4(1.0f), glm::vec3(radius, len, radius));
    }

    // ---- building the pieces ----
    void _build_srm(PieceId id, float side) {
        Piece& p = pieces_[id];
        p.pivot = glm::vec3(side * SRM_OFFSET_X, 13.5f, 0); p.drag_area_m2 = 30; p.mass_kg = 15000;
        float x = side * SRM_OFFSET_X;
        glm::mat4 at = _move(x, 0, 0);
        _add(id, make_cylinder(SRM_RADIUS, SRM_RADIUS, SRM_BODY_TOP, ROCKET_SLICES), at, _mat(COLOR_WHITE, 0.3f));
        _add(id, make_revolve({{SRM_RADIUS, SRM_BODY_TOP}, {1.25f, SRM_BODY_TOP + 1.6f}, {0.6f, SRM_NOSE_TOP - 0.5f}, {0.0f, SRM_NOSE_TOP}}, ROCKET_SLICES), at, _mat(COLOR_WHITE, 0.3f));
        _add(id, make_cylinder(SRM_RADIUS + 0.02f, SRM_RADIUS + 0.02f, 0.7f, ROCKET_SLICES, false), at * _move(0, 23.8f, 0), _mat(accent_, 0.2f));   // mission stripe
        for (float y : {5.0f, 10.5f, 16.0f}) _add(id, make_cylinder(SRM_RADIUS + 0.02f, SRM_RADIUS + 0.02f, 0.18f, ROCKET_SLICES, false), at * _move(0, y, 0), _mat(COLOR_BLACK, 0.1f));  // segment joints
        _add(id, make_cylinder(1.35f, 0.65f, SRM_NOZZLE_DEPTH, 28), at * _move(0, -SRM_NOZZLE_DEPTH, 0), _mat(COLOR_NOZZLE, 0.2f));
        p.flames.push_back({glm::vec3(x, -SRM_NOZZLE_DEPTH, 0), 1.25f, 30.0f});
    }
    void _build_stage1() {
        Piece& p = pieces_[PIECE_STAGE1];
        p.pivot = glm::vec3(0, 11.0f, 0); p.drag_area_m2 = 20; p.mass_kg = 8500;
        _add(PIECE_STAGE1, make_cylinder(CORE_RADIUS, CORE_RADIUS, Y_STAGE1_TOP, ROCKET_SLICES), glm::mat4(1.0f), _mat(COLOR_WHITE, 0.3f));
        for (float y : {3.0f, 9.0f, 15.0f}) _add(PIECE_STAGE1, make_cylinder(CORE_RADIUS + 0.02f, CORE_RADIUS + 0.02f, 0.8f, ROCKET_SLICES, false), _move(0, y, 0), _mat(COLOR_BLACK, 0.1f));
        _add(PIECE_STAGE1, make_cylinder(CORE_RADIUS - 0.1f, CORE_RADIUS - 0.1f, Y_INTERSTAGE_TOP - Y_STAGE1_TOP, 24, false), _move(0, Y_STAGE1_TOP, 0), _mat(COLOR_DARK_METAL, 0.4f));  // open interstage
        for (float sx : {-0.6f, 0.6f}) {
            _add(PIECE_STAGE1, make_cylinder(0.55f, 0.25f, 1.6f, 20), _move(sx, -1.6f, 0), _mat(COLOR_NOZZLE, 0.3f));
            p.flames.push_back({glm::vec3(sx, -1.6f, 0), 0.55f, 14.0f});
        }
    }
    void _build_stage2() {
        Piece& p = pieces_[PIECE_STAGE2];
        p.pivot = glm::vec3(0, 27.0f, 0); p.drag_area_m2 = 12; p.mass_kg = 4500;
        _add(PIECE_STAGE2, make_cylinder(CORE_RADIUS, CORE_RADIUS, Y_STAGE2_TOP - Y_INTERSTAGE_TOP, ROCKET_SLICES), _move(0, Y_INTERSTAGE_TOP, 0), _mat(COLOR_WHITE, 0.3f));
        _add(PIECE_STAGE2, make_cylinder(CORE_RADIUS + 0.02f, CORE_RADIUS + 0.02f, 0.6f, ROCKET_SLICES, false), _move(0, 27.5f, 0), _mat(COLOR_BLACK, 0.1f));
        _add(PIECE_STAGE2, make_cylinder(0.9f, 0.45f, 1.9f, 20), _move(0, Y_INTERSTAGE_TOP - 1.9f, 0), _mat(COLOR_NOZZLE, 0.3f));
        p.flames.push_back({glm::vec3(0, Y_INTERSTAGE_TOP - 1.9f, 0), 0.9f, 14.0f});
    }
    void _build_centaur() {
        Piece& p = pieces_[PIECE_CENTAUR];
        p.pivot = glm::vec3(0, 36.0f, 0); p.drag_area_m2 = 12; p.mass_kg = 2800;
        _add(PIECE_CENTAUR, make_revolve({{0, Y_STAGE2_TOP}, {CORE_RADIUS, Y_STAGE2_TOP}, {CORE_RADIUS, Y_CENTAUR_TOP - 0.3f}, {CORE_RADIUS - 0.2f, Y_CENTAUR_TOP}, {0, Y_CENTAUR_TOP}}, ROCKET_SLICES), glm::mat4(1.0f), _mat(COLOR_SILVER, 0.6f));
        _add(PIECE_CENTAUR, make_cylinder(CORE_RADIUS - 0.2f, 0.7f, Y_KICK_BOTTOM - Y_CENTAUR_TOP, 24), _move(0, Y_CENTAUR_TOP, 0), _mat(COLOR_DARK_METAL, 0.4f));   // adapter cone
        for (float sx : {-0.4f, 0.4f}) {
            _add(PIECE_CENTAUR, make_cylinder(0.35f, 0.15f, 0.8f, 16), _move(sx, Y_STAGE2_TOP - 0.8f, 0), _mat(COLOR_NOZZLE, 0.3f));
            p.flames.push_back({glm::vec3(sx, Y_STAGE2_TOP - 0.8f, 0), 0.3f, 4.0f});
        }
    }
    void _build_shroud(PieceId id, bool left) {
        Piece& p = pieces_[id];
        p.pivot = glm::vec3(left ? -1.0f : 1.0f, 40.0f, 0); p.drag_area_m2 = 10; p.mass_kg = 900;
        float a0 = left ? 1.5707963f : -1.5707963f, a1 = a0 + 3.1415927f;
        std::vector<glm::vec2> outline = {{1.55f, 31.0f}, {SHROUD_RADIUS, 32.6f}, {SHROUD_RADIUS, 42.5f}, {2.10f, 43.5f},
                                          {1.95f, 44.8f}, {1.60f, 46.2f}, {1.00f, 47.6f}, {0.0f, 48.7f}};
        _add(id, make_revolve(outline, ROCKET_SLICES / 2, a0, a1), glm::mat4(1.0f), _mat(COLOR_WHITE, 0.35f));
        _add(id, make_revolve({{SHROUD_RADIUS + 0.02f, 36.2f}, {SHROUD_RADIUS + 0.02f, 37.4f}}, ROCKET_SLICES / 2, a0, a1), glm::mat4(1.0f), _mat(accent_, 0.2f));   // mission stripe
    }
    void _build_kick_motor() {
        Piece& p = pieces_[PIECE_KICK_MOTOR];
        p.pivot = glm::vec3(0, 41.7f, 0); p.drag_area_m2 = 1.5; p.mass_kg = 130;
        _add(PIECE_KICK_MOTOR, make_revolve({{0.12f, 40.9f}, {0.40f, 41.05f}, {0.47f, 41.5f}, {0.47f, 41.9f}, {0.30f, 42.4f}, {0.0f, Y_KICK_TOP}}, 24), glm::mat4(1.0f), _mat(COLOR_GOLD, 0.5f));
        _add(PIECE_KICK_MOTOR, make_cylinder(0.40f, 0.12f, 0.5f, 16), _move(0, 40.4f, 0), _mat(COLOR_NOZZLE, 0.3f));
        p.flames.push_back({glm::vec3(0, 40.4f, 0), 0.4f, 5.0f});
    }
    // Voyager parts that never move (bus, dish, sub-reflector, struts). Local origin = top of kick motor.
    void _build_voyager_static() {
        voyager_static_.clear();
        auto add = [&](Mesh&& m, const glm::mat4& l, const Material& mat) { voyager_static_.push_back({std::move(m), l, mat}); };
        add(make_cylinder(0.89f, 0.89f, 0.47f, 10), glm::mat4(1.0f), _mat(COLOR_GOLD, 0.5f));                    // 10-sided bus
        std::vector<glm::vec2> dish;                                                                              // parabolic dish
        const float focal = 0.9f;
        for (int i = 0; i <= 10; i++) { float r = 1.83f * i / 10; dish.push_back({r, 0.62f + r * r / (4 * focal)}); }
        add(make_revolve(dish, 36), glm::mat4(1.0f), _mat(COLOR_WHITE, 0.4f, 0.3f));
        add(make_sphere(10, 20), _move(0, 1.52f, 0) * glm::scale(glm::mat4(1.0f), glm::vec3(0.2f)), _mat(COLOR_SILVER, 0.5f));
        for (int k = 0; k < 4; k++) {
            float a = k * 1.5707963f + 0.785f;
            add(make_cylinder(1, 1, 1, 6, false), _strut(glm::vec3(1.8f * std::cos(a), 1.55f, 1.8f * std::sin(a)), glm::vec3(0, 1.52f, 0), 0.025f), _mat(COLOR_DARK_METAL));
        }
    }

    // ---- drawing helpers ----
    void _draw_voyager(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& pose) {
        glm::mat4 base = _move(0, Y_VOYAGER_BASE, 0);
        auto draw = [&](const Mesh& m, const glm::mat4& l, const Material& mat) { r.draw_mesh(m, make_relative_model(pose, base * l, ctx.camera_position), mat); };
        for (auto& part : voyager_static_) draw(part.mesh, part.local, part.material);
        float d = (float)glm::clamp(boom_deploy, 0.0, 1.0);
        Material steel = _mat(COLOR_SILVER, 0.5f), dark = _mat(COLOR_DARK_METAL, 0.3f);
        // boom: from hinge, direction and length blend from "folded" to "out"
        auto boom = [&](glm::vec3 hinge, glm::vec3 d0, glm::vec3 d1, float l0, float l1, float radius, glm::vec3& tip_out, glm::vec3& dir_out) {
            dir_out = glm::normalize(glm::mix(glm::normalize(d0), glm::normalize(d1), d));
            float len = glm::mix(l0, l1, d);
            tip_out = hinge + dir_out * len;
            draw(unit_cylinder_, _strut(hinge, tip_out, radius), steel);
        };
        glm::vec3 tip, dir;
        // RTG boom (3 power generators)
        glm::vec3 h1(-0.89f, 0.2f, 0);
        boom(h1, glm::vec3(-0.3f, -1, 0), glm::vec3(-1, 0, 0), 1.5f, 3.5f, 0.05f, tip, dir);
        for (float t : {0.55f, 0.72f, 0.89f}) {
            glm::vec3 a = h1 + (tip - h1) * t;
            draw(unit_cylinder_, _strut(a, a + dir * 0.45f, 0.2f), dark);
        }
        // science boom with scan platform
        glm::vec3 h2(0.89f, 0.2f, 0);
        boom(h2, glm::vec3(0.3f, -1, 0), glm::vec3(1, 0, 0), 1.0f, 2.3f, 0.05f, tip, dir);
        draw(unit_box_, _move(tip.x, tip.y, tip.z) * glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.5f, 0.5f)), _mat(COLOR_GOLD, 0.4f));
        // magnetometer boom (13 m when out)
        glm::vec3 h3(0, 0.2f, 0.89f);
        boom(h3, glm::vec3(0, -1, 0.2f), glm::vec3(0, -0.1f, 1), 0.5f, 13.0f, 0.04f, tip, dir);
        for (float t : {0.5f, 1.0f}) {
            glm::vec3 a = h3 + (tip - h3) * t;
            draw(unit_sphere_, _move(a.x, a.y, a.z) * glm::scale(glm::mat4(1.0f), glm::vec3(0.12f)), dark);
        }
    }
    void _draw_flames(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& pose, double altitude, float max_length) {
        float s = (float)glm::clamp(altitude / 80000.0, 0.0, 1.0);    // the plume balloons out in thin air
        float len_k = 1.0f + 1.5f * s, rad_k = 1.0f + 1.2f * s;
        glm::mat4 flip = glm::rotate(glm::mat4(1.0f), 3.1415927f, glm::vec3(1, 0, 0));   // cone tip points down
        for (auto& p : pieces_) {
            if (!p.is_burning || !p.is_visible) continue;
            WorldTransform wt = _piece_transform(p, pose);
            for (auto& f : p.flames) {
                // A short plume is used close to the pad so the exhaust does not
                // extend through the ground or launch table. A zero limit keeps
                // the original altitude-scaled plume length.
                float plume_length = f.length * len_k;
                if (max_length > 0.0f) plume_length = glm::min(plume_length, max_length);
                glm::mat4 local = glm::translate(glm::mat4(1.0f), -p.pivot) * glm::translate(glm::mat4(1.0f), f.position) * flip
                                  * glm::scale(glm::mat4(1.0f), glm::vec3(f.radius * rad_k, plume_length, f.radius * rad_k));
                r.draw_flame(cone_, make_relative_model(wt, local, ctx.camera_position), 1.2f);
            }
        }
    }
};

