// a2-scene: the whole Voyager launch - Earth, atmosphere, Moon, Cape Canaveral pad, Titan IIIE/Centaur + Voyager.
// Keys:  SPACE start/pause | M switch Voyager 1 <-> 2 (restarts) | R restart
//        1 Earth camera | 2 launch-site camera | 3 vehicle camera (follows the rocket)
//        Earth camera: arrows = X/Y, [ ] = Z, mouse drag = look, F = look at Earth
//        Vehicle camera: [ ] zoom, mouse drag = look around the rocket
//        . / , faster / slower time | W auto time-speed on/off
// Test flags: --start SECONDS (jump to T+SECONDS)  --camera N  --shot file.ppm
#include <deque>
#include "earth.h"
#include "launch_pad.h"
#include "rocket.h"

// ---------- globals ----------
enum CameraMode { CAMERA_EARTH = 1, CAMERA_SITE = 2, CAMERA_VEHICLE = 3 };
constexpr double TRAIL_SAMPLE_EVERY_S = 2.0;
constexpr size_t TRAIL_MAX_POINTS = 4000;
constexpr double MAX_WARP = 200.0, MIN_WARP = 1.0;
constexpr double FOLLOW_CAMERA_DISTANCE_M = 70.0;   // vehicle camera distance while the whole rocket is there
const glm::dvec3 SITE_CAMERA_OFFSET(80.0, 7.0, 110.0);   // pad-local position of the launch-site camera (east, up, south)

FlightSim g_sim;
Rocket g_rocket;
FlyCamera g_earth_camera;
CameraMode g_camera_mode = CAMERA_EARTH;
bool g_running = false, g_auto_warp = true;
double g_manual_warp = 1.0, g_vehicle_zoom = 1.0, g_vehicle_yaw = 0.0, g_vehicle_pitch = 0.0;
std::vector<dvec3> g_trail;
double g_last_trail_time = -1e9;
std::string g_last_event = "Countdown";

// ---------- helpers ----------
// Description: pose of the rocket's base in the world, from the simulation.
WorldTransform rocket_pose() { WorldTransform t; t.position = g_sim.position; t.rotation = glm::mat3_cast(g_sim.orientation); return t; }

// Description: where the Earth camera starts: far away, looking at the Earth, launch site on the lit side.
void reset_earth_camera() {
    SitePose pad = launch_site_pose(g_sim.earth_angle(), 0);
    dvec3 sun = dvec3(sun_direction(MISSIONS[g_mission_index]));
    g_earth_camera.position = glm::normalize(glm::normalize(pad.position) + 0.5 * sun) * (EARTH_RADIUS_M * 3.5);
    g_earth_camera.look_at(dvec3(0));
}

// Description: start (or restart) the mission. In: mission index (0 = Voyager 1, 1 = Voyager 2).
void restart_mission(int mission) {
    g_mission_index = mission;
    g_sim.reset(mission);
    g_rocket.build(mission);
    g_trail.clear(); g_last_trail_time = -1e9;
    g_last_event = "Countdown";
    reset_earth_camera();
    std::cout << "=== " << MISSIONS[mission].name << "  (" << MISSIONS[mission].launch_date << ") ===\n";
}

// Description: run the simulation, tell the rocket model about events, move dropped stages, record the trail.
// In: seconds of simulated time to advance (done in 0.5 s pieces so events happen in order).
void step_simulation(double sim_seconds) {
    while (sim_seconds > 1e-9) {
        double chunk = glm::min(sim_seconds, 0.5);
        g_sim.advance(chunk);
        for (const FlightEvent& e : g_sim.take_events()) {
            g_rocket.apply_event(e.type, rocket_pose(), g_sim.velocity);
            g_last_event = event_name(e.type);
            std::printf("T+%7.1f s  %s  (altitude %.1f km, speed %.0f m/s)\n", e.time, event_name(e.type), g_sim.altitude() / 1000.0, g_sim.speed());
        }
        g_rocket.sync_engines(g_sim);
        g_rocket.boom_deploy = g_sim.boom_deploy;
        g_rocket.update_detached(chunk, free_flight_acceleration, EARTH_RADIUS_M - 5.0);
        if (g_sim.has_lifted_off && g_sim.mission_time - g_last_trail_time >= TRAIL_SAMPLE_EVERY_S && g_trail.size() < TRAIL_MAX_POINTS) {
            g_trail.push_back(g_sim.position); g_last_trail_time = g_sim.mission_time;
        }
        sim_seconds -= chunk;
    }
}

