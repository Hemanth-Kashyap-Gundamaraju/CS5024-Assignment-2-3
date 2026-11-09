// a2-model-1: launch-site viewer - Cape Canaveral LC-41 style pad with ground texture (rocket optional).
// Keys: W/S forward-back, A/D left-right, Q/E down-up (hold Shift = faster) | arrows = X/Y, [ ] = Z | mouse drag = look
//       V show/hide rocket | B engines on/off (flame test) | 1 = Voyager 1 stripe, 2 = Voyager 2 stripe | R reset camera
#include "launch_pad.h"
#include "rocket.h"
#include "hud.h"

constexpr double WALK_SPEED = 18.0;
FlyCamera g_camera;
bool g_show_rocket = true, g_engines_on = false, g_show_help = true;
const SitePose PAD_POSE = {dvec3(0.0), dmat3(1.0)};   // pad frame IS the world here: +X east, +Y up, +Z south

// Description: start position about 120 m from the pad, facing it.
void reset_camera() { g_camera.position = dvec3(-60.0, 8.0, 120.0); g_camera.look_at(dvec3(0, 25, 0)); }

// Description: WASD/QE movement relative to where the camera looks. In: window, camera, speed.
void handle_walk_keys(const AppWindow& win, FlyCamera& cam, double speed) {
    double step = speed * win.delta_time * (win.is_key_down(GLFW_KEY_LEFT_SHIFT) ? 4.0 : 1.0);
    if (win.is_key_down(GLFW_KEY_W)) cam.translate_world(cam.forward() * step);
    if (win.is_key_down(GLFW_KEY_S)) cam.translate_world(-cam.forward() * step);
    if (win.is_key_down(GLFW_KEY_D)) cam.translate_world(cam.right() * step);
    if (win.is_key_down(GLFW_KEY_A)) cam.translate_world(-cam.right() * step);
    if (win.is_key_down(GLFW_KEY_E)) cam.translate_world(dvec3(0, step, 0));
    if (win.is_key_down(GLFW_KEY_Q)) cam.translate_world(dvec3(0, -step, 0));
}

int main(int argc, char** argv) {
    AppWindow win;
    if (!win.init("a2-model-1  Launch site viewer", argc, argv)) return 1;
    SceneRenderer renderer; renderer.init();
    LaunchPad pad; pad.init();
    Rocket rocket; rocket.build(g_mission_index);
    reset_camera();
    if (win.has_flag("--engines")) g_engines_on = true;

    WorldTransform rocket_pose; rocket_pose.position = dvec3(0, ROCKET_BASE_HEIGHT_M, 0);
    const glm::vec3 sun = glm::normalize(glm::vec3(0.75f, 0.55f, 0.3f));

    while (win.is_open()) {
        win.begin_frame();
        handle_walk_keys(win, g_camera, WALK_SPEED);
        handle_fly_keys(win, g_camera, WALK_SPEED);
        if (win.was_key_pressed(GLFW_KEY_V)) g_show_rocket = !g_show_rocket;
        if (win.was_key_pressed(GLFW_KEY_B)) g_engines_on = !g_engines_on;
        if (win.was_key_pressed(GLFW_KEY_R)) reset_camera();
        if (win.was_key_pressed(GLFW_KEY_F1)) g_show_help = !g_show_help;
        for (int k = 0; k < 2; k++)
            if (win.was_key_pressed(GLFW_KEY_1 + k)) { g_mission_index = k; rocket.build(k); }
        for (int i = 0; i < PIECE_COUNT; i++) rocket.set_burning((PieceId)i, g_engines_on);

        renderer.time_seconds = (float)win.total_time;
        FrameContext ctx = make_frame_context(g_camera.position, g_camera.forward(), g_camera.up(), FIELD_OF_VIEW_DEG, win.aspect(),
                                              glm::max(1.0, glm::length(g_camera.position - rocket_pose.position)), sun, (float)win.total_time);
        glViewport(0, 0, win.width, win.height);
        glClearColor(0.52f, 0.72f, 0.95f, 1.0f);      // daytime sky colour
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        begin_local_pass(renderer, ctx);
        pad.draw(renderer, ctx, PAD_POSE);
        if (g_show_rocket) rocket.draw(renderer, ctx, rocket_pose, 0.0, true, 0.75f);
        char title[120];
        std::snprintf(title, sizeof(title), "a2-model-1 | Launch Complex 41 | %s stripe | engines %s", MISSIONS[g_mission_index].name, g_engines_on ? "ON" : "off");
        glfwSetWindowTitle(win.handle, title);
        ViewerHUD::draw(renderer, win, "LAUNCH SITE - CONTROLS", {
            "W S MOVE FORWARD BACK",
            "A D MOVE LEFT RIGHT",
            "Q E MOVE DOWN UP",
            "SHIFT MOVE FASTER",
            "ARROWS MOVE CAMERA X Y",
            "[ ] MOVE CAMERA Z",
            "MOUSE DRAG LOOK AROUND",
            "V ROCKET ON OFF  B FLAMES",
            "1 2 SELECT VOYAGER MISSION",
            "R RESET CAMERA  F1 HIDE PANEL"
        }, g_show_help);
        win.end_frame();
    }
    glfwTerminate();
    return 0;
}
