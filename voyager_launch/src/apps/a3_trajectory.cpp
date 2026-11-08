#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include "app_window.h"
#include "renderer.h"
#include "trajectory_io.h"
#include "keyframes.h"
#include "mission_paths.h"

using trajectory::Point;
using trajectory::MissionPath;

namespace {
constexpr float FOV = 50.0f;
constexpr unsigned CURVE_SEGMENTS = 300;
const glm::vec4 CURVE_COLOR(0.15f, 0.85f, 1.0f, 1.0f);
const glm::vec4 POLYGON_COLOR(0.8f, 0.8f, 0.8f, 1.0f);
const glm::vec4 POINT_COLOR(1.0f, 0.75f, 0.15f, 1.0f);
const glm::vec4 SELECTED_COLOR(1.0f, 0.2f, 0.25f, 1.0f);

glm::vec3 relative(const Point& p, const Point& eye) {
    return glm::vec3(p - eye);
}

void draw_line(SceneRenderer& r, PointBuffer& buffer,
               const std::vector<Point>& points, const Point& eye,
               const glm::vec4& color, GLenum mode = GL_LINE_STRIP) {
    std::vector<glm::vec3> v;
    v.reserve(points.size());
    for (const auto& p : points) v.push_back(relative(p, eye));
    buffer.update(v);
    r.draw_points(buffer, mode, color, 2.0f);
}

void draw_point(SceneRenderer& r, PointBuffer& buffer, const Point& p,
                const Point& eye, const glm::vec4& color, float size) {
    buffer.update({relative(p, eye)});
    r.draw_points(buffer, GL_POINTS, color, size);
}

Point mouse_to_world(double mx, double my, int width, int height,
                     const Point& target, double distance) {
    const double aspect = static_cast<double>(width) / std::max(height, 1);
    const double half_h = distance * std::tan(glm::radians(static_cast<double>(FOV) * 0.5));
    const double nx = 2.0 * mx / std::max(width, 1) - 1.0;
    const double ny = 1.0 - 2.0 * my / std::max(height, 1);
    return {target.x + nx * half_h * aspect, target.y + ny * half_h, 0.0};
}
}

