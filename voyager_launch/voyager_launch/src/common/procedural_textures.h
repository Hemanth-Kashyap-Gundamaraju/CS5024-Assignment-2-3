// procedural_textures.h - makes simple stand-in textures in code, used when a picture is missing from assets/.
// Replace them with real NASA / Solar System Scope images for a better look (see README).
#pragma once
#include <cmath>
#include <functional>
#include "gl_utils.h"

// ---------- noise helpers ----------
// Description: random-looking number 0..1 from three integers.
inline float noise_hash(int x, int y, int z) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)z * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFF) / 16777215.0f;
}

// Description: smooth 3D value noise (0..1).
inline float value_noise(const glm::vec3& p) {
    glm::vec3 f = glm::floor(p), t = p - f;
    t = t * t * (3.0f - 2.0f * t);
    int ix = (int)f.x, iy = (int)f.y, iz = (int)f.z;
    auto h = [&](int a, int b, int c) { return noise_hash(ix + a, iy + b, iz + c); };
    float x00 = glm::mix(h(0, 0, 0), h(1, 0, 0), t.x), x10 = glm::mix(h(0, 1, 0), h(1, 1, 0), t.x);
    float x01 = glm::mix(h(0, 0, 1), h(1, 0, 1), t.x), x11 = glm::mix(h(0, 1, 1), h(1, 1, 1), t.x);
    return glm::mix(glm::mix(x00, x10, t.y), glm::mix(x01, x11, t.y), t.z);
}

// Description: layered noise (fbm) with `octaves` layers.
inline float fractal_noise(glm::vec3 p, int octaves) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * value_noise(p);
        norm += amp;
        p *= 2.03f;
        amp *= 0.5f;
    }
    return sum / norm;
}

// A pixel generator: gets (u, v) in 0..1, returns an RGB colour 0..1.
using PixelFunction = std::function<glm::vec3(float, float)>;

// Description: fills a width x height image by calling pixel_fn for every pixel, then makes a texture.
inline GLuint make_texture_from_function(int width, int height, const PixelFunction& pixel_fn, bool repeat = true) {
    std::vector<unsigned char> pixels((size_t)width * height * 3);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            glm::vec3 c = glm::clamp(pixel_fn((x + 0.5f) / width, (y + 0.5f) / height), 0.0f, 1.0f);
            size_t i = ((size_t)y * width + x) * 3;
            pixels[i] = (unsigned char)(c.r * 255);
            pixels[i + 1] = (unsigned char)(c.g * 255);
            pixels[i + 2] = (unsigned char)(c.b * 255);
        }
    return create_texture_rgb(pixels.data(), width, height, repeat, 3);
}

// ---------- the stand-in textures ----------
// Description: fake Earth (blue sea, noise continents, polar ice) in lat/lon layout.
inline glm::vec3 earth_pixel(float u, float v) {
    float lon = (u - 0.5f) * 6.2831853f, lat = (0.5f - v) * 3.1415927f;
    glm::vec3 dir(std::cos(lat) * std::cos(lon), std::sin(lat), -std::cos(lat) * std::sin(lon));
    float height = fractal_noise(dir * 2.3f + glm::vec3(7.0f), 6);
    float detail = fractal_noise(dir * 14.0f, 4);
    float abs_lat = std::fabs(lat) * 57.2958f;
    glm::vec3 color;
    if (height < 0.52f) {
        float depth = glm::clamp((0.52f - height) * 6.0f, 0.0f, 1.0f);
        color = glm::mix(glm::vec3(0.10f, 0.32f, 0.55f), glm::vec3(0.02f, 0.08f, 0.28f), depth);
    } else {
        float dry = glm::smoothstep(8.0f, 28.0f, abs_lat) * (1.0f - glm::smoothstep(30.0f, 45.0f, abs_lat));
        glm::vec3 green(0.16f, 0.36f, 0.14f), sand(0.62f, 0.52f, 0.32f);
        color = glm::mix(green, sand, dry * detail) * (0.8f + 0.4f * detail);
        color = glm::mix(color, glm::vec3(0.95f), glm::smoothstep(0.66f, 0.74f, height));  // snowy peaks
    }
    return glm::mix(color, glm::vec3(0.95f, 0.97f, 1.0f), glm::smoothstep(68.0f, 76.0f, abs_lat));  // ice caps
}

// Description: grassy ground with dirt patches (tiles nicely).
inline glm::vec3 ground_pixel(float u, float v) {
    glm::vec3 p(u * 24.0f, v * 24.0f, 3.0f);
    float n = fractal_noise(p, 4), m = fractal_noise(p * 0.25f + 9.0f, 3);
    glm::vec3 grass(0.22f, 0.36f, 0.14f), dry(0.45f, 0.42f, 0.24f);
    return glm::mix(grass, dry, glm::smoothstep(0.45f, 0.65f, m)) * (0.75f + 0.5f * n);
}

// Description: grey concrete with small speckles.
inline glm::vec3 concrete_pixel(float u, float v) {
    glm::vec3 p(u * 40.0f, v * 40.0f, 1.0f);
    float n = fractal_noise(p, 4);
    float crack = (std::fabs(u * 4.0f - std::floor(u * 4.0f + 0.5f)) < 0.01f || std::fabs(v * 4.0f - std::floor(v * 4.0f + 0.5f)) < 0.01f) ? 0.6f : 1.0f;
    return glm::vec3(0.55f + 0.2f * n) * crack;
}

// Description: grey Moon with crater-like blotches.
inline glm::vec3 moon_pixel(float u, float v) {
    float lon = (u - 0.5f) * 6.2831853f, lat = (0.5f - v) * 3.1415927f;
    glm::vec3 dir(std::cos(lat) * std::cos(lon), std::sin(lat), -std::cos(lat) * std::sin(lon));
    float n = fractal_noise(dir * 6.0f, 6);
    float maria = glm::smoothstep(0.50f, 0.58f, fractal_noise(dir * 1.8f + 4.0f, 3));
    return glm::vec3(0.62f) * (0.55f + 0.7f * n) * (1.0f - 0.35f * maria);
}

// Description: load assets/<file> if it exists, otherwise build the stand-in from pixel_fn.
// In: file name, fallback generator, texture size, repeat flag.  Out: texture id.
inline GLuint texture_or_fallback(const std::string& file_name, const PixelFunction& fallback, int w, int h, bool repeat = true) {
    GLuint tex = load_texture_file(file_name, repeat);
    if (tex) return tex;
    std::cout << "assets/" << file_name << " not found - using a generated stand-in.\n";
    return make_texture_from_function(w, h, fallback, repeat);
}
