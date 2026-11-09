// Interactive 3D Bezier editor, schematic mission playback, planetary flybys and HUD.
// Mission geometry is intentionally schematic, NOT an ephemeris or to-scale solar system.
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include "app_window.h"
#include "renderer.h"
#include "procedural_textures.h"
#include "trajectory_io.h"
#include "keyframes.h"
#include "mission_paths.h"

using trajectory::Point;
using trajectory::MissionPath;

namespace {
constexpr float FOV = 48.0f;
constexpr unsigned CURVE_SEGMENTS = 420;
const glm::vec4 WHITE(0.88f, 0.94f, 1.0f, 1.0f);
const glm::vec4 CONTROL(1.0f, 0.72f, 0.22f, 1.0f);
const glm::vec4 SELECTED(1.0f, 0.24f, 0.30f, 1.0f);
const glm::vec4 CYAN(0.20f, 0.88f, 1.0f, 1.0f);
const glm::vec4 VIOLET(0.74f, 0.48f, 1.0f, 1.0f);
const glm::vec4 GREEN(0.35f, 1.0f, 0.66f, 1.0f);

struct Flyby { const char* name; double u; float radius; glm::vec3 color; const char* detail; };
const std::vector<Flyby> VOYAGER1_FLYBYS = {
    {"EARTH", 0.025, 2.4f, {0.18f,0.48f,1.0f}, "Launch / departure"},
    {"JUPITER", 0.31, 5.3f, {0.96f,0.38f,0.12f}, "Jupiter flyby - March 1979"},
    {"SATURN", 0.53, 4.5f, {0.96f,0.76f,0.22f}, "Saturn flyby - November 1980"},
    {"TITAN", 0.60, 1.55f, {1.00f,0.43f,0.12f}, "Titan flyby - November 1980"},
    {"HELIOPAUSE", 0.93, 2.0f, {0.88f,0.94f,1.0f}, "Crossed heliopause - August 2012"}
};
const std::vector<Flyby> VOYAGER2_FLYBYS = {
    {"EARTH", 0.025, 2.4f, {0.18f,0.48f,1.0f}, "Launch / departure"},
    {"JUPITER", 0.25, 5.1f, {0.96f,0.38f,0.12f}, "Jupiter flyby - July 1979"},
    {"SATURN", 0.42, 4.4f, {0.96f,0.76f,0.22f}, "Saturn flyby - August 1981"},
    {"URANUS", 0.65, 3.8f, {0.10f,0.78f,0.76f}, "Uranus flyby - January 1986"},
    {"NEPTUNE", 0.80, 3.8f, {0.24f,0.38f,1.0f}, "Neptune flyby - August 1989"},
    {"HELIOPAUSE", 0.96, 2.0f, {0.88f,0.94f,1.0f}, "Crossed heliopause - November 2018"}
};

Point relative(const Point& p, const Point& eye) { return Point(p - eye); }

void draw_line(SceneRenderer& r, PointBuffer& b, const std::vector<Point>& pts,
               const Point& eye, const glm::vec4& color, GLenum mode = GL_LINE_STRIP,
               float width = 2.0f) {
    std::vector<glm::vec3> v; v.reserve(pts.size());
    for (const auto& p : pts) v.emplace_back(glm::vec3(p - eye));
    b.update(v); r.draw_points(b, mode, color, width);
}
void draw_point(SceneRenderer& r, PointBuffer& b, const Point& p, const Point& eye,
                const glm::vec4& color, float size) {
    b.update({glm::vec3(p - eye)}); r.draw_points(b, GL_POINTS, color, size);
}

glm::mat4 model_at(const Point& p, const Point& eye, const glm::vec3& scale) {
    return glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(p-eye)), scale);
}

bool project_to_screen(const Point& p, const Point& eye, const Point& target,
                       int width, int height, double& sx, double& sy) {
    Point forward=glm::normalize(target-eye), up(0,1,0);
    if (std::abs(glm::dot(forward,up))>0.985) up=Point(0,0,1);
    const glm::mat4 view=glm::lookAt(glm::vec3(0),glm::vec3(forward),glm::vec3(up));
    const glm::mat4 proj=glm::perspective(glm::radians(FOV),static_cast<float>(width)/std::max(height,1),0.1f,1200.0f);
    const glm::vec4 clip=proj*view*glm::vec4(glm::vec3(p-eye),1.0f);
    if (clip.w<=0.001f) return false;
    const glm::vec3 ndc=glm::vec3(clip)/clip.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
    sx=(ndc.x*0.5+0.5)*width;
    sy=(1.0-(ndc.y*0.5+0.5))*height;
    return sx>=0 && sx<=width && sy>=0 && sy<=height;
}

Point mouse_to_world(double mx, double my, int width, int height,
                     const Point& eye, const Point& target, double distance) {
    const double aspect = static_cast<double>(width) / std::max(height, 1);
    const double nx = 2.0 * mx / std::max(width, 1) - 1.0;
    const double ny = 1.0 - 2.0 * my / std::max(height, 1);
    const Point forward = glm::normalize(target - eye);
    Point right = glm::cross(forward, Point(0,1,0));
    if (glm::length(right) < 1e-8) right = Point(1,0,0);
    right = glm::normalize(right);
    const Point up = glm::normalize(glm::cross(right, forward));
    const double half_h = distance * std::tan(glm::radians(static_cast<double>(FOV) * 0.5));
    const Point ray = glm::normalize(forward + right * (nx * half_h * aspect / distance)
                                            + up * (ny * half_h / distance));
    const double denom = glm::dot(ray, forward);
    if (std::abs(denom) < 1e-8) return target;
    return eye + ray * (glm::dot(target-eye, forward) / denom);
}