int main(int argc, char** argv) {
    AppWindow win;
    if (!win.init("Assignment 3 - Voyager trajectory editor", argc, argv)) return 1;
    SceneRenderer renderer;
    renderer.init();

    MissionPath current = trajectory::voyager_one_path();
    std::vector<std::vector<Point>> saved;
    std::vector<trajectory::Keyframe> keyframes;
    std::string message = "Voyager 1 | left-click select/drag, right-click add";
    std::string error;
    int selected = -1;
    int mission_index = 1;
    bool playing = false;
    double playback_time = 0.0;
    double playback_duration = 20.0;
    double camera_distance = 160.0;
    Point camera_target(35.0, 0.0, 0.0);
    Point camera_eye(camera_target.x, camera_target.y, camera_distance);
    bool previous_left = false, previous_right = false;
    PointBuffer curve_buffer, polygon_buffer, points_buffer, selected_buffer;
    PointBuffer saved_buffer, ship_buffer;

    auto select_mission = [&](int index) {
        mission_index = index;
        current = index == 1 ? trajectory::voyager_one_path() : trajectory::voyager_two_path();
        selected = -1;
        playing = false;
        keyframes.clear();
        const std::string name = index == 1 ? "Voyager1" : "Voyager2";
        if (trajectory::load_keyframes("keyframes.txt", name, keyframes, error)) {
            playback_duration = keyframes.back().time;
            message = current.name + " loaded. " + std::to_string(keyframes.size()) + " keyframes.";
        }
        else
            message = current.name + " loaded; keyframes unavailable: " + error;
    };
    select_mission(1);

    while (win.is_open()) {
        win.begin_frame();
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(win.handle, &mx, &my);
        const bool left = glfwGetMouseButton(win.handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool right = glfwGetMouseButton(win.handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        const Point mouse_world = mouse_to_world(mx, my, win.width, win.height,
                                                  camera_target, camera_distance);

        if (win.was_key_pressed(GLFW_KEY_1)) select_mission(1);
        if (win.was_key_pressed(GLFW_KEY_2)) select_mission(2);
        if (win.was_key_pressed(GLFW_KEY_T)) {
            if (trajectory::append_trajectory("trajectory.txt", current.control_points, error))
                message = "Appended control points to trajectory.txt";
            else message = error;
        }
        if (win.was_key_pressed(GLFW_KEY_Y)) {
            if (trajectory::load_trajectories("trajectory.txt", saved, error))
                message = "Loaded " + std::to_string(saved.size()) + " saved trajectories";
            else message = error;
        }
        if (win.was_key_pressed(GLFW_KEY_P)) {
            if (keyframes.empty()) message = "No keyframes loaded; check keyframes.txt";
            else { playing = !playing; playback_time = 0.0; message = playing ? "Playback started" : "Playback paused"; }
        }
        if (win.was_key_pressed(GLFW_KEY_R)) { playing = false; playback_time = 0.0; }
        if (win.was_key_pressed(GLFW_KEY_DELETE) && selected >= 0 &&
            current.control_points.size() > 2) {
            current.control_points.erase(current.control_points.begin() + selected);
            selected = -1;
            message = "Control point deleted";
        }

        // Arrow keys pan the editor camera. WASD moves the selected control point.
        const double pan = 35.0 * win.delta_time;
        if (win.is_key_down(GLFW_KEY_LEFT)) camera_target.x -= pan;
        if (win.is_key_down(GLFW_KEY_RIGHT)) camera_target.x += pan;
        if (win.is_key_down(GLFW_KEY_UP)) camera_target.y += pan;
        if (win.is_key_down(GLFW_KEY_DOWN)) camera_target.y -= pan;
        if (win.was_key_pressed(GLFW_KEY_EQUAL)) camera_distance = std::max(20.0, camera_distance * 0.9);
        if (win.was_key_pressed(GLFW_KEY_MINUS)) camera_distance = std::min(500.0, camera_distance * 1.1);

        if (selected >= 0 && selected < static_cast<int>(current.control_points.size())) {
            Point& p = current.control_points[static_cast<std::size_t>(selected)];
            const double move = 18.0 * win.delta_time;
            if (win.is_key_down(GLFW_KEY_A)) p.x -= move;
            if (win.is_key_down(GLFW_KEY_D)) p.x += move;
            if (win.is_key_down(GLFW_KEY_W)) p.y += move;
            if (win.is_key_down(GLFW_KEY_S)) p.y -= move;
            if (win.is_key_down(GLFW_KEY_Q)) p.z += move;
            if (win.is_key_down(GLFW_KEY_E)) p.z -= move;
        }

        if (left && !previous_left && !playing) {
            selected = -1;
            double best = 2.5;
            for (std::size_t i = 0; i < current.control_points.size(); ++i) {
                const Point d = current.control_points[i] - mouse_world;
                const double dist = std::sqrt(d.x*d.x + d.y*d.y);
                if (dist < best) { best = dist; selected = static_cast<int>(i); }
            }
            if (selected >= 0) message = "Selected control point " + std::to_string(selected);
        }
        if (left && selected >= 0 && !playing) {
            current.control_points[static_cast<std::size_t>(selected)].x = mouse_world.x;
            current.control_points[static_cast<std::size_t>(selected)].y = mouse_world.y;
        }
        if (right && !previous_right && !playing) {
            current.control_points.push_back(mouse_world);
            selected = static_cast<int>(current.control_points.size() - 1);
            message = "Added control point";
        }
        previous_left = left;
        previous_right = right;

        Point ship(0.0);
        if (playing) {
            playback_time += win.delta_time;
            if (playback_time > playback_duration) {
                playback_time = playback_duration;
                playing = false;
            }
            const auto state = trajectory::evaluate_keyframes(keyframes, playback_time);
            camera_eye = state.eye;
            camera_target = state.target;
            if (state.active && current.control_points.size() >= 2)
                ship = trajectory::evaluate_de_casteljau(current.control_points, state.parameter);
        } else {
            camera_eye = Point(camera_target.x, camera_target.y, camera_distance);
        }

        const Point forward = glm::normalize(camera_target - camera_eye);
        const glm::mat4 view = glm::lookAt(glm::vec3(0.0f),
            glm::vec3(forward), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 projection = glm::perspective(glm::radians(FOV),
            win.aspect(), 0.1f, 1000.0f);
        renderer.set_pass(view, projection, glm::normalize(glm::vec3(1.0f, 0.7f, 0.4f)));

        glViewport(0, 0, win.width, win.height);
        glClearColor(0.025f, 0.035f, 0.075f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        // A subtle XY reference grid.
        std::vector<Point> grid;
        for (int i = -100; i <= 100; i += 10) {
            grid.push_back({static_cast<double>(i), -100, -2});
            grid.push_back({static_cast<double>(i), 100, -2});
            grid.push_back({-100, static_cast<double>(i), -2});
            grid.push_back({100, static_cast<double>(i), -2});
        }
        draw_line(renderer, polygon_buffer, grid, camera_eye,
                  glm::vec4(0.13f, 0.17f, 0.24f, 1.0f), GL_LINES);

        for (std::size_t i = 0; i < saved.size(); ++i) {
            const glm::vec4 colors[] = {
                {0.85f,0.35f,0.8f,1}, {0.3f,0.8f,0.45f,1},
                {0.95f,0.55f,0.2f,1}, {0.4f,0.6f,1,1}
            };
            draw_line(renderer, saved_buffer, trajectory::sample_curve(saved[i], 150),
                      camera_eye, colors[i % 4]);
        }

        if (current.control_points.size() >= 2) {
            draw_line(renderer, curve_buffer,
                trajectory::sample_curve(current.control_points, CURVE_SEGMENTS),
                camera_eye, CURVE_COLOR);
            draw_line(renderer, polygon_buffer, current.control_points,
                      camera_eye, POLYGON_COLOR);
        }
        for (std::size_t i = 0; i < current.control_points.size(); ++i) {
            const bool is_selected = static_cast<int>(i) == selected && !playing;
            draw_point(renderer, is_selected ? selected_buffer : points_buffer,
                current.control_points[i], camera_eye,
                is_selected ? SELECTED_COLOR : POINT_COLOR,
                is_selected ? 13.0f : 9.0f);
        }
        if (playing) draw_point(renderer, ship_buffer, ship, camera_eye,
                                glm::vec4(1.0f, 1.0f, 0.35f, 1.0f), 16.0f);

        std::string title = "A3 | " + current.name +
            " | T save | Y load | P play/pause | 1/2 mission | " + message;
        glfwSetWindowTitle(win.handle, title.c_str());
        win.end_frame();
    }
    glfwTerminate();
    return 0;
}
