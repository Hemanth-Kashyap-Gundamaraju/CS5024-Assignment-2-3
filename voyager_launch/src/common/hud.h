// hud.h - shared screen-space instruction panel for the Assignment 2 viewers.
#pragma once
#include <array>
#include <cctype>
#include <string>
#include <vector>
#include "renderer.h"
#include "app_window.h"
namespace ViewerHUD {
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


inline void draw(SceneRenderer& renderer, const AppWindow& win, const std::string& heading,
                 const std::vector<std::string>& instructions, bool visible) {
    if (!visible) return;
    static PointBuffer panel_buffer, text_buffer;
    const glm::vec4 background(0.012f, 0.025f, 0.055f, 0.88f);
    const glm::vec4 white(0.88f, 0.94f, 1.0f, 1.0f);
    const glm::vec4 cyan(0.20f, 0.85f, 1.0f, 1.0f);
    const float left = 18.0f, top = static_cast<float>(win.height) - 18.0f;
    const float panel_w = 330.0f, line_h = 21.0f;
    const float panel_h = 43.0f + line_h * static_cast<float>(instructions.size());
    const float bottom = top - panel_h;
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    renderer.set_pass(glm::mat4(1.0f), glm::ortho(0.0f, static_cast<float>(win.width),
        0.0f, static_cast<float>(win.height), -1.0f, 1.0f), glm::vec3(0, 0, 1));
    hud_draw_rect(renderer, panel_buffer, left, bottom, panel_w, panel_h, background);
    hud_draw_rect(renderer, panel_buffer, left, top - 3.0f, panel_w, 3.0f, cyan);
    hud_draw_text(renderer, text_buffer, left + 12.0f, top - 25.0f, heading, cyan, 1.7f);
    float y = top - 48.0f;
    for (const auto& instruction : instructions) {
        hud_draw_text(renderer, text_buffer, left + 12.0f, y, instruction, white, 1.35f);
        y -= line_h;
    }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}
} // namespace ViewerHUD
