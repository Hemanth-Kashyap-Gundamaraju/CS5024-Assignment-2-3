// a2-scene: the whole Voyager launch - Earth, atmosphere, Moon, Cape Canaveral pad, Titan IIIE/Centaur + Voyager.
// Keys: SPACE start/pause | M switch Voyager 1 <-> 2 (restarts) | R restart
//       1 Earth camera | 2 launch-site camera | 3 vehicle camera (follows the rocket)
//       Earth camera: arrows = X/Y, [ ] = Z, mouse drag = look, F = look at Earth
//       Vehicle camera: [ ] zoom, mouse drag = look around the rocket
//       L cycle lighting preset | 0 restore mission sunlight
//       . / , faster / slower time | W auto time-speed on/off
// Test flags: --start SECONDS (jump to T+SECONDS) --camera N --shot file.ppm
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include "earth.h"
#include "launch_pad.h"
#include "rocket.h"
#include <array>
#include <cctype>

// ---------- globals ----------
enum CameraMode { CAMERA_EARTH = 1, CAMERA_SITE = 2, CAMERA_VEHICLE = 3 };
constexpr double TRAIL_SAMPLE_EVERY_S = 2.0;
constexpr size_t TRAIL_MAX_POINTS = 4000;
constexpr double MAX_WARP = 200.0, MIN_WARP = 1.0;
constexpr double FOLLOW_CAMERA_DISTANCE_M = 70.0;
const glm::dvec3 SITE_CAMERA_OFFSET(80.0, 7.0, 110.0);

FlightSim g_sim;
Rocket g_rocket;
FlyCamera g_earth_camera;
CameraMode g_camera_mode = CAMERA_EARTH;
bool g_running = false, g_auto_warp = true;
int g_lighting_mode = 0; // 0=mission Sun, 1=warm side light, 2=cool rim light
double g_manual_warp = 1.0, g_vehicle_zoom = 1.0, g_vehicle_yaw = 0.0, g_vehicle_pitch = 0.0;
std::vector<dvec3> g_trail;
double g_last_trail_time = -1e9;
std::string g_last_event = "Countdown";

 // ---------- ON-SCREEN MISSION HUD ----------

std::array<unsigned char, 7> hud_glyph(char c) {
    switch (c) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};

    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};

    case ':': return {0,4,4,0,4,4,0};
    case '.': return {0,0,0,0,0,6,6};
    case '-': return {0,0,0,31,0,0,0};
    case '/': return {1,2,2,4,8,8,16};
    case '+': return {0,4,4,31,4,4,0};
    default:  return {0,0,0,0,0,0,0};
    }
}

void hud_draw_rect(SceneRenderer& renderer, PointBuffer& buffer,
                   float x, float y, float w, float h,
                   const glm::vec4& color) {
    buffer.update({
        {x,     y,     0},
        {x + w, y,     0},
        {x + w, y + h, 0},
        {x,     y + h, 0}
    });
    renderer.draw_points(buffer, GL_TRIANGLE_FAN, color, 1.0f);
}

void hud_draw_text(SceneRenderer& renderer, PointBuffer& buffer,
                   float x, float y, const std::string& message,
                   const glm::vec4& color, float pixel = 2.0f) {
    std::vector<glm::vec3> points;
    float cursor_x = x;

    for (char raw : message) {
        char c = static_cast<char>(
            std::toupper(static_cast<unsigned char>(raw)));

        if (c == ' ') {
            cursor_x += 6.0f * pixel;
            continue;
        }

        const auto glyph = hud_glyph(c);

        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph[row] & (1u << (4 - col))) {
                    points.emplace_back(
                        cursor_x + col * pixel,
                        y + (6 - row) * pixel,
                        0.0f
                    );
                }
            }
        }

        cursor_x += 6.0f * pixel;
    }

    buffer.update(points);
    renderer.draw_points(buffer, GL_POINTS, color, pixel);
}

