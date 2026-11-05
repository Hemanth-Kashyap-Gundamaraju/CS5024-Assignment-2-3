// app_window.h - window, keyboard, mouse and frame timing shared by all four programs.
// Also supports test flags:  --shot file.ppm  (save a picture and quit)  --frames N
#pragma once
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "gl_utils.h"

class AppWindow {
public:
    GLFWwindow* handle = nullptr;
    double mouse_dx = 0, mouse_dy = 0;   // mouse movement this frame while left button is held
    double scroll_y = 0;                 // scroll wheel this frame
    double delta_time = 0.016;           // seconds since last frame
    double total_time = 0;               // seconds since start
    int width = WINDOW_WIDTH, height = WINDOW_HEIGHT;
    int argc = 0; char** argv = nullptr;

    // Description: opens the window and OpenGL 3.3 core. Out: false if it failed.
    bool init(const char* title, int arg_count, char** arg_values) {
        argc = arg_count; argv = arg_values;
        if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return false; }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        handle = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, title, nullptr, nullptr);
        if (!handle) { std::cerr << "Could not create window\n"; return false; }
        glfwMakeContextCurrent(handle);
        glfwSwapInterval(1);
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) { std::cerr << "glewInit failed\n"; return false; }
        glGetError();  // glewInit can leave a harmless error
        glfwSetWindowUserPointer(handle, this);
        glfwSetKeyCallback(handle, _key_callback);
        glfwSetScrollCallback(handle, _scroll_callback);
        glfwSetCursorPosCallback(handle, _cursor_callback);
        glfwSetMouseButtonCallback(handle, _mouse_button_callback);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_PROGRAM_POINT_SIZE);
        std::cout << "OpenGL " << glGetString(GL_VERSION) << " | " << glGetString(GL_RENDERER) << "\n";
        shot_file_ = get_arg("--shot");
        max_frames_ = get_arg("--frames") ? std::atoi(get_arg("--frames")) : (shot_file_ ? 3 : 0);
        return true;
    }

    // Description: value after a command-line flag, or nullptr.  In: flag name.
    const char* get_arg(const char* flag) const {
        for (int i = 1; i + 1 < argc; i++) if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
        return nullptr;
    }
    bool has_flag(const char* flag) const {
        for (int i = 1; i < argc; i++) if (std::strcmp(argv[i], flag) == 0) return true;
        return false;
    }

    bool is_open() const { return !glfwWindowShouldClose(handle); }
    // Description: true while the key is held.
    bool is_key_down(int key) const { return glfwGetKey(handle, key) == GLFW_PRESS; }
    // Description: true once, in the frame the key was pressed.
    bool was_key_pressed(int key) const { return key_pressed_[key]; }
    bool is_dragging() const { return dragging_; }

    // Description: start of frame - reads input, updates timing. Call first in the loop.
    void begin_frame() {
        mouse_dx = mouse_dy = 0; scroll_y = 0;
        key_pressed_.fill(false);
        glfwPollEvents();
        double now = glfwGetTime();
        delta_time = glm::clamp(now - last_time_, 0.0001, 0.1);
        last_time_ = now;
        total_time = now;
        glfwGetFramebufferSize(handle, &width, &height);
        if (height < 1) height = 1;
        mouse_dx = pending_dx_; mouse_dy = pending_dy_;
        scroll_y = pending_scroll_;
        pending_dx_ = pending_dy_ = pending_scroll_ = 0;
    }

    float aspect() const { return (float)width / (float)height; }

    // Description: end of frame - saves a screenshot if asked, then swaps buffers.
    void end_frame() {
        frame_count_++;
        if (shot_file_ && frame_count_ >= max_frames_) { _save_ppm(shot_file_); glfwSetWindowShouldClose(handle, 1); }
        else if (!shot_file_ && max_frames_ > 0 && frame_count_ >= max_frames_) glfwSetWindowShouldClose(handle, 1);
        glfwSwapBuffers(handle);
    }
    int frame_count() const { return frame_count_; }

private:
    std::array<bool, GLFW_KEY_LAST + 1> key_pressed_{};
    bool dragging_ = false;
    double last_x_ = 0, last_y_ = 0, pending_dx_ = 0, pending_dy_ = 0, pending_scroll_ = 0, last_time_ = 0;
    const char* shot_file_ = nullptr;
    int max_frames_ = 0, frame_count_ = 0;

    void _save_ppm(const char* file) {
        std::vector<unsigned char> px((size_t)width * height * 3);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, px.data());
        std::ofstream out(file, std::ios::binary);
        out << "P6\n" << width << " " << height << "\n255\n";
        for (int y = height - 1; y >= 0; y--) out.write((char*)&px[(size_t)y * width * 3], (std::streamsize)width * 3);
        std::cout << "Saved screenshot " << file << "\n";
    }
    static AppWindow* _self(GLFWwindow* w) { return (AppWindow*)glfwGetWindowUserPointer(w); }
    static void _key_callback(GLFWwindow* w, int key, int, int action, int) {
        if (key < 0 || key > GLFW_KEY_LAST) return;
        if (action == GLFW_PRESS) _self(w)->key_pressed_[key] = true;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(w, 1);
    }
    static void _scroll_callback(GLFWwindow* w, double, double dy) { _self(w)->pending_scroll_ += dy; }
    static void _mouse_button_callback(GLFWwindow* w, int button, int action, int) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            AppWindow* a = _self(w);
            a->dragging_ = (action == GLFW_PRESS);
            glfwGetCursorPos(w, &a->last_x_, &a->last_y_);
        }
    }
    static void _cursor_callback(GLFWwindow* w, double x, double y) {
        AppWindow* a = _self(w);
        if (a->dragging_) { a->pending_dx_ += x - a->last_x_; a->pending_dy_ += y - a->last_y_; }
        a->last_x_ = x; a->last_y_ = y;
    }
};