// Compact 5x7 pixel font for the on-screen instrument panel. Only points are used,
// so the HUD has no external font dependency and works in the existing OpenGL renderer.
std::array<unsigned char,7> glyph(char c) {
    switch (c) {
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case ':': return {0,4,4,0,4,4,0}; case '.': return {0,0,0,0,0,6,6};
    case '-': return {0,0,0,31,0,0,0}; case '/': return {1,2,2,4,8,8,16};
    case '+': return {0,4,4,31,4,4,0}; case '%': return {17,2,4,8,17,0,0};
    case '|': return {4,4,4,4,4,4,4}; case '>': return {16,8,4,2,4,8,16};
    case '<': return {1,2,4,8,4,2,1}; case '_': return {0,0,0,0,0,0,31};
    default: return {0,0,0,0,0,0,0};
    }
}

void hud_rect(SceneRenderer& r, PointBuffer& b, int w, int h,
              float x, float y, float rw, float rh, const glm::vec4& color) {
    b.update({{x,y,0},{x+rw,y,0},{x+rw,y+rh,0},{x,y+rh,0}});
    r.draw_points(b, GL_TRIANGLE_FAN, color, 1.0f);
}
void hud_text(SceneRenderer& r, PointBuffer& b, int w, int h, float x, float y,
              const std::string& text, const glm::vec4& color, float pixel = 2.2f) {
    std::vector<glm::vec3> pts;
    float cx = x;
    for (char raw : text) {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        if (c == ' ') { cx += 6.0f*pixel; continue; }
        const auto g = glyph(c);
        for (int row=0; row<7; ++row)
            for (int col=0; col<5; ++col)
                if (g[row] & (1u << (4-col)))
                    pts.emplace_back(cx + col*pixel, y + (6-row)*pixel, 0.0f);
        cx += 6.0f*pixel;
    }
    b.update(pts); r.draw_points(b, GL_POINTS, color, pixel*0.95f);
}

std::vector<Point> make_stars() {
    std::vector<Point> s; s.reserve(700);
    unsigned seed = 81231u;
    auto rnd = [&]() { seed = seed*1664525u + 1013904223u; return (seed >> 8) / 16777215.0; };
    for (int i=0;i<700;++i) s.emplace_back((rnd()-.5)*380.0, (rnd()-.5)*230.0, (rnd()-.5)*260.0);
    return s;
}

void draw_spacecraft(SceneRenderer& r, const Mesh& sphere, const Mesh& box,
                     const Point& pos, const Point& eye, double phase, bool deployed) {
    // Probe bus, gold insulation, deployed solar/science panels, antenna and faint blue glint.
    const glm::mat4 base = glm::translate(glm::mat4(1), glm::vec3(pos-eye));
    r.draw_mesh(sphere, base * glm::scale(glm::mat4(1), glm::vec3(1.15f,0.78f,0.78f)),
                make_material({0.78f,0.67f,0.39f},0.8f,0.32f));
    const float wing = deployed ? 2.8f : 0.9f;
    for (int side : {-1,1}) {
        glm::mat4 panel = base * glm::translate(glm::mat4(1), glm::vec3(0.0f,0.0f,side*wing));
        panel = panel * glm::scale(glm::mat4(1), glm::vec3(1.55f,0.10f,deployed?1.05f:0.48f));
        r.draw_mesh(box, panel, make_material({0.12f,0.28f,0.58f},0.65f,0.26f));
        // thin bright panel highlight
        glm::mat4 stripe = base * glm::translate(glm::mat4(1), glm::vec3(0.0f,0.08f,side*wing));
        stripe = stripe * glm::scale(glm::mat4(1), glm::vec3(1.45f,0.025f,0.035f));
        r.draw_mesh(box, stripe, make_material({0.22f,0.78f,1.0f},0.5f,0.5f));
    }
    glm::mat4 antenna = base * glm::translate(glm::mat4(1), glm::vec3(-0.7f,0.9f,0.0f));
    antenna = antenna * glm::rotate(glm::mat4(1), glm::radians(28.0f + 3.0f*static_cast<float>(std::sin(phase))), glm::vec3(0,0,1));
    antenna = antenna * glm::scale(glm::mat4(1), glm::vec3(0.65f,0.10f,0.65f));
    r.draw_mesh(sphere, antenna, make_material({0.85f,0.88f,0.92f},0.85f,0.42f));
}

// Procedural lat/lon maps for the A3 solar-system waypoints. These are deliberately
// illustrative textures, not scientific surface maps. They are generated once after
// the OpenGL context exists, so A3 works even when the assets/ folder is empty.
glm::vec3 jupiter_pixel(float u, float v) {
    const float lat = (0.5f-v)*3.14159265f;
    const float n = fractal_noise(glm::vec3(u*9.0f, v*15.0f, 2.0f), 4);
    const float bands = 0.5f + 0.5f*std::sin(lat*34.0f + 1.2f*std::sin(u*18.0f) + (n-0.5f)*2.0f);
    const glm::vec3 cream(0.82f,0.69f,0.49f), ochre(0.58f,0.32f,0.16f), pale(0.94f,0.82f,0.63f);
    glm::vec3 c = glm::mix(ochre, cream, glm::smoothstep(0.28f,0.72f,bands));
    c = glm::mix(c,pale,0.22f*n);
    const float storm = std::exp(-std::pow((u-0.72f)*18.0f,2.0f)-std::pow((v-0.58f)*34.0f,2.0f));
    return glm::mix(c,glm::vec3(0.72f,0.24f,0.12f),storm*0.85f);
}