void draw_mission_hud(SceneRenderer& renderer,
                      const AppWindow& win,
                      double warp) {
    static PointBuffer panel_buffer;
    static PointBuffer text_buffer;

    const glm::vec4 background(0.015f, 0.035f, 0.075f, 0.94f);
    const glm::vec4 white(0.88f, 0.94f, 1.0f, 1.0f);
    const glm::vec4 cyan(0.20f, 0.85f, 1.0f, 1.0f);
    const glm::vec4 green(0.35f, 1.0f, 0.60f, 1.0f);
    const glm::vec4 orange(1.0f, 0.65f, 0.25f, 1.0f);

    // Switch from 3D scene coordinates to screen-pixel coordinates.
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderer.set_pass(
        glm::mat4(1.0f),
        glm::ortho(
            0.0f, static_cast<float>(win.width),
            0.0f, static_cast<float>(win.height),
            -1.0f, 1.0f
        ),
        glm::vec3(0, 0, 1)
    );

    const float left = 18.0f;
    const float top = static_cast<float>(win.height) - 18.0f;
    const float panel_w = 440.0f;
    const float panel_h = 250.0f;
    const float panel_y = top - panel_h;

    hud_draw_rect(renderer, panel_buffer,
                  left, panel_y, panel_w, panel_h, background);

    hud_draw_rect(renderer, panel_buffer,
                  left, panel_y + panel_h - 3.0f,
                  panel_w, 3.0f, cyan);

    auto line = [&](float y, const std::string& text,
                const glm::vec4& color,
                float size = 1.8f) {
    hud_draw_text(renderer, text_buffer,
                  left + 14.0f, y, text, color, size);
};

    const double t = g_sim.mission_time;
    const bool countdown = !g_sim.has_lifted_off;

    char time_text[64];
    std::snprintf(
        time_text, sizeof(time_text),
        countdown ? "MISSION TIME: T-%02d:%02d"
                  : "MISSION TIME: T+%02d:%02d",
        static_cast<int>(std::fabs(t)) / 60,
        static_cast<int>(std::fabs(t)) % 60
    );

    line(top - 28.0f, "VOYAGER MISSION CONTROL", cyan, 2.0f);

    line(top - 54.0f,
         std::string("MISSION: ") + MISSIONS[g_mission_index].name,
         white, 1.7f);

    line(top - 76.0f,
         std::string("LAUNCH: ") + MISSIONS[g_mission_index].launch_date,
         white, 1.6f);

    line(top - 101.0f, time_text, green, 1.8f);

    line(top - 123.0f,
         g_running ? "STATUS: SIMULATION RUNNING"
                   : "STATUS: PAUSED - PRESS SPACE",
         g_running ? green : orange, 1.6f);

    char telemetry[100];
    std::snprintf(
        telemetry, sizeof(telemetry),
        "ALTITUDE: %.1f KM", g_sim.altitude() / 1000.0
    );
    line(top - 146.0f, telemetry, white, 1.6f);

    std::snprintf(
        telemetry, sizeof(telemetry),
        "SPEED: %.2f KM/S", g_sim.speed() / 1000.0
    );
    line(top - 168.0f, telemetry, white, 1.6f);

    std::snprintf(
        telemetry, sizeof(telemetry),
        "TIME WARP: X%.0f", warp
    );
    line(top - 190.0f, telemetry, white, 1.6f);

    line(top - 214.0f,
         std::string("LATEST EVENT: ") + g_last_event,
         orange, 1.5f);

    // Mission-specific educational guidance.
    const float help_y = 26.0f;
    const float help_h = 112.0f;

    hud_draw_rect(renderer, panel_buffer,
                  18.0f, help_y, 440.0f, help_h, background);

    hud_draw_rect(renderer, panel_buffer,
                  18.0f, help_y + help_h - 3.0f,
                  440.0f, 3.0f, cyan);

    hud_draw_text(renderer, text_buffer,
                  32.0f, help_y + help_h - 23.0f,
                  "MISSION GUIDE", cyan, 1.8f);

    if (countdown) {
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 65.0f,
            "WAIT FOR LAUNCH COUNTDOWN", white, 1.5f);
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 43.0f,
            "PRESS SPACE TO START OR PAUSE", white, 1.5f);
    } else if (g_sim.is_stage1_attached) {
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 65.0f,
            "BOOSTER POWERED ASCENT", orange, 1.5f);
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 43.0f,
            "WATCH ALTITUDE AND STAGE EVENTS", white, 1.5f);
    } else if (g_sim.is_stage2_attached) {
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 65.0f,
            "UPPER STAGE FLIGHT", orange, 1.5f);
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 43.0f,
            "WATCH FOR STAGE SEPARATION", white, 1.5f);
    } else if (g_sim.is_centaur_attached) {
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 65.0f,
            "CENTAUR UPPER STAGE ACTIVE", orange, 1.5f);
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 43.0f,
            "FOLLOW THE VEHICLE CAMERA", white, 1.5f);
    } else {
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 65.0f,
            "VOYAGER SPACECRAFT IN FLIGHT", green, 1.5f);
        hud_draw_text(renderer, text_buffer, 32.0f, help_y + 43.0f,
            "FOLLOW THE TRAIL AND TELEMETRY", white, 1.5f);
    }

    hud_draw_text(renderer, text_buffer, 32.0f, help_y + 15.0f,
        "M SWITCH MISSION   R RESTART", cyan, 1.35f);

    // Restore the 3D scene's usual depth-test state.
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}


