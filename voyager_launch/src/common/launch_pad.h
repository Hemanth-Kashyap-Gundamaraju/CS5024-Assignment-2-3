// launch_pad.h - LaunchPad: ground, concrete pad, launch table, umbilical tower, masts, tanks, service tower.
// Pad-local axes: +X east, +Y up, +Z south. Origin = ground under the rocket.
// Shapes are simple (boxes, cylinders, spheres) - a stand-in for LC-41, sized roughly like the real one.
#pragma once
#include "earth.h"
#include "procedural_textures.h"

constexpr double GROUND_CAP_ANGLE_RAD = 0.021;   // ground patch radius ~ 134 km
constexpr float GROUND_TILE_METRES = 12.0f;      // one grass texture repeat
constexpr float PAD_SLAB_SIZE = 70.0f;
constexpr float PAD_SLAB_HEIGHT = 0.6f;
constexpr float TABLE_SIZE = 14.0f;
constexpr float TOWER_X = 13.0f;                 // umbilical tower distance east of the rocket
constexpr float TOWER_HEIGHT = 62.0f;
const glm::vec3 COLOR_CONCRETE(0.75f), COLOR_TOWER(0.80f, 0.25f, 0.18f), COLOR_STEEL(0.55f, 0.57f, 0.60f);

class LaunchPad {
public:
    // Description: build shapes and textures.
    void init() {
        ground_cap_ = make_ground_cap(GROUND_CAP_ANGLE_RAD, 70, 96, GROUND_TILE_METRES);
        unit_box_ = make_box(1, 1, 1);
        unit_cylinder_ = make_cylinder(1, 1, 1, 24);
        unit_sphere_ = make_sphere(24, 48);
        ground_texture_ = texture_or_fallback("ground.jpg", ground_pixel, 512, 512, true);
        concrete_texture_ = texture_or_fallback("concrete.jpg", concrete_pixel, 512, 512, true);
    }

    // Description: draw the whole site. In: renderer (local pass already begun), frame, pad pose in the world.
    void draw(SceneRenderer& r, const FrameContext& ctx, const SitePose& pose) {
        WorldTransform t; t.position = pose.position; t.rotation = pose.basis;
        _draw_ground(r, ctx, t);
        _draw_slab_and_table(r, ctx, t);
        _draw_umbilical_tower(r, ctx, t);
        _draw_extras(r, ctx, t);
    }

private:
    Mesh ground_cap_, unit_box_, unit_cylinder_, unit_sphere_;
    GLuint ground_texture_ = 0, concrete_texture_ = 0;

    // --- small placement helpers (everything goes through these) ---
    void _box(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& t, glm::vec3 center, glm::vec3 size, const Material& m) {
        glm::mat4 local = glm::scale(glm::translate(glm::mat4(1.0f), center), size);
        r.draw_mesh(unit_box_, make_relative_model(t, local, ctx.camera_position), m);
    }
    void _cylinder(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& t, glm::vec3 base_center, float radius, float height, const Material& m) {
        glm::mat4 local = glm::scale(glm::translate(glm::mat4(1.0f), base_center), glm::vec3(radius, height, radius));
        r.draw_mesh(unit_cylinder_, make_relative_model(t, local, ctx.camera_position), m);
    }
    void _sphere(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& t, glm::vec3 center, float radius, const Material& m) {
        glm::mat4 local = glm::scale(glm::translate(glm::mat4(1.0f), center), glm::vec3(radius));
        r.draw_mesh(unit_sphere_, make_relative_model(t, local, ctx.camera_position), m);
    }