glm::vec3 saturn_pixel(float u, float v) {
    const float lat = (0.5f-v)*3.14159265f;
    const float n = fractal_noise(glm::vec3(u*5.0f,v*18.0f,7.0f),3);
    const float bands = 0.5f+0.5f*std::sin(lat*43.0f + 0.8f*std::sin(u*8.0f));
    const glm::vec3 dark(0.55f,0.43f,0.29f), light(0.91f,0.80f,0.61f);
    return glm::mix(dark,light,glm::smoothstep(0.22f,0.78f,bands))* (0.88f+0.18f*n);
}

glm::vec3 titan_pixel(float u, float v) {
    const float n=fractal_noise(glm::vec3(u*8.0f,v*12.0f,5.0f),5);
    const float haze=0.5f+0.5f*std::sin((v*3.14159265f)*18.0f+n*2.0f);
    return glm::mix(glm::vec3(0.42f,0.20f,0.08f),glm::vec3(0.91f,0.55f,0.22f),0.45f+0.35f*haze+0.15f*n);
}

glm::vec3 uranus_pixel(float u, float v) {
    const float lat=(0.5f-v)*3.14159265f;
    const float n=fractal_noise(glm::vec3(u*4.0f,v*9.0f,11.0f),3);
    const float band=0.5f+0.5f*std::sin(lat*24.0f+n*1.4f);
    return glm::mix(glm::vec3(0.22f,0.58f,0.65f),glm::vec3(0.58f,0.85f,0.86f),0.35f+0.45f*band);
}

glm::vec3 neptune_pixel(float u, float v) {
    const float lat=(0.5f-v)*3.14159265f;
    const float n=fractal_noise(glm::vec3(u*7.0f,v*13.0f,17.0f),4);
    const float band=0.5f+0.5f*std::sin(lat*30.0f+n*2.0f);
    glm::vec3 c=glm::mix(glm::vec3(0.025f,0.10f,0.42f),glm::vec3(0.12f,0.42f,0.86f),0.35f+0.55f*band);
    const float storm=std::exp(-std::pow((u-0.34f)*24.0f,2.0f)-std::pow((v-0.43f)*28.0f,2.0f));
    return glm::mix(c,glm::vec3(0.12f,0.18f,0.35f),storm*0.75f);
}

struct A3PlanetTextures {
    GLuint earth=0, jupiter=0, saturn=0, titan=0, uranus=0, neptune=0;
    void create() {
        earth=make_texture_from_function(512,256,earth_pixel);
        jupiter=make_texture_from_function(512,256,jupiter_pixel);
        saturn=make_texture_from_function(512,256,saturn_pixel);
        titan=make_texture_from_function(512,256,titan_pixel);
        uranus=make_texture_from_function(512,256,uranus_pixel);
        neptune=make_texture_from_function(512,256,neptune_pixel);
    }
    GLuint for_name(const char* name) const {
        const std::string n(name);
        if(n=="EARTH") return earth;
        if(n=="JUPITER") return jupiter;
        if(n=="SATURN") return saturn;
        if(n=="TITAN") return titan;
        if(n=="URANUS") return uranus;
        if(n=="NEPTUNE") return neptune;
        return 0;
    }
    void destroy() {
        const GLuint ids[]={earth,jupiter,saturn,titan,uranus,neptune};
        glDeleteTextures(6,ids);
        earth=jupiter=saturn=titan=uranus=neptune=0;
    }
};

