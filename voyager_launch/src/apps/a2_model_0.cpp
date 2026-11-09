// a2-model-0: Earth viewer - Earth, atmosphere, stars, Sun and Moon.
// Keys: A/a, D/d spin Earth | arrows = camera X/Y | [ ] = camera Z | mouse drag = look around
//       1 space view, 2 low orbit, 3 ground view at the launch site
//       F look at Earth | M look at Moon | V switch Voyager launch day | R reset current view
#include "earth.h"
#include "hud.h"

constexpr double EARTH_SPIN_KEY_SPEED = 0.8;
constexpr double SPACE_VIEW_DISTANCE = 3.5 * EARTH_RADIUS_M;

FlyCamera g_camera;
double g_earth_angle = 0.0;
int g_active_view = 1;
bool g_show_help = true;

// Description: camera far away so the whole Earth fits in view.
void set_space_view() {
    g_active_view = 1;
    g_camera.up_reference = dvec3(0, 1, 0);
    g_camera.position = dvec3(0.5, 0.35, 1.0) * (SPACE_VIEW_DISTANCE / 1.2);
    g_camera.look_at(dvec3(0));
}

// Description: camera above the launch site, looking toward the horizon.
void set_orbit_view() {
    g_active_view = 2;
    SitePose pose = launch_site_pose(g_earth_angle, 0);
    dvec3 up = pose.basis[1], east = pose.basis[0];
    g_camera.up_reference = up;
    g_camera.position = pose.position + up * 400000.0 - east * 900000.0;
    g_camera.look_at(pose.position + up * 20000.0 + east * 700000.0);
}

// Description: camera 30 m above the launch site, looking east along the ground.
void set_ground_view() {
    g_active_view = 3;
    SitePose pose = launch_site_pose(g_earth_angle, 0);
    dvec3 up = pose.basis[1], east = pose.basis[0];
    g_camera.up_reference = up;
    g_camera.position = pose.position + up * 30.0;
    g_camera.look_at(g_camera.position + east * 2000.0 + up * 150.0);
}

// Rebuild the selected preset after the mission/launch-day changes.
void restore_active_view() {
    if (g_active_view == 2) set_orbit_view();
    else if (g_active_view == 3) set_ground_view();
    else set_space_view();
}

int main(int argc, char** argv) {
    AppWindow win;
    if (!win.init("a2-model-0  Earth viewer", argc, argv)) return 1;

    SceneRenderer renderer;
    renderer.init();

    EarthSystem earth;
    earth.init();

    g_earth_angle = earth_angle_at_launch(MISSIONS[g_mission_index]);
    set_space_view();

    if (const char* v = win.get_arg("--view")) {
        int n = std::atoi(v);
        if (n == 2) set_orbit_view();
        else if (n == 3) set_ground_view();
    }

    while (win.is_open()) {
        win.begin_frame();

        if (win.is_key_down(GLFW_KEY_A))
            g_earth_angle -= EARTH_SPIN_KEY_SPEED * win.delta_time;
        if (win.is_key_down(GLFW_KEY_D))
            g_earth_angle += EARTH_SPIN_KEY_SPEED * win.delta_time;

        if (win.was_key_pressed(GLFW_KEY_1)) set_space_view();
        if (win.was_key_pressed(GLFW_KEY_2)) set_orbit_view();
        if (win.was_key_pressed(GLFW_KEY_3)) set_ground_view();

        if (win.was_key_pressed(GLFW_KEY_F)) g_camera.look_at(dvec3(0));
        if (win.was_key_pressed(GLFW_KEY_M)) g_camera.look_at(moon_position());

        // Switch between the two launch days and update the Earth angle as well as the sunlight.
        if (win.was_key_pressed(GLFW_KEY_V)) {
            g_mission_index = 1 - g_mission_index;
            g_earth_angle = earth_angle_at_launch(MISSIONS[g_mission_index]);
            restore_active_view();
        }

        if (win.was_key_pressed(GLFW_KEY_R)) restore_active_view();
        if (win.was_key_pressed(GLFW_KEY_F1)) g_show_help = !g_show_help;

        handle_fly_keys(win, g_camera, fly_speed_for(g_camera.position, 5.0));

        renderer.time_seconds = (float)win.total_time;
        FrameContext ctx = make_frame_context(
            g_camera.position, g_camera.forward(), g_camera.up(),
            FIELD_OF_VIEW_DEG, win.aspect(), 1.0e12,
            sun_direction(MISSIONS[g_mission_index]), (float)win.total_time
        );

        glViewport(0, 0, win.width, win.height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        earth.draw(renderer, ctx, g_earth_angle);

        char title[160];
        std::snprintf(
            title, sizeof(title),
            "a2-model-0 | view %d | camera height %.0f km | Earth angle %.1f deg | Sun: %s day",
            g_active_view, ctx.altitude / 1000.0, glm::degrees(g_earth_angle),
            MISSIONS[g_mission_index].name
        );
        glfwSetWindowTitle(win.handle, title);

        ViewerHUD::draw(renderer, win, "EARTH VIEWER - CONTROLS", {
            "A D ROTATE EARTH",
            "ARROWS MOVE CAMERA X Y",
            "[ ] MOVE CAMERA Z",
            "MOUSE DRAG LOOK AROUND",
            "1 SPACE  2 ORBIT  3 GROUND",
            "F FOCUS EARTH  M FOCUS MOON",
            "V SWITCH VOYAGER  R RESET",
            "F1 HIDE OR SHOW THIS PANEL"
        }, g_show_help);
        win.end_frame();
    }

    glfwTerminate();
    return 0;
}