// Description: pose of the rocket base in world coordinates.
WorldTransform rocket_pose() {
    WorldTransform t;
    t.position = g_sim.position;
    t.rotation = glm::mat3_cast(g_sim.orientation);
    return t;
}

// Description: start far enough away to show Earth and the launch site together.
void reset_earth_camera() {
    SitePose pad = launch_site_pose(g_sim.earth_angle(), 0);
    dvec3 sun = dvec3(sun_direction(MISSIONS[g_mission_index]));
    g_earth_camera.up_reference = dvec3(0, 1, 0);
    g_earth_camera.position =
        glm::normalize(glm::normalize(pad.position) + 0.5 * sun) * (EARTH_RADIUS_M * 3.5);
    g_earth_camera.look_at(dvec3(0));
}

// Description: reset the mission simulation and visual state.
void restart_mission(int mission) {
    g_mission_index = mission;
    g_sim.reset(mission);
    g_rocket.build(mission);
    g_trail.clear();
    g_last_trail_time = -1e9;
    g_last_event = "Countdown";
    g_vehicle_zoom = 1.0;
    g_vehicle_yaw = 0.0;
    g_vehicle_pitch = 0.0;
    reset_earth_camera();
    std::cout << "=== " << MISSIONS[mission].name << " (" << MISSIONS[mission].launch_date << ") ===\n";
}

// Advance in short chunks so flight events are handled in order.
void step_simulation(double sim_seconds) {
    while (sim_seconds > 1e-9) {
        double chunk = glm::min(sim_seconds, 0.5);
        g_sim.advance(chunk);

        for (const FlightEvent& e : g_sim.take_events()) {
            g_rocket.apply_event(e.type, rocket_pose(), g_sim.velocity);
            g_last_event = event_name(e.type);
            std::printf("T+%7.1f s  %s  (altitude %.1f km, speed %.0f m/s)\n",
                        e.time, event_name(e.type), g_sim.altitude() / 1000.0, g_sim.speed());
        }

        g_rocket.sync_engines(g_sim);
        g_rocket.boom_deploy = g_sim.boom_deploy;
        g_rocket.update_detached(chunk, free_flight_acceleration, EARTH_RADIUS_M - 5.0);

        if (g_sim.has_lifted_off &&
            g_sim.mission_time - g_last_trail_time >= TRAIL_SAMPLE_EVERY_S &&
            g_trail.size() < TRAIL_MAX_POINTS) {
            g_trail.push_back(g_sim.position);
            g_last_trail_time = g_sim.mission_time;
        }
        sim_seconds -= chunk;
    }
}

// Rocket focus height changes as stages are released.
glm::vec3 focus_point_local() {
    return glm::vec3(0.0f,
        g_sim.is_stage1_attached ? 24.0f :
        (g_sim.is_stage2_attached ? 34.0f : 43.0f), 0.0f);
}

double follow_scale() {
    return g_sim.is_stage1_attached ? 1.0 :
        (g_sim.is_stage2_attached ? 0.7 :
        (g_sim.is_centaur_attached ? 0.45 : 0.14));
}

struct CameraView {
    dvec3 position, forward, up;
    float fov;
};