// Animated layered corona around the Sun. The shells use additive blending so
// the Sun reads as a luminous object instead of an ordinary orange planet.
void draw_sun_corona(SceneRenderer& r, const Mesh& sphere,
                     const Point& sun, const Point& eye, float radius, double time) {
    const float pulse = 1.0f + 0.035f * std::sin(static_cast<float>(time) * 2.4f)
                              + 0.018f * std::sin(static_cast<float>(time) * 5.7f);
    const glm::mat4 base = glm::translate(glm::mat4(1.0f), glm::vec3(sun - eye));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    // Draw outer-to-inner translucent shells; each is slightly larger and softer.
    const float scales[] = {1.72f, 1.48f, 1.27f};
    const glm::vec3 colors[] = {
        {1.0f, 0.16f, 0.025f},
        {1.0f, 0.34f, 0.045f},
        {1.0f, 0.66f, 0.16f}
    };
    const float alphas[] = {0.075f, 0.095f, 0.13f};
    for (int i = 0; i < 3; ++i) {
        const float s = radius * scales[i] * pulse;
        glm::mat4 model = base * glm::scale(glm::mat4(1.0f), glm::vec3(s));
        Material glow = make_material(colors[i], 0.0f, 1.0f);
        glow.alpha = alphas[i];
        r.draw_mesh(sphere, model, glow);
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

void draw_planet(SceneRenderer& r, const Mesh& sphere, PointBuffer& ring_buffer,
                 const Point& pos, const Point& eye, float radius,
                 const glm::vec3& color, bool ring, double phase, float spin_rate, GLuint texture=0) {
    const glm::mat4 m = glm::translate(glm::mat4(1), glm::vec3(pos-eye));
    const float spin = static_cast<float>(phase * spin_rate);
    const glm::mat4 planet_model = m * glm::rotate(glm::mat4(1),spin,glm::vec3(0,1,0))
                                     * glm::scale(glm::mat4(1), glm::vec3(radius));
    Material material=make_material(texture?glm::vec3(1.0f):color,0.55f,0.24f);
    material.texture=texture;
    r.draw_mesh(sphere,planet_model,material);
    if (ring) {
        std::vector<Point> ringline; ringline.reserve(97);
        for (int i=0;i<=96;++i) {
            const double a = glm::two_pi<double>()*i/96.0;
            ringline.emplace_back(pos.x + radius*1.55*std::cos(a), pos.y + radius*0.18*std::sin(a), pos.z + radius*1.55*std::sin(a));
        }
        // Ring is drawn as a true world-space loop, not a 2D screen decoration.
        draw_line(r, ring_buffer, ringline, eye, glm::vec4(0.95f,0.79f,0.48f,1), GL_LINE_STRIP, 2.0f);
    }
}
}

int main(int argc, char** argv) {
    AppWindow win;
    if (!win.init("VOYAGER MISSION CONTROL | 3D TRAJECTORY EXPLORER", argc, argv)) return 1;
    SceneRenderer renderer; renderer.init();
    A3PlanetTextures planet_textures;
    planet_textures.create();

    Mesh sphere = make_sphere(48, 72);
    Mesh box = make_box(1.0f,1.0f,1.0f);
    Mesh stage_mesh = make_cylinder(0.62f,0.44f,2.8f,24,true);
    MissionPath current = trajectory::voyager_one_path();
    std::vector<std::vector<Point>> saved;
    std::vector<trajectory::Keyframe> keyframes;
    std::string message = "READY - SELECT A MISSION OR PRESS P TO PLAY";
    std::string error;
    int selected = -1, mission_index = 1;
    bool playing = false, show_help = true, follow_camera = true;
    double playback_time = 0.0, playback_duration = 24.0, playback_speed = 1.0;
    double camera_distance = 155.0, camera_yaw = 0.10, camera_pitch = 0.12;
    Point camera_target(35.0, 5.0, 0.0), camera_eye(35.0, 5.0, 155.0);
    bool previous_left = false, previous_right = false;
    PointBuffer curve_buffer, polygon_buffer, points_buffer, selected_buffer, saved_buffer;
    PointBuffer ship_buffer, star_buffer, hud_buffer, hud_text_buffer, event_buffer, exhaust_buffer;
    const std::vector<Point> stars = make_stars();

    auto select_mission = [&](int index) {
        mission_index = index;
        current = index == 1 ? trajectory::voyager_one_path() : trajectory::voyager_two_path();
        selected = -1; playing = false; playback_time = 0.0;
        keyframes.clear();
        const std::string name = index == 1 ? "Voyager1" : "Voyager2";
        if (trajectory::load_keyframes("keyframes.txt", name, keyframes, error)) {
            playback_duration = keyframes.back().time;
            message = current.name + " LOADED - " + std::to_string(keyframes.size()) + " KEYFRAMES";
        } else message = current.name + " LOADED - KEYFRAMES: " + error;
        camera_target = current.control_points[current.control_points.size()/2];
    };
    select_mission(1);
    if (trajectory::load_trajectories("trajectory.txt",saved,error))
        message = current.name + " READY | " + std::to_string(saved.size()) + " 3D TRAJECTORIES LOADED";
    else message = "TRAJECTORY LOAD WARNING: " + error;

    while (win.is_open()) {
        win.begin_frame();
        const double dt = win.delta_time;
        double mx=0.0,my=0.0; glfwGetCursorPos(win.handle,&mx,&my);
        const bool left = glfwGetMouseButton(win.handle,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
        const bool right = glfwGetMouseButton(win.handle,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
        const Point mouse_world = mouse_to_world(mx,my,win.width,win.height,camera_eye,camera_target,camera_distance);
        bool ui_click = false;
        // Timeline scrubber: click anywhere along the bar to jump to that mission time.
        const float timeline_x=std::max(390.0f,static_cast<float>(win.width)*0.34f);
        const float timeline_w=std::min(520.0f,static_cast<float>(win.width)-timeline_x-300.0f);
        if (left && !previous_left && timeline_w>120.0f) {
            const float hy=static_cast<float>(win.height)-static_cast<float>(my);
            if (hy>=174.0f && hy<=202.0f && mx>=timeline_x && mx<=timeline_x+timeline_w) {
                playback_time=std::clamp((mx-timeline_x)/timeline_w,0.0,1.0)*playback_duration;
                playing=false;
                ui_click=true;
                message="TIMELINE SCRUBBED - PLAY TO CONTINUE";
            }
        }
        // Clickable HUD controls: top row mission/playback, second row view movement.
        if (left && !previous_left) {
            const float hy = static_cast<float>(win.height) - static_cast<float>(my);
            const float bw = 78.0f, gap = 8.0f;
            const float row1x = std::max(12.0f, (win.width - (8.0f*bw + 7.0f*gap))*0.5f);
            if (hy >= 252.0f && hy <= 282.0f) {
                const int bi = static_cast<int>((static_cast<float>(mx)-row1x)/(bw+gap));
                const float bx = static_cast<float>(mx)-row1x-bi*(bw+gap);
                if (bi>=0 && bi<8 && bx>=0 && bx<=bw) {
                    ui_click=true;
                    switch (bi) {
                    case 0: select_mission(1); break;
                    case 1: select_mission(2); break;
                    case 2: if (!keyframes.empty()) { playing=!playing; if(playing && playback_time>=playback_duration) playback_time=0; message=playing?"MISSION PLAYBACK RUNNING":"PLAYBACK PAUSED"; } break;
                    case 3: playing=false; playback_time=0; message="PLAYBACK RESET"; break;
                    case 4: playback_speed=std::max(0.25,playback_speed-0.25); message="PLAYBACK SPEED DECREASED"; break;
                    case 5: playback_speed=std::min(4.0,playback_speed+0.25); message="PLAYBACK SPEED INCREASED"; break;
                    case 6: follow_camera=!follow_camera; message=follow_camera?"CINEMATIC CAMERA ON":"FREE CAMERA"; break;
                    case 7: show_help=!show_help; break;
                    }
                }
            } else if (hy >= 215.0f && hy <= 245.0f) {
                const float bw2=78.0f, gap2=8.0f;
                const float row2x=std::max(12.0f,(win.width-(6.0f*bw2+5.0f*gap2))*0.5f);
                const int bi=static_cast<int>((static_cast<float>(mx)-row2x)/(bw2+gap2));
                const float bx=static_cast<float>(mx)-row2x-bi*(bw2+gap2);
                if (bi>=0 && bi<6 && bx>=0 && bx<=bw2) {
                    ui_click=true; const double step=8.0;
                    if (bi==0) camera_target.x-=step;
                    if (bi==1) camera_target.x+=step;
                    if (bi==2) camera_target.y+=step;
                    if (bi==3) camera_target.y-=step;
                    if (bi==4) camera_distance=std::max(18.0,camera_distance*0.85);
                    if (bi==5) camera_distance=std::min(450.0,camera_distance*1.15);
                    message="VIEW MOVED WITH ON-SCREEN CONTROL";
                }
            }
        }

        if (win.was_key_pressed(GLFW_KEY_1)) select_mission(1);
        if (win.was_key_pressed(GLFW_KEY_2)) select_mission(2);
        if (win.was_key_pressed(GLFW_KEY_H)) show_help = !show_help;
        if (win.was_key_pressed(GLFW_KEY_C)) { follow_camera = !follow_camera; message = follow_camera ? "CINEMATIC CAMERA ON" : "FREE EDITOR CAMERA"; }
        if (win.was_key_pressed(GLFW_KEY_T)) {
            if (trajectory::append_trajectory("trajectory.txt",current.control_points,error)) message="CURRENT 3D CURVE APPENDED TO trajectory.txt";
            else message=error;
        }
        if (win.was_key_pressed(GLFW_KEY_Y)) {
            if (trajectory::load_trajectories("trajectory.txt",saved,error)) message="LOADED "+std::to_string(saved.size())+" TRAJECTORY BLOCKS";
            else message=error;
        }
        if (win.was_key_pressed(GLFW_KEY_P)) {
            if (keyframes.empty()) message="NO KEYFRAMES - CHECK keyframes.txt";
            else { playing=!playing; if (playing && playback_time>=playback_duration) playback_time=0.0; message=playing?"MISSION PLAYBACK RUNNING":"PLAYBACK PAUSED"; }
        }
        if (win.was_key_pressed(GLFW_KEY_SPACE)) playing=!playing;
        if (win.was_key_pressed(GLFW_KEY_R)) { playing=false; playback_time=0.0; message="PLAYBACK RESET"; }
        if (win.was_key_pressed(GLFW_KEY_LEFT_BRACKET)) playback_speed=std::max(0.25,playback_speed-0.25);
        if (win.was_key_pressed(GLFW_KEY_RIGHT_BRACKET)) playback_speed=std::min(4.0,playback_speed+0.25);
        if (win.was_key_pressed(GLFW_KEY_DELETE) && selected>=0 && current.control_points.size()>2) {
            current.control_points.erase(current.control_points.begin()+selected); selected=-1; message="CONTROL POINT DELETED";
        }
        if (win.was_key_pressed(GLFW_KEY_HOME)) { camera_yaw=0.1; camera_pitch=0.12; camera_distance=155; camera_target=Point(35,5,0); }

        const double pan=32.0*dt;
        if (win.is_key_down(GLFW_KEY_LEFT)) camera_target.x-=pan;
        if (win.is_key_down(GLFW_KEY_RIGHT)) camera_target.x+=pan;
        if (win.is_key_down(GLFW_KEY_UP)) camera_target.y+=pan;
        if (win.is_key_down(GLFW_KEY_DOWN)) camera_target.y-=pan;
        if (win.was_key_pressed(GLFW_KEY_EQUAL) || win.scroll_y>0) camera_distance=std::max(18.0,camera_distance*0.90);
        if (win.was_key_pressed(GLFW_KEY_MINUS) || win.scroll_y<0) camera_distance=std::min(450.0,camera_distance*1.10);
        if (win.is_key_down(GLFW_KEY_J)) camera_yaw-=0.9*dt;
        if (win.is_key_down(GLFW_KEY_L)) camera_yaw+=0.9*dt;
        if (win.is_key_down(GLFW_KEY_I)) camera_pitch=std::min(1.25,camera_pitch+0.8*dt);
        if (win.is_key_down(GLFW_KEY_K)) camera_pitch=std::max(-1.25,camera_pitch-0.8*dt);

        if (selected>=0 && selected<(int)current.control_points.size() && !playing) {
            Point& p=current.control_points[(std::size_t)selected]; const double move=15.0*dt;
            if (win.is_key_down(GLFW_KEY_A)) p.x-=move;
            if (win.is_key_down(GLFW_KEY_D)) p.x+=move;
            if (win.is_key_down(GLFW_KEY_W)) p.y+=move;
            if (win.is_key_down(GLFW_KEY_S)) p.y-=move;
            if (win.is_key_down(GLFW_KEY_Q)) p.z+=move;
            if (win.is_key_down(GLFW_KEY_E)) p.z-=move;
        }
        if (left && !previous_left && !playing && !ui_click) {
            // Select in pixel space, so points at different depths remain easy to pick.
            selected=-1; double best=20.0;
            for (std::size_t i=0;i<current.control_points.size();++i) {
                double px=0.0,py=0.0;
                if (!project_to_screen(current.control_points[i],camera_eye,camera_target,win.width,win.height,px,py)) continue;
                const double dx=px-mx, dy=py-my, dist=std::sqrt(dx*dx+dy*dy);
                if (dist<best) { best=dist; selected=(int)i; }
            }
            if (selected>=0) message="SELECTED CONTROL POINT "+std::to_string(selected+1);
        }
        if (left && selected>=0 && !playing && !ui_click) current.control_points[(std::size_t)selected]=mouse_world;
        if (right && !previous_right && !playing) { current.control_points.push_back(mouse_world); selected=(int)current.control_points.size()-1; message="NEW 3D CONTROL POINT ADDED"; }
        previous_left=left; previous_right=right;

        trajectory::KeyframeState state;
        Point ship=current.control_points.front();
        if (playing) {
            playback_time+=dt*playback_speed;
            if (playback_time>playback_duration) { playback_time=playback_duration; playing=false; message="MISSION TIMELINE COMPLETE - PRESS P TO REPLAY"; }
        }
        if (!keyframes.empty()) {
            state=trajectory::evaluate_keyframes(keyframes,playback_time);
            if (state.active && current.control_points.size()>=2) ship=trajectory::evaluate_de_casteljau(current.control_points,state.parameter);
            if (playing && follow_camera) { camera_eye=state.eye; camera_target=state.target; }
        }
        if (!playing) {
            camera_eye=camera_target+Point(camera_distance*std::sin(camera_yaw)*std::cos(camera_pitch),
                camera_distance*std::sin(camera_pitch),camera_distance*std::cos(camera_yaw)*std::cos(camera_pitch));
        }
        if (!playing && current.control_points.size()>=2) ship=trajectory::evaluate_de_casteljau(current.control_points,0.0);

        const Point forward=glm::normalize(camera_target-camera_eye);
        Point up(0,1,0); if (std::abs(glm::dot(forward,up))>0.985) up=Point(0,0,1);
        const glm::mat4 view=glm::lookAt(glm::vec3(0),glm::vec3(forward),glm::vec3(up));
        const glm::mat4 projection=glm::perspective(glm::radians(FOV),win.aspect(),0.1f,1200.0f);
        renderer.set_pass(view,projection,glm::normalize(glm::vec3(-0.6f,0.8f,0.4f)));
        renderer.time_seconds=static_cast<float>(win.total_time);
        glViewport(0,0,win.width,win.height);
        glClearColor(0.008f,0.012f,0.035f,1.0f); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST); glEnable(GL_PROGRAM_POINT_SIZE); glDisable(GL_BLEND);

        // Deep-space starfield and faint three-dimensional reference grid.
        draw_line(renderer,star_buffer,stars,camera_eye,glm::vec4(0.55f,0.68f,0.95f,0.8f),GL_POINTS,1.7f);
        std::vector<Point> grid; grid.reserve(84);
        for (int i=-80;i<=120;i+=10) { grid.push_back({(double)i,-28,-15}); grid.push_back({(double)i,28,-15}); }
        for (int i=-30;i<=30;i+=10) { grid.push_back({-80,(double)i,-15}); grid.push_back({120,(double)i,-15}); }
        draw_line(renderer,polygon_buffer,grid,camera_eye,glm::vec4(0.12f,0.18f,0.29f,1),GL_LINES,1.0f);

        // Draw the main route and all imported component routes in distinct colours.
        if (current.control_points.size()>=2) {
            draw_line(renderer,curve_buffer,trajectory::sample_curve(current.control_points,CURVE_SEGMENTS),camera_eye,
                      mission_index==1?CYAN:VIOLET,GL_LINE_STRIP,2.6f);
            draw_line(renderer,polygon_buffer,current.control_points,camera_eye,glm::vec4(0.55f,0.62f,0.72f,1),GL_LINE_STRIP,1.4f);
        }
        const std::array<glm::vec4,5> path_colors={CYAN,glm::vec4(1,0.47f,0.26f,1),GREEN,glm::vec4(1,0.78f,0.3f,1),VIOLET};
        for (std::size_t i=0;i<saved.size();++i) if (saved[i].size()>=2)
            draw_line(renderer,saved_buffer,trajectory::sample_curve(saved[i],160),camera_eye,path_colors[i%path_colors.size()],GL_LINE_STRIP,1.8f);

        // Mission waypoint planets are attached to points on the 3D Bezier path. Their layout is
        // a legible, compressed educational schematic; real orbital distances are not to scale.
        const auto& flybys=(mission_index==1?VOYAGER1_FLYBYS:VOYAGER2_FLYBYS);
        for (const auto& f:flybys) {
            const Point pp=trajectory::evaluate_de_casteljau(current.control_points,f.u);
            const bool saturn=std::string(f.name)=="SATURN";
            const std::string planet_name=f.name;
            const float spin_rate = planet_name=="JUPITER" ? 0.42f :
                                    planet_name=="SATURN" ? 0.36f :
                                    planet_name=="EARTH" ? 0.70f :
                                    planet_name=="TITAN" ? 0.10f :
                                    planet_name=="URANUS" ? -0.24f : 0.30f;
            draw_planet(renderer,sphere,polygon_buffer,pp,camera_eye,f.radius,f.color,saturn,win.total_time,
                        spin_rate,planet_textures.for_name(f.name));
            draw_point(renderer,event_buffer,pp,camera_eye,glm::vec4(f.color,1),6.0f);
        }
        // Sun near the departure point: animated corona plus a bright warm core.
        const Point earth=current.control_points.front();
        const Point sun=earth+Point(-10,4,-4);
        draw_sun_corona(renderer,sphere,sun,camera_eye,4.5f,win.total_time);
        const glm::mat4 sun_model = glm::translate(glm::mat4(1.0f),glm::vec3(sun-camera_eye))
                                  * glm::scale(glm::mat4(1.0f),glm::vec3(4.5f));
        Material sun_material=make_material({1.0f,0.72f,0.28f},0.0f,1.0f);
        renderer.draw_mesh(sphere,sun_model,sun_material);

        // Booster and cruise-stage trajectories remain independently active per keyframe flags.
        if (playing) {
            for (std::size_t i=1;i<saved.size() && i<state.parameters.size() && i<state.active_flags.size();++i) {
                if (state.active_flags[i] && saved[i].size()>=2) {
                    const Point component=trajectory::evaluate_de_casteljau(saved[i],state.parameters[i]);
                    draw_point(renderer,ship_buffer,component,camera_eye,path_colors[i%path_colors.size()],10.0f);
                    if (i==3) {
                        // The probe deployment trajectory carries a small articulated spacecraft model.
                        draw_spacecraft(renderer,sphere,box,component,camera_eye,win.total_time,true);
                    } else {
                        // Booster and upper-stage trajectories use distinct low-poly cylindrical bodies.
                        glm::mat4 stage=glm::translate(glm::mat4(1.0f),glm::vec3(component-camera_eye));
                        stage=stage*glm::rotate(glm::mat4(1.0f),glm::radians(90.0f),glm::vec3(0,0,1));
                        stage=stage*glm::translate(glm::mat4(1.0f),glm::vec3(0,-1.4f,0));
                        renderer.draw_mesh(stage_mesh,stage,make_material(i==1?glm::vec3(0.82f,0.38f,0.20f):glm::vec3(0.68f,0.73f,0.80f),0.6f,0.28f));
                    }
                }
            }
        }
        const bool deployed = playing && playback_time>7.0;
        draw_spacecraft(renderer,sphere,box,ship,camera_eye,win.total_time,deployed);
        // Animated micro-particles behind the craft add a subtle cruise/exhaust cue.
        if (playing && playback_time<5.0) {
            std::vector<Point> exhaust;
            for (int i=0;i<24;++i) {
                const double t=i/23.0;
                exhaust.emplace_back(ship.x-3.0-t*7.0,ship.y+std::sin(win.total_time*9+i*1.7)*0.7*t,
                                     ship.z+std::cos(win.total_time*7+i)*0.7*t);
            }
            draw_line(renderer,exhaust_buffer,exhaust,camera_eye,glm::vec4(1.0f,0.42f,0.13f,1),GL_POINTS,4.0f);
        }
        for (std::size_t i=0;i<current.control_points.size();++i) {
            const bool sel=(int)i==selected&&!playing;
            draw_point(renderer,sel?selected_buffer:points_buffer,current.control_points[i],camera_eye,
                       sel?SELECTED:CONTROL,sel?15.0f:9.0f);
        }

        // HUD pass: screen-space glass-like panels replace the crowded window-title instructions.
        glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        renderer.set_pass(glm::mat4(1),glm::ortho(0.0f,(float)win.width,0.0f,(float)win.height,-1.0f,1.0f),glm::vec3(0,0,1));
        const float leftw=330.0f, panelh=145.0f;
        hud_rect(renderer,hud_buffer,win.width,win.height,18,win.height-panelh-18,leftw,panelh,glm::vec4(0.025f,0.055f,0.105f,0.92f));
        hud_rect(renderer,hud_buffer,win.width,win.height,18,win.height-panelh-18,leftw,3,mission_index==1?CYAN:VIOLET);
        hud_text(renderer,hud_text_buffer,win.width,win.height,32,win.height-42,"VOYAGER MISSION CONTROL",WHITE,2.1f);
        hud_text(renderer,hud_text_buffer,win.width,win.height,32,win.height-65,mission_index==1?"VOYAGER 1 / TITAN SATURN":"VOYAGER 2 / GRAND TOUR",mission_index==1?CYAN:VIOLET,1.9f);
        hud_text(renderer,hud_text_buffer,win.width,win.height,32,win.height-88,playing?"STATUS: PLAYING":"STATUS: EDIT / PAUSED",GREEN,1.7f);
        hud_text(renderer,hud_text_buffer,win.width,win.height,32,win.height-109,"TIME: "+std::to_string((int)playback_time)+" S  SPEED: "+std::to_string(playback_speed).substr(0,3)+"X",WHITE,1.5f);
        hud_text(renderer,hud_text_buffer,win.width,win.height,32,win.height-129,"3D BEZIER  /  SCHEMATIC DISTANCES",glm::vec4(0.63f,0.76f,0.90f,1),1.35f);
        if (show_help) {
            const float hw=365.0f, hh=188.0f;
            hud_rect(renderer,hud_buffer,win.width,win.height,18,18,hw,hh,glm::vec4(0.025f,0.055f,0.105f,0.92f));
            hud_rect(renderer,hud_buffer,win.width,win.height,18,18,hw,3,glm::vec4(0.24f,0.72f,0.94f,1));
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,184,"NAVIGATION / EDITING",WHITE,1.8f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,163,"1 / 2  SWITCH VOYAGER MISSION",CYAN,1.45f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,145,"P PLAY / PAUSE    R RESET",WHITE,1.45f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,127,"MOUSE LEFT DRAG CONTROL POINT",WHITE,1.45f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,109,"RIGHT CLICK ADD  /  DEL REMOVE",WHITE,1.45f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,91,"WASD XY   Q/E DEPTH   ARROWS PAN",WHITE,1.35f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,73,"J/L ORBIT  I/K ELEVATE  +/- ZOOM",WHITE,1.35f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,55,"T SAVE  Y LOAD  [ ] PLAY SPEED",GREEN,1.45f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,32,37,"C CAMERA  H HIDE HELP  HOME RESET VIEW",WHITE,1.25f);
        }
        const float eventw=285.0f, eventh=32.0f;
        hud_rect(renderer,hud_buffer,win.width,win.height,win.width-eventw-18,win.height-eventh-18,eventw,eventh,glm::vec4(0.025f,0.055f,0.105f,0.90f));
        std::string event="MISSION: "+std::string(flybys.front().name);
        if (playing) {
            double best=10.0; const Flyby* closest=&flybys.front();
            for (const auto& f:flybys) { const double d=std::abs(f.u-state.parameter); if(d<best){best=d;closest=&f;} }
            event="FLYBY: "+std::string(closest->name);
        } else event="ROUTE: "+std::to_string(flybys.size()-2)+" PLANETARY WAYPOINTS";
        hud_text(renderer,hud_text_buffer,win.width,win.height,win.width-eventw-5,win.height-39,event,CYAN,1.5f);
        // Compact mission map legend with color-coded mini planet icons.
        const float legendw=220.0f, legendh=38.0f+static_cast<float>(flybys.size())*17.0f;
        hud_rect(renderer,hud_buffer,win.width,win.height,win.width-legendw-18,48,legendw,legendh,glm::vec4(0.025f,0.055f,0.105f,0.91f));
        hud_text(renderer,hud_text_buffer,win.width,win.height,win.width-legendw-5,48+legendh-20,"FLYBY WAYPOINTS",WHITE,1.45f);
        for (std::size_t fi=0; fi<flybys.size(); ++fi) {
            const float yy=48+legendh-39-static_cast<float>(fi)*17.0f;
            hud_buffer.update({{win.width-legendw+1.0f,yy+2.0f,0.0f}});
            renderer.draw_points(hud_buffer,GL_POINTS,glm::vec4(flybys[fi].color,1.0f),7.0f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,win.width-legendw+14,yy,flybys[fi].name,glm::vec4(flybys[fi].color,1.0f),1.35f);
        }
        hud_rect(renderer,hud_buffer,win.width,win.height,18,win.height-panelh-24,2.0f,6.0f,mission_index==1?CYAN:VIOLET);
        // On-screen buttons provide clickable controls in addition to the keyboard shortcuts.
        const float bw=78.0f, gap=8.0f;
        const float row1x=std::max(12.0f,(win.width-(8.0f*bw+7.0f*gap))*0.5f);
        const std::array<std::string,8> row1={"VGR 1","VGR 2",playing?"PAUSE":"PLAY","RESET","SPEED -","SPEED +","CAMERA",show_help?"HIDE HELP":"SHOW HELP"};
        for (int i=0;i<8;++i) {
            const float x=row1x+i*(bw+gap);
            const glm::vec4 c=(i==0&&mission_index==1)?CYAN:((i==1&&mission_index==2)?VIOLET:glm::vec4(0.10f,0.20f,0.32f,1));
            hud_rect(renderer,hud_buffer,win.width,win.height,x,252,bw,30,c);
            hud_text(renderer,hud_text_buffer,win.width,win.height,x+6,263,row1[i],WHITE,1.15f);
        }
        const float bw2=78.0f, gap2=8.0f;
        const float row2x=std::max(12.0f,(win.width-(6.0f*bw2+5.0f*gap2))*0.5f);
        const std::array<std::string,6> row2={"PAN LEFT","PAN RIGHT","PAN UP","PAN DOWN","ZOOM +","ZOOM -"};
        for (int i=0;i<6;++i) {
            const float x=row2x+i*(bw2+gap2);
            hud_rect(renderer,hud_buffer,win.width,win.height,x,215,bw2,30,glm::vec4(0.06f,0.13f,0.22f,1));
            hud_text(renderer,hud_text_buffer,win.width,win.height,x+5,226,row2[i],glm::vec4(0.72f,0.88f,1,1),1.15f);
        }
        // Visible mission timeline with progress fill and a draggable/clickable playhead.
        if (timeline_w>120.0f) {
            const float ty=184.0f;
            hud_rect(renderer,hud_buffer,win.width,win.height,timeline_x,ty,timeline_w,7.0f,glm::vec4(0.12f,0.22f,0.34f,0.98f));
            const float progress=static_cast<float>(std::clamp(playback_time/std::max(playback_duration,0.001),0.0,1.0));
            hud_rect(renderer,hud_buffer,win.width,win.height,timeline_x,ty,timeline_w*progress,7.0f,CYAN);
            const float knobx=timeline_x+timeline_w*progress;
            hud_rect(renderer,hud_buffer,win.width,win.height,knobx-3.0f,ty-5.0f,6.0f,17.0f,WHITE);
            hud_text(renderer,hud_text_buffer,win.width,win.height,timeline_x,199.0f,"MISSION TIMELINE",WHITE,1.15f);
            hud_text(renderer,hud_text_buffer,win.width,win.height,timeline_x+timeline_w-44.0f,199.0f,std::to_string((int)playback_duration)+"S",WHITE,1.05f);
        }
        hud_rect(renderer,hud_buffer,win.width,win.height,18,8,win.width-36,2,glm::vec4(0.15f,0.25f,0.38f,0.8f));
        hud_text(renderer,hud_text_buffer,win.width,win.height,24,win.height-11,message,glm::vec4(0.76f,0.86f,0.97f,1),1.25f);
        glDisable(GL_BLEND);

        // Keep title compact; all instructions and status are now in the designed HUD panels.
        glfwSetWindowTitle(win.handle,mission_index==1?"VOYAGER I  |  JUPITER - SATURN - INTERSTELLAR":"VOYAGER II  |  JUPITER - SATURN - URANUS - NEPTUNE");
        win.end_frame();
    }
    planet_textures.destroy();
    glfwTerminate();
    return 0;
}
