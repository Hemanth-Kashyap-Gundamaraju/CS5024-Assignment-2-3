// camera.h - cameras (free-fly and orbit), the per-frame matrices (FrameContext), and key handling.
#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include "app_window.h"
#include "frames.h"

// Description: a camera you can move anywhere. Position is double (needed for space-size distances).
class FlyCamera {
public:
    dvec3 position{0.0, 0.0, 3.0e7};
    double yaw_deg = -90.0, pitch_deg = 0.0;   // yaw -90 = looking down -Z
    dvec3 up_reference{0.0, 1.0, 0.0};         // which way is "up" for the horizon (set to local up near the ground)

    dvec3 forward() const {
        double y = deg_to_rad(yaw_deg), p = deg_to_rad(pitch_deg);
        return dvec3(std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y));
    }
    dvec3 right() const { return glm::normalize(glm::cross(forward(), up_reference)); }
    dvec3 up() const { return glm::cross(right(), forward()); }

    // Description: turn the camera to face a point.
    void look_at(const dvec3& target) {
        dvec3 d = glm::normalize(target - position);
        pitch_deg = glm::degrees(std::asin(glm::clamp(d.y, -1.0, 1.0)));
        yaw_deg = glm::degrees(std::atan2(d.z, d.x));
    }
    // Description: mouse drag turns the view. In: mouse movement in pixels.
    void rotate_with_mouse(double dx, double dy) {
        yaw_deg += dx * 0.2;
        pitch_deg = glm::clamp(pitch_deg - dy * 0.2, -89.0, 89.0);
    }
    // Description: move along the WORLD axes (as the assignment asks), not along where the camera looks.
    void translate_world(const dvec3& delta) { position += delta; }
};

// Description: camera that circles a target (used to inspect the rocket). Mouse drags, wheel zooms.
class OrbitCamera {
public:
    dvec3 target{0.0, 22.0, 0.0};
    double distance = 90.0, yaw_deg = 35.0, pitch_deg = 12.0;

    dvec3 position() const {
        double y = deg_to_rad(yaw_deg), p = deg_to_rad(pitch_deg);
        return target + dvec3(std::cos(p) * std::sin(y), std::sin(p), std::cos(p) * std::cos(y)) * distance;
    }
    dvec3 forward() const { return glm::normalize(target - position()); }
    void rotate_with_mouse(double dx, double dy) {
        yaw_deg -= dx * 0.3;
        pitch_deg = glm::clamp(pitch_deg + dy * 0.3, -89.0, 89.0);
    }
    void zoom(double scroll) { distance = glm::clamp(distance * std::pow(0.9, scroll), 3.0, 4000.0); }
};

// Description: everything the draw code needs for one frame.
struct FrameContext {
    dvec3 camera_position{0.0};
    glm::mat4 view{1.0f};           // rotation only
    glm::mat4 proj_space{1.0f};     // for planets: near plane far away, far plane very far
    glm::mat4 proj_local{1.0f};     // for pad and rocket: near plane close to the camera
    glm::vec3 sun_dir{1, 0, 0};
    float time_seconds = 0.0f;
    double altitude = 0.0;          // camera height above the Earth's surface
};

// Description: builds the matrices for one frame. Two projections are used because Earth is millions of
// metres across while the rocket is 49 m tall: no single near/far pair can hold both.
// In: camera position/forward/up, field of view, aspect, distance to the nearest pad/rocket thing, sun, time.
inline FrameContext make_frame_context(const dvec3& cam_pos, const dvec3& forward, const dvec3& up, float fov_deg,
                                       float aspect, double nearest_local_distance, const glm::vec3& sun_dir, float time_seconds) {
    FrameContext ctx;
    ctx.camera_position = cam_pos;
    ctx.view = glm::lookAt(glm::vec3(0.0f), glm::vec3(forward), glm::vec3(up));
    ctx.sun_dir = sun_dir;
    ctx.time_seconds = time_seconds;
    ctx.altitude = glm::max(1.0, glm::length(cam_pos) - EARTH_RADIUS_M);
    double near_space = glm::max(1.0, 0.3 * ctx.altitude);
    ctx.proj_space = glm::perspective(glm::radians(fov_deg), aspect, (float)near_space, 1.0e10f);
    double near_local = glm::max(0.3, 0.02 * nearest_local_distance);
    double far_local = glm::max(near_local * 1.0e5, 3.0e5);
    ctx.proj_local = glm::perspective(glm::radians(fov_deg), aspect, (float)near_local, (float)far_local);
    return ctx;
}

// Description: speed that feels right: slow near the ground, fast far away. In: camera position.
inline double fly_speed_for(const dvec3& position, double minimum_speed = 3.0) {
    double altitude = glm::length(position) - EARTH_RADIUS_M;
    return glm::max(minimum_speed, altitude * 0.6);
}

// Description: assignment keys for the Earth/scene camera: arrows = X and Y, [ and ] = Z (world axes).
// Mouse drag turns the view. In: window, camera, speed in metres per second.
inline void handle_fly_keys(const AppWindow& win, FlyCamera& cam, double speed) {
    double step = speed * win.delta_time;
    if (win.is_key_down(GLFW_KEY_RIGHT)) cam.translate_world(dvec3(step, 0, 0));
    if (win.is_key_down(GLFW_KEY_LEFT)) cam.translate_world(dvec3(-step, 0, 0));
    if (win.is_key_down(GLFW_KEY_UP)) cam.translate_world(dvec3(0, step, 0));
    if (win.is_key_down(GLFW_KEY_DOWN)) cam.translate_world(dvec3(0, -step, 0));
    if (win.is_key_down(GLFW_KEY_RIGHT_BRACKET)) cam.translate_world(dvec3(0, 0, step));   // ]  towards +Z
    if (win.is_key_down(GLFW_KEY_LEFT_BRACKET)) cam.translate_world(dvec3(0, 0, -step));   // [  towards -Z
    if (win.is_dragging()) cam.rotate_with_mouse(win.mouse_dx, win.mouse_dy);
}