// Description: the point the launch-site and vehicle cameras look at, in rocket space (height depends on what is left).
glm::vec3 focus_point_local() { return glm::vec3(0.0f, g_sim.is_stage1_attached ? 24.0f : (g_sim.is_stage2_attached ? 34.0f : 43.0f), 0.0f); }
// Description: how close the follow camera sits (smaller once the rocket is smaller).
double follow_scale() { return g_sim.is_stage1_attached ? 1.0 : (g_sim.is_stage2_attached ? 0.7 : (g_sim.is_centaur_attached ? 0.45 : 0.14)); }

// Description: camera data for the current mode. Out: position, forward, up, field of view.
struct CameraView { dvec3 position, forward, up; float fov; };
CameraView compute_camera(const AppWindow& win) {
    CameraView v; v.fov = FIELD_OF_VIEW_DEG;
    WorldTransform pose = rocket_pose();
    dvec3 focus = Rocket::point_in_world(pose, focus_point_local());
    if (g_camera_mode == CAMERA_EARTH) {
        v.position = g_earth_camera.position; v.forward = g_earth_camera.forward(); v.up = g_earth_camera.up();
    } else if (g_camera_mode == CAMERA_SITE) {
        SitePose pad = launch_site_pose(g_sim.earth_angle(), 0);
        v.position = pad.position + pad.basis * SITE_CAMERA_OFFSET;
        v.forward = glm::normalize(focus - v.position);
        v.up = glm::normalize(v.position);
        double dist = glm::length(focus - v.position);
        v.fov = (float)glm::clamp(glm::degrees(2.0 * std::atan(45.0 / dist)), 1.5, (double)FIELD_OF_VIEW_DEG);   // zoom in as it flies away
    } else {
        double s = follow_scale() * g_vehicle_zoom;
        double yaw = deg_to_rad(g_vehicle_yaw), pitch = deg_to_rad(g_vehicle_pitch);
        dvec3 offset = dvec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)) * (FOLLOW_CAMERA_DISTANCE_M * s);
        glm::dvec3 local_focus(focus_point_local());
        v.position = pose.position + pose.rotation * (local_focus + offset);
        v.forward = glm::normalize(focus - v.position);
        v.up = pose.rotation * dvec3(0, 1, 0);
    }
    (void)win;
    return v;
}

// Description: draw the flight path and the little bright dots that mark the rocket and the pad from far away.
void draw_trail_and_markers(SceneRenderer& r, const FrameContext& ctx, const SitePose& pad) {
    static PointBuffer trail_buffer, marker_buffer;
    std::vector<glm::vec3> rel;
    for (const dvec3& p : g_trail) rel.push_back(glm::vec3(p - ctx.camera_position));
    rel.push_back(glm::vec3(g_sim.position - ctx.camera_position));
    trail_buffer.update(rel);
    r.set_pass(ctx.view, ctx.proj_space, ctx.sun_dir);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    // The trail is in space coordinates (the pad slides east under it), so only the Earth camera shows it.
    if (g_sim.has_lifted_off && g_camera_mode == CAMERA_EARTH) r.draw_points(trail_buffer, GL_LINE_STRIP, glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
    marker_buffer.update({glm::vec3(g_sim.position - ctx.camera_position)});
    r.draw_points(marker_buffer, GL_POINTS, glm::vec4(1.0f, 1.0f, 0.4f, 1.0f), 5.0f);
    marker_buffer.update({glm::vec3(pad.position - ctx.camera_position)});
    r.draw_points(marker_buffer, GL_POINTS, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), 5.0f);
}

// Description: show mission clock, height, speed and last event in the window title.
void update_title(AppWindow& win, double warp) {
    static double next_update = 0;
    if (win.total_time < next_update) return;
    next_update = win.total_time + 0.25;
    double t = g_sim.mission_time;
    char title[256];
    std::snprintf(title, sizeof(title), "%s | T%c%d:%02d | alt %.1f km | speed %.2f km/s | x%.0f | cam %d | %s%s", MISSIONS[g_mission_index].name,
                  t < 0 ? '-' : '+', (int)std::fabs(t) / 60, (int)std::fabs(t) % 60, g_sim.altitude() / 1000.0, g_sim.speed() / 1000.0, warp,
                  (int)g_camera_mode, g_last_event.c_str(), g_running ? "" : " | PAUSED (SPACE to start)");
    glfwSetWindowTitle(win.handle, title);
}