CameraView compute_camera(const AppWindow& win) {
    CameraView v;
    v.fov = FIELD_OF_VIEW_DEG;
    WorldTransform pose = rocket_pose();
    dvec3 focus = Rocket::point_in_world(pose, focus_point_local());

    if (g_camera_mode == CAMERA_EARTH) {
        v.position = g_earth_camera.position;
        v.forward = g_earth_camera.forward();
        v.up = g_earth_camera.up();
    } else if (g_camera_mode == CAMERA_SITE) {
        SitePose pad = launch_site_pose(g_sim.earth_angle(), 0);
        v.position = pad.position + pad.basis * SITE_CAMERA_OFFSET;
        v.forward = glm::normalize(focus - v.position);
        v.up = glm::normalize(v.position);

        // The launch-site camera must widen its field of view as the rocket recedes.
        // The previous inverse-distance formula narrowed the view during ascent.
        double distance = glm::length(focus - v.position);
        v.fov = (float)glm::clamp(
            glm::degrees(2.0 * std::atan(distance / 2000.0)),
            12.0, 65.0
        );
    } else {
        double scale = follow_scale() * g_vehicle_zoom;
        double yaw = deg_to_rad(g_vehicle_yaw);
        double pitch = deg_to_rad(g_vehicle_pitch);
        dvec3 offset = dvec3(
            std::sin(yaw) * std::cos(pitch),
            std::sin(pitch),
            std::cos(yaw) * std::cos(pitch)
        ) * (FOLLOW_CAMERA_DISTANCE_M * scale);

        glm::dvec3 local_focus(focus_point_local());
        v.position = pose.position + pose.rotation * (local_focus + offset);
        v.forward = glm::normalize(focus - v.position);
        v.up = pose.rotation * dvec3(0, 1, 0);
    }

    (void)win;
    return v;
}

// Draw the flight trail in the Earth view, plus markers for rocket and pad.

// Three directional-light presets make the launch vehicle easier to inspect.
// The scene renderer uses a directional Sun vector, so these alter the key-light direction
// rather than pretending to provide physically based multi-light rendering.
dvec3 active_sun_direction() {
    const dvec3 mission_sun = dvec3(sun_direction(MISSIONS[g_mission_index]));
    if (g_lighting_mode == 1)
        return glm::normalize(dvec3(-0.75, 0.25, 0.60)); // warm-looking side illumination
    if (g_lighting_mode == 2)
        return glm::normalize(dvec3(0.15, 0.75, -0.65)); // rim/back illumination
    return glm::normalize(mission_sun);
}

