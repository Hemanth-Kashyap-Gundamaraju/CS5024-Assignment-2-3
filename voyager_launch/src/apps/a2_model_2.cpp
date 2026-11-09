// a2-model-2: vehicle and payload viewer - Titan IIIE / Centaur with Voyager 1 or 2, stages that detach.
// Keys: mouse drag = orbit | wheel = zoom | N = release next stage | O = open/close Voyager booms
//       B = engines on/off | 1 / 2 = Voyager 1 / 2 stripe | H = whole rocket | V = Voyager close-up | R = reset
#include "rocket.h"
#include "hud.h"

OrbitCamera g_camera;
bool g_engines_on = false;
bool g_show_help = true;
int g_next_event = 0;
double g_boom_target = 0.0;

// The order the real vehicle dropped its parts.
const FlightEventType RELEASE_ORDER[] = {EVENT_SRM_JETTISON, EVENT_STAGE1_SEPARATION, EVENT_SHROUD_JETTISON,
                                         EVENT_STAGE2_SEPARATION, EVENT_VOYAGER_SEPARATION, EVENT_KICK_SEPARATION};
constexpr int RELEASE_COUNT = sizeof(RELEASE_ORDER) / sizeof(RELEASE_ORDER[0]);

void view_whole_rocket() { g_camera.target = dvec3(0, 22, 0); g_camera.distance = 95.0; }
void view_voyager() { g_camera.target = dvec3(0, 44.0, 0); g_camera.distance = 16.0; }

// Description: drop the next part in the real order. In: rocket and the (fixed) pose.
void release_next(Rocket& rocket, const WorldTransform& pose) {
    if (g_next_event >= RELEASE_COUNT) return;
    rocket.apply_event(RELEASE_ORDER[g_next_event], pose, dvec3(0.0));
    std::cout << "Released: " << event_name(RELEASE_ORDER[g_next_event]) << "\n";
    g_next_event++;
}
// Description: engines that can still fire right now.
void update_engines(Rocket& rocket) { for (int i = 0; i < PIECE_COUNT; i++) rocket.set_burning((PieceId)i, g_engines_on); }

int main(int argc, char** argv) {
    AppWindow win;
    if (!win.init("a2-model-2  Vehicle viewer", argc, argv)) return 1;
    SceneRenderer renderer; renderer.init();
    Rocket rocket; rocket.build(g_mission_index);
    view_whole_rocket();
    WorldTransform pose;
    if (const char* n = win.get_arg("--release")) for (int i = 0; i < std::atoi(n); i++) release_next(rocket, pose);
    if (win.has_flag("--voyager")) view_voyager();
    if (win.has_flag("--open")) g_boom_target = 1.0;
    if (win.has_flag("--engines")) g_engines_on = true;
    const glm::vec3 sun = glm::normalize(glm::vec3(0.6f, 0.5f, 0.6f));
    AccelerationFunction no_forces = [](const dvec3&, const dvec3&, double) { return dvec3(0.0); };

    while (win.is_open()) {
        win.begin_frame();
        if (win.is_dragging()) g_camera.rotate_with_mouse(win.mouse_dx, win.mouse_dy);
        g_camera.zoom(win.scroll_y);
        if (win.was_key_pressed(GLFW_KEY_N)) release_next(rocket, pose);
        if (win.was_key_pressed(GLFW_KEY_O)) g_boom_target = 1.0 - g_boom_target;
        if (win.was_key_pressed(GLFW_KEY_B)) g_engines_on = !g_engines_on;
        if (win.was_key_pressed(GLFW_KEY_H)) view_whole_rocket();
        if (win.was_key_pressed(GLFW_KEY_V)) view_voyager();
        if (win.was_key_pressed(GLFW_KEY_R)) { rocket.reset(); g_next_event = 0; g_boom_target = 0.0; }
        if (win.was_key_pressed(GLFW_KEY_F1)) g_show_help = !g_show_help;
        for (int k = 0; k < 2; k++)
            if (win.was_key_pressed(GLFW_KEY_1 + k)) { g_mission_index = k; rocket.build(k); g_next_event = 0; }
        rocket.boom_deploy += glm::clamp(g_boom_target - rocket.boom_deploy, -win.delta_time * 0.2, win.delta_time * 0.2);
        update_engines(rocket);
        rocket.update_detached(win.delta_time, no_forces, 0.0);

        renderer.time_seconds = (float)win.total_time;
        dvec3 eye = g_camera.position();
        FrameContext ctx = make_frame_context(eye, g_camera.forward(), dvec3(0, 1, 0), FIELD_OF_VIEW_DEG, win.aspect(), g_camera.distance, sun, (float)win.total_time);
        glViewport(0, 0, win.width, win.height);
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        begin_local_pass(renderer, ctx);
        rocket.draw(renderer, ctx, pose, 0.0, true);
        char title[140];
        std::snprintf(title, sizeof(title), "a2-model-2 | %s | released %d/%d | booms %.0f%% | engines %s", MISSIONS[g_mission_index].name,
                      g_next_event, RELEASE_COUNT, rocket.boom_deploy * 100.0, g_engines_on ? "ON" : "off");
        glfwSetWindowTitle(win.handle, title);
        ViewerHUD::draw(renderer, win, "VEHICLE VIEWER - CONTROLS", {
            "MOUSE DRAG ORBIT CAMERA",
            "MOUSE WHEEL ZOOM",
            "N RELEASE NEXT COMPONENT",
            "O DEPLOY OR RETRACT BOOMS",
            "B TOGGLE ENGINE FLAMES",
            "H FRAME COMPLETE ROCKET",
            "V FOCUS VOYAGER",
            "1 2 SELECT VOYAGER MISSION",
            "R RESET VEHICLE  F1 HIDE PANEL"
        }, g_show_help);
        win.end_frame();
    }
    glfwTerminate();
    return 0;
}