// Description: handle all keys for the scene.
void handle_keys(AppWindow& win, double& warp) {
    if (win.was_key_pressed(GLFW_KEY_SPACE)) g_running = !g_running;
    if (win.was_key_pressed(GLFW_KEY_M)) restart_mission(1 - g_mission_index);
    if (win.was_key_pressed(GLFW_KEY_R)) restart_mission(g_mission_index);
    if (win.was_key_pressed(GLFW_KEY_1)) g_camera_mode = CAMERA_EARTH;
    if (win.was_key_pressed(GLFW_KEY_2)) g_camera_mode = CAMERA_SITE;
    if (win.was_key_pressed(GLFW_KEY_3)) g_camera_mode = CAMERA_VEHICLE;
    if (win.was_key_pressed(GLFW_KEY_W)) g_auto_warp = !g_auto_warp;
    if (win.was_key_pressed(GLFW_KEY_PERIOD)) { g_auto_warp = false; g_manual_warp = glm::min(MAX_WARP, glm::max(warp, 1.0) * 2.0); }
    if (win.was_key_pressed(GLFW_KEY_COMMA)) { g_auto_warp = false; g_manual_warp = glm::max(MIN_WARP, warp * 0.5); }
    if (g_camera_mode == CAMERA_EARTH) {
        if (win.was_key_pressed(GLFW_KEY_F)) g_earth_camera.look_at(dvec3(0));
        handle_fly_keys(win, g_earth_camera, fly_speed_for(g_earth_camera.position, 5.0));
    } else if (g_camera_mode == CAMERA_VEHICLE) {
        if (win.is_key_down(GLFW_KEY_RIGHT_BRACKET)) g_vehicle_zoom *= 1.0 + 1.5 * win.delta_time;
        if (win.is_key_down(GLFW_KEY_LEFT_BRACKET)) g_vehicle_zoom /= 1.0 + 1.5 * win.delta_time;
        g_vehicle_zoom = glm::clamp(g_vehicle_zoom * std::pow(0.9, win.scroll_y), 0.05, 40.0);
        if (win.is_dragging()) { g_vehicle_yaw -= win.mouse_dx * 0.3; g_vehicle_pitch = glm::clamp(g_vehicle_pitch + win.mouse_dy * 0.3, -80.0, 80.0); }
    }
}

int main(int argc, char** argv) {
    AppWindow win;
    if (!win.init("a2-scene  Voyager launch", argc, argv)) return 1;
    SceneRenderer renderer; renderer.init();
    EarthSystem earth; earth.init();
    LaunchPad pad; pad.init();
    if (const char* m = win.get_arg("--mission")) g_mission_index = std::atoi(m) == 1 ? 0 : 1;   // --mission 1 or 2
    restart_mission(g_mission_index);
    if (const char* c = win.get_arg("--camera")) g_camera_mode = (CameraMode)glm::clamp(std::atoi(c), 1, 3);
    if (const char* s = win.get_arg("--start")) { step_simulation(std::atof(s) - COUNTDOWN_START_S); g_running = false; }
    if (win.has_flag("--run")) g_running = true;
    if (const char* z = win.get_arg("--zoom")) g_vehicle_zoom = std::atof(z);
    if (const char* y = win.get_arg("--yaw")) g_vehicle_yaw = std::atof(y);
    if (g_camera_mode == CAMERA_EARTH && win.get_arg("--eye")) {   // test helper: --eye x,y,z in Earth radii, looks at Earth
        double x, y, z; std::sscanf(win.get_arg("--eye"), "%lf,%lf,%lf", &x, &y, &z);
        g_earth_camera.position = dvec3(x, y, z) * EARTH_RADIUS_M; g_earth_camera.look_at(dvec3(0));
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
        double nearest = glm::min(glm::length(cam.position - pad_pose.position), glm::length(cam.position - g_sim.position));
        FrameContext ctx = make_frame_context(cam.position, cam.forward, cam.up, cam.fov, win.aspect(), nearest,
                                              sun_direction(MISSIONS[g_mission_index]), (float)win.total_time);
        glViewport(0, 0, win.width, win.height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        earth.draw(renderer, ctx, g_sim.earth_angle());              // pass 1: space, planets
        draw_trail_and_markers(renderer, ctx, pad_pose);
        begin_local_pass(renderer, ctx);                              // pass 2: pad and rocket
        if (nearest < 3.0e6) pad.draw(renderer, ctx, pad_pose);
        g_rocket.draw(renderer, ctx, rocket_pose(), glm::max(0.0, g_sim.altitude()), true);
        update_title(win, warp);
        win.end_frame();
    }
    glfwTerminate();
    return 0;
}