// A camera-facing cyan atmospheric limb. EarthSystem already draws the atmosphere;
// this extra subtle ring is an inexpensive visual accent, not a volumetric scattering model.
void draw_atmosphere_limb(SceneRenderer& r, const FrameContext& ctx) {
    static PointBuffer atmosphere_buffer;
    const dvec3 normal = glm::normalize(ctx.camera_position);
    dvec3 right = glm::cross(normal, dvec3(0.0, 1.0, 0.0));
    if (glm::length(right) < 1e-5) right = dvec3(1.0, 0.0, 0.0);
    right = glm::normalize(right);
    const dvec3 up = glm::normalize(glm::cross(right, normal));
    const double radius = EARTH_RADIUS_M * 1.018;
    std::vector<glm::vec3> ring;
    ring.reserve(96);
    for (int i = 0; i < 96; ++i) {
        const double a = 6.28318530717958647692 * i / 96.0;
        const dvec3 p = radius * (std::cos(a) * right + std::sin(a) * up);
        ring.push_back(glm::vec3(p - ctx.camera_position));
    }
    r.set_pass(ctx.view, ctx.proj_space, ctx.sun_dir);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    atmosphere_buffer.update(ring);
    r.draw_points(atmosphere_buffer, GL_LINE_LOOP, glm::vec4(0.12f, 0.62f, 1.0f, 0.28f), 2.0f);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

// A lightweight stylised engine plume. It is drawn in world coordinates behind the
// current velocity direction, so it remains visible from Earth, launch-site, and vehicle views.
// This is a visual effect, not a combustion or exhaust-particle simulation.

void draw_engine_exhaust(SceneRenderer& r, const FrameContext& ctx) {
    if (!g_sim.has_lifted_off) return;

    static PointBuffer outer_buffer;
    static PointBuffer middle_buffer;
    static PointBuffer core_buffer;

    const double time = static_cast<double>(r.time_seconds);

    // Use the flight direction once the vehicle is moving.
    // At liftoff, use the rocket's own downward axis instead.
    WorldTransform pose = rocket_pose();

    dvec3 direction = g_sim.velocity;
    if (glm::length(direction) > 2.0) {
        direction = glm::normalize(direction);
    } else {
        direction = glm::normalize(
            pose.rotation * dvec3(0.0, 1.0, 0.0)
        );
    }

    const dvec3 back = -direction;

    // Build two axes perpendicular to the plume.
    dvec3 side = glm::cross(back, dvec3(0.0, 1.0, 0.0));
    if (glm::length(side) < 1e-5)
        side = glm::cross(back, dvec3(1.0, 0.0, 0.0));

    side = glm::normalize(side);
    const dvec3 other = glm::normalize(glm::cross(back, side));

    // Keep the plume visible near liftoff, then extend it as speed increases.
    const double speed_factor =
        glm::clamp(g_sim.speed() / 1000.0, 0.0, 1.5);

    const double plume_length =
        22.0 + 18.0 * speed_factor;

    const double flicker =
        0.85 + 0.15 * std::sin(time * 23.0)
             + 0.08 * std::sin(time * 41.0);

    std::vector<glm::vec3> outer;
    std::vector<glm::vec3> middle;
    std::vector<glm::vec3> core;

    constexpr int OUTER_COUNT = 100;
    constexpr int MIDDLE_COUNT = 75;
    constexpr int CORE_COUNT = 48;

    outer.reserve(OUTER_COUNT);
    middle.reserve(MIDDLE_COUNT);
    core.reserve(CORE_COUNT);

    // Generate an animated, expanding outer flame.
    for (int i = 0; i < OUTER_COUNT; ++i) {
        const double t =
            static_cast<double>(i) / (OUTER_COUNT - 1);

        const double angle =
            i * 2.399963229728653 + time * 3.0;

        const double radius =
            (0.35 + 3.5 * t) *
            (0.80 + 0.20 * std::sin(time * 17.0 + i));

        const double length_offset =
            plume_length * t;

        const dvec3 point =
            g_sim.position
            + back * length_offset
            + side * (std::cos(angle) * radius)
            + other * (std::sin(angle) * radius);

        outer.push_back(glm::vec3(point - ctx.camera_position));
    }

    // Yellow-orange inner flame.
    for (int i = 0; i < MIDDLE_COUNT; ++i) {
        const double t =
            static_cast<double>(i) / (MIDDLE_COUNT - 1);

        const double angle =
            i * 2.399963229728653 - time * 5.0;

        const double radius =
            (0.15 + 1.45 * t) *
            (0.85 + 0.15 * std::sin(time * 21.0 + i));

        const dvec3 point =
            g_sim.position
            + back * (plume_length * 0.78 * t)
            + side * (std::cos(angle) * radius)
            + other * (std::sin(angle) * radius);

        middle.push_back(glm::vec3(point - ctx.camera_position));
    }

    // Narrow, bright core near the engine nozzle.
    for (int i = 0; i < CORE_COUNT; ++i) {
        const double t =
            static_cast<double>(i) / (CORE_COUNT - 1);

        const double angle =
            i * 2.399963229728653 + time * 8.0;

        const double radius =
            (0.08 + 0.48 * t) *
            (0.75 + 0.25 * std::sin(time * 29.0 + i));

        const dvec3 point =
            g_sim.position
            + back * (plume_length * 0.42 * t)
            + side * (std::cos(angle) * radius)
            + other * (std::sin(angle) * radius);

        core.push_back(glm::vec3(point - ctx.camera_position));
    }

    r.set_pass(ctx.view, ctx.proj_space, ctx.sun_dir);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    // Wide, soft orange exhaust envelope.
    outer_buffer.update(outer);
    r.draw_points(
        outer_buffer,
        GL_POINTS,
        glm::vec4(1.0f, 0.16f, 0.015f,
                  static_cast<float>(0.42 * flicker)),
        25.0f
    );

    // Brighter yellow-orange interior.
    middle_buffer.update(middle);
    r.draw_points(
        middle_buffer,
        GL_POINTS,
        glm::vec4(1.0f, 0.48f, 0.055f,
                  static_cast<float>(0.72 * flicker)),
        13.0f
    );

    // White-hot centre.
    core_buffer.update(core);
    r.draw_points(
        core_buffer,
        GL_POINTS,
        glm::vec4(1.0f, 0.91f, 0.67f,
                  static_cast<float>(0.95 * flicker)),
        6.0f
    );

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}


void draw_trail_and_markers(SceneRenderer& r, const FrameContext& ctx, const SitePose& pad) {
    static PointBuffer trail_buffer, marker_buffer;
    std::vector<glm::vec3> rel;
    rel.reserve(g_trail.size() + 1);
    for (const dvec3& p : g_trail)
        rel.push_back(glm::vec3(p - ctx.camera_position));
    rel.push_back(glm::vec3(g_sim.position - ctx.camera_position));

    trail_buffer.update(rel);
    r.set_pass(ctx.view, ctx.proj_space, ctx.sun_dir);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    if (g_sim.has_lifted_off && g_camera_mode == CAMERA_EARTH)
        r.draw_points(trail_buffer, GL_LINE_STRIP, glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));

    marker_buffer.update({glm::vec3(g_sim.position - ctx.camera_position)});
    r.draw_points(marker_buffer, GL_POINTS, glm::vec4(1.0f, 1.0f, 0.4f, 1.0f), 5.0f);

    marker_buffer.update({glm::vec3(pad.position - ctx.camera_position)});
    r.draw_points(marker_buffer, GL_POINTS, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), 5.0f);
}