    void _draw_ground(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& t) {
        Material m = make_material(glm::vec3(1.0f), 0.0f, 0.3f);
        m.texture = ground_texture_;
        m.fade_radius = (float)(EARTH_RADIUS_M * GROUND_CAP_ANGLE_RAD);
        r.draw_mesh(ground_cap_, make_relative_model(t, glm::mat4(1.0f), ctx.camera_position), m);
    }
    void _draw_slab_and_table(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& t) {
        Material concrete = make_material(COLOR_CONCRETE, 0.1f, 0.3f);
        concrete.texture = concrete_texture_; concrete.uv_scale = glm::vec2(2.0f);
        _box(r, ctx, t, glm::vec3(0, PAD_SLAB_HEIGHT / 2, 0), glm::vec3(PAD_SLAB_SIZE, PAD_SLAB_HEIGHT, PAD_SLAB_SIZE), concrete);
        // flame trench: dark strip running north from the table
        _box(r, ctx, t, glm::vec3(0, PAD_SLAB_HEIGHT + 0.02f, -26), glm::vec3(12, 0.05f, 36), make_material(glm::vec3(0.08f), 0.0f));
        // launch table: 4 legs and a deck frame with an opening, so the exhaust can pass through (rocket stands 3 m up)
        float deck_y = (float)ROCKET_BASE_HEIGHT_M - 0.3f, half = TABLE_SIZE / 2;
        Material steel = make_material(COLOR_STEEL, 0.3f);
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sz = -1; sz <= 1; sz += 2)
                _box(r, ctx, t, glm::vec3(sx * (half - 1.0f), PAD_SLAB_HEIGHT + (deck_y - PAD_SLAB_HEIGHT) / 2, sz * (half - 1.0f)), glm::vec3(2.0f, deck_y - PAD_SLAB_HEIGHT, 2.0f), steel);
        float open_x = 5.5f, open_z = 3.0f;   // opening half-sizes (SRM nozzles are at +-3.1 m east/west)
        _box(r, ctx, t, glm::vec3(0, deck_y, (open_z + half) / 2), glm::vec3(TABLE_SIZE, 0.6f, half - open_z), steel);
        _box(r, ctx, t, glm::vec3(0, deck_y, -(open_z + half) / 2), glm::vec3(TABLE_SIZE, 0.6f, half - open_z), steel);
        _box(r, ctx, t, glm::vec3((open_x + half) / 2, deck_y, 0), glm::vec3(half - open_x, 0.6f, 2 * open_z), steel);
        _box(r, ctx, t, glm::vec3(-(open_x + half) / 2, deck_y, 0), glm::vec3(half - open_x, 0.6f, 2 * open_z), steel);
    }
    void _draw_umbilical_tower(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& t) {
        Material steel = make_material(COLOR_TOWER, 0.3f), white = make_material(glm::vec3(0.9f), 0.2f);
        float half = 2.2f;
        for (int sx = -1; sx <= 1; sx += 2)                                  // 4 tall corner columns
            for (int sz = -1; sz <= 1; sz += 2)
                _box(r, ctx, t, glm::vec3(TOWER_X + sx * half, PAD_SLAB_HEIGHT + TOWER_HEIGHT / 2, sz * half), glm::vec3(0.7f, TOWER_HEIGHT, 0.7f), steel);
        for (float y = 6.0f; y < TOWER_HEIGHT; y += 6.0f)                    // floors every 6 m
            _box(r, ctx, t, glm::vec3(TOWER_X, PAD_SLAB_HEIGHT + y, 0), glm::vec3(2 * half + 0.7f, 0.35f, 2 * half + 0.7f), white);
        for (float y : {14.0f, 27.0f, 41.0f})                                // swing arms reaching the rocket
            _box(r, ctx, t, glm::vec3(TOWER_X / 2 + 0.6f, PAD_SLAB_HEIGHT + y, 0), glm::vec3(TOWER_X - 3.0f, 0.7f, 1.2f), white);
    }
    void _draw_extras(SceneRenderer& r, const FrameContext& ctx, const WorldTransform& t) {
        Material steel = make_material(COLOR_STEEL, 0.4f), white = make_material(glm::vec3(0.92f), 0.4f);
        for (int sx = -1; sx <= 1; sx += 2)                                  // 4 lightning masts
            for (int sz = -1; sz <= 1; sz += 2)
                _cylinder(r, ctx, t, glm::vec3(sx * 75.0f, 0, sz * 75.0f), 0.35f, 80.0f, steel);
        // Two vertical cylindrical propellant tanks with rounded end caps.
        // These replace the old spherical placeholders and read more clearly
        // as storage tanks when viewed from the launch-site camera.
        auto tank = [&](float x, float z) {
            const float radius = 4.2f;
            const float lower_cap_y = 4.5f;
            const float upper_cap_y = 14.5f;
            Material tank_paint = make_material(glm::vec3(0.88f, 0.90f, 0.92f), 0.45f, 0.28f);
            _cylinder(r, ctx, t, glm::vec3(x, lower_cap_y, z), radius, upper_cap_y - lower_cap_y, tank_paint);
            _sphere(r, ctx, t, glm::vec3(x, lower_cap_y, z), radius, tank_paint);
            _sphere(r, ctx, t, glm::vec3(x, upper_cap_y, z), radius, tank_paint);
            // Dark support bands make the tank silhouette easier to read.
            Material band = make_material(glm::vec3(0.28f, 0.31f, 0.34f), 0.25f, 0.25f);
            _cylinder(r, ctx, t, glm::vec3(x, 7.0f, z), radius + 0.08f, 0.35f, band);
            _cylinder(r, ctx, t, glm::vec3(x, 12.0f, z), radius + 0.08f, 0.35f, band);
        };
        tank(-95.0f, -60.0f);
        tank(-95.0f, -85.0f);
        _box(r, ctx, t, glm::vec3(110, 3, 45), glm::vec3(30, 6, 20), make_material(glm::vec3(0.6f, 0.58f, 0.52f), 0.1f));   // bunker
        _box(r, ctx, t, glm::vec3(0, 41, 230), glm::vec3(22, 82, 24), make_material(glm::vec3(0.85f, 0.45f, 0.15f), 0.2f)); // service tower, rolled back
    }
};