// Keep the title readable and refresh it only four times per second.
void update_title(AppWindow& win, double warp) {
    static double next_update = 0;
    if (win.total_time < next_update) return;
    next_update = win.total_time + 0.25;

    double t = g_sim.mission_time;
    char title[256];
    std::snprintf(title, sizeof(title),
        "%s | T%c%d:%02d | alt %.1f km | speed %.2f km/s | x%.0f | cam %d | light %d | %s%s",
        MISSIONS[g_mission_index].name,
        t < 0 ? '-' : '+',
        (int)std::fabs(t) / 60, (int)std::fabs(t) % 60,
        g_sim.altitude() / 1000.0, g_sim.speed() / 1000.0, warp,
        (int)g_camera_mode, g_lighting_mode, g_last_event.c_str(),
        g_running ? "" : " | PAUSED (SPACE to start)");
    glfwSetWindowTitle(win.handle, title);
}

// Handle mission, playback, camera and zoom controls.
void handle_keys(AppWindow& win, double& warp) {
    if (win.was_key_pressed(GLFW_KEY_SPACE)) g_running = !g_running;
    if (win.was_key_pressed(GLFW_KEY_M)) restart_mission(1 - g_mission_index);
    if (win.was_key_pressed(GLFW_KEY_R)) restart_mission(g_mission_index);

    if (win.was_key_pressed(GLFW_KEY_1)) g_camera_mode = CAMERA_EARTH;
    if (win.was_key_pressed(GLFW_KEY_2)) g_camera_mode = CAMERA_SITE;
    if (win.was_key_pressed(GLFW_KEY_3)) g_camera_mode = CAMERA_VEHICLE;
    if (win.was_key_pressed(GLFW_KEY_L)) g_lighting_mode = (g_lighting_mode + 1) % 3;
    if (win.was_key_pressed(GLFW_KEY_0)) g_lighting_mode = 0;

    if (win.was_key_pressed(GLFW_KEY_W)) g_auto_warp = !g_auto_warp;
    if (win.was_key_pressed(GLFW_KEY_PERIOD)) {
        g_auto_warp = false;
        g_manual_warp = glm::min(MAX_WARP, glm::max(warp, 1.0) * 2.0);
    }
    if (win.was_key_pressed(GLFW_KEY_COMMA)) {
        g_auto_warp = false;
        g_manual_warp = glm::max(MIN_WARP, warp * 0.5);
    }

    if (g_camera_mode == CAMERA_EARTH) {
        if (win.was_key_pressed(GLFW_KEY_F)) g_earth_camera.look_at(dvec3(0));
        handle_fly_keys(win, g_earth_camera, fly_speed_for(g_earth_camera.position, 5.0));
    } else if (g_camera_mode == CAMERA_VEHICLE) {
        if (win.is_key_down(GLFW_KEY_RIGHT_BRACKET))
            g_vehicle_zoom *= 1.0 + 1.5 * win.delta_time;
        if (win.is_key_down(GLFW_KEY_LEFT_BRACKET))
            g_vehicle_zoom /= 1.0 + 1.5 * win.delta_time;

        g_vehicle_zoom = glm::clamp(g_vehicle_zoom * std::pow(0.9, win.scroll_y), 0.05, 40.0);
        if (win.is_dragging()) {
            g_vehicle_yaw -= win.mouse_dx * 0.3;
            g_vehicle_pitch = glm::clamp(g_vehicle_pitch + win.mouse_dy * 0.3, -80.0, 80.0);
        }
    }
}

int main(int argc, char** argv) {
    AppWindow win;
    if (!win.init("a2-scene  Voyager launch", argc, argv)) return 1;

    SceneRenderer renderer;
    renderer.init();

    EarthSystem earth;
    earth.init();

    LaunchPad pad;
    pad.init();

    if (const char* m = win.get_arg("--mission"))
        g_mission_index = std::atoi(m) == 1 ? 0 : 1;

    restart_mission(g_mission_index);

    if (const char* c = win.get_arg("--camera"))
        g_camera_mode = (CameraMode)glm::clamp(std::atoi(c), 1, 3);
    if (const char* s = win.get_arg("--start")) {
        step_simulation(std::atof(s) - COUNTDOWN_START_S);
        g_running = false;
    }
    if (win.has_flag("--run")) g_running = true;
    if (const char* z = win.get_arg("--zoom")) g_vehicle_zoom = std::atof(z);
    if (const char* y = win.get_arg("--yaw")) g_vehicle_yaw = std::atof(y);

    if (g_camera_mode == CAMERA_EARTH && win.get_arg("--eye")) {
        double x, y, z;
        std::sscanf(win.get_arg("--eye"), "%lf,%lf,%lf", &x, &y, &z);
        g_earth_camera.position = dvec3(x, y, z) * EARTH_RADIUS_M;
        g_earth_camera.look_at(dvec3(0));
    }

    while (win.is_open()) {
        win.begin_frame();

        double warp = g_auto_warp ? g_sim.recommended_warp() : g_manual_warp;
        handle_keys(win, warp);
        warp = g_auto_warp ? g_sim.recommended_warp() : g_manual_warp;

        if (g_running) step_simulation(win.delta_time * warp);

        renderer.time_seconds = (float)win.total_time;
        CameraView cam = compute_camera(win);
        SitePose pad_pose = launch_site_pose(g_sim.earth_angle(), 0);
        double nearest = glm::min(
            glm::length(cam.position - pad_pose.position),
            glm::length(cam.position - g_sim.position)
        );

        FrameContext ctx = make_frame_context(
            cam.position, cam.forward, cam.up, cam.fov, win.aspect(),
            nearest, active_sun_direction(), (float)win.total_time
        );

        glViewport(0, 0, win.width, win.height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        earth.draw(renderer, ctx, g_sim.earth_angle());
        draw_atmosphere_limb(renderer, ctx);
        draw_trail_and_markers(renderer, ctx, pad_pose);

        begin_local_pass(renderer, ctx);

        if (nearest < 3.0e6)
            pad.draw(renderer, ctx, pad_pose);

        const double rocket_altitude = glm::max(0.0, g_sim.altitude());
        // Start with a short ground-safe plume, then allow it to grow as the
        // vehicle climbs into thinner atmosphere.
        // Keep a clearly visible nozzle flame near the pad, then let it grow with altitude.
        const float plume_limit = (float)glm::max(4.0, rocket_altitude * 0.00045);
        g_rocket.draw(
            renderer, ctx, rocket_pose(),
            rocket_altitude, true, plume_limit
        );

        // Add the larger animated exhaust envelope after liftoff. This helper was
        // implemented above but was not called by the main render loop.
        draw_engine_exhaust(renderer, ctx);

        // Draw the Assignment 2 on-screen mission information panel.
        draw_mission_hud(renderer, win, warp);

        update_title(win, warp);
        win.end_frame();
            }

    glfwTerminate();
    return 0;
}