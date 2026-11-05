// earth.h - EarthSystem: stars, Sun, Moon, Earth, clouds and atmosphere (the "space pass").
#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include "camera.h"
#include "procedural_textures.h"
#include "renderer.h"

constexpr int EARTH_STACKS = 128, EARTH_SLICES = 256;   // how finely the Earth sphere is cut
constexpr float CLOUD_HEIGHT_FACTOR = 1.0025f;          // cloud shell is a bit bigger than Earth

class EarthSystem {
public:
    // Description: build meshes and load (or generate) textures. In: renderer for shaders.
    void init() {
        sphere_ = make_sphere(EARTH_STACKS, EARTH_SLICES);
        sky_sphere_ = make_sphere(24, 48);
        earth_texture_ = texture_or_fallback("earth_day.jpg", earth_pixel, 1024, 512, true);
        moon_texture_ = texture_or_fallback("moon.jpg", moon_pixel, 512, 256, true);
        cloud_texture_ = load_texture_file("clouds.jpg", true);   // optional: no stand-in
        star_texture_ = load_texture_file("stars.jpg", true);     // optional: shader stars if missing
    }

    // Description: draws the whole space pass: stars, Moon, Earth, clouds, atmosphere.
    // In: renderer, frame, Earth spin angle, how bright the stars should be (0..1).
    void draw(SceneRenderer& r, const FrameContext& ctx, double earth_theta) {
        r.set_pass(ctx.view, ctx.proj_space, ctx.sun_dir);
        _draw_sky(r, ctx);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        _draw_moon(r, ctx);
        _draw_earth(r, ctx, earth_theta);
        _draw_clouds(r, ctx, earth_theta);
        _draw_atmosphere(r, ctx);
    }

    // Description: star brightness: stars fade out in daylight when you are inside the air.
    static float star_brightness(const FrameContext& ctx) {
        glm::dvec3 up = glm::normalize(ctx.camera_position);
        double sun_height = glm::dot(up, glm::dvec3(ctx.sun_dir));
        double day = glm::smoothstep(-0.12, 0.15, sun_height);
        double in_air = 1.0 - glm::clamp(ctx.altitude / 120000.0, 0.0, 1.0);
        return (float)(1.0 - day * in_air);
    }

private:
    Mesh sphere_, sky_sphere_;
    GLuint earth_texture_ = 0, moon_texture_ = 0, cloud_texture_ = 0, star_texture_ = 0;

    void _draw_sky(SceneRenderer& r, const FrameContext& ctx) {
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE);
        r.sky.use();
        r.sky.set_mat4("u_view", ctx.view); r.sky.set_mat4("u_proj", ctx.proj_space);
        r.sky.set_vec3("u_sun_dir", ctx.sun_dir);
        r.sky.set_float("u_star_brightness", star_brightness(ctx));
        r.sky.set_int("u_use_texture", star_texture_ ? 1 : 0);
        if (star_texture_) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, star_texture_); r.sky.set_int("u_tex", 0); }
        sky_sphere_.draw();
    }
    void _draw_moon(SceneRenderer& r, const FrameContext& ctx) {
        WorldTransform t; t.position = moon_position();
        Material m = make_material(glm::vec3(1.0f), 0.0f, 0.03f);
        m.texture = moon_texture_;
        r.draw_mesh(sphere_, make_relative_model(t, glm::scale(glm::mat4(1.0f), glm::vec3((float)MOON_RADIUS_M)), ctx.camera_position), m);
    }
    glm::mat4 _earth_model(const FrameContext& ctx, double theta, float scale) const {
        WorldTransform t; t.rotation = earth_rotation(theta);
        return make_relative_model(t, glm::scale(glm::mat4(1.0f), glm::vec3((float)EARTH_RADIUS_M * scale)), ctx.camera_position);
    }
    void _draw_earth(SceneRenderer& r, const FrameContext& ctx, double theta) {
        Material m = make_material(glm::vec3(1.0f), 0.12f, 0.025f);
        m.texture = earth_texture_; m.shininess = 60.0f;
        r.draw_mesh(sphere_, _earth_model(ctx, theta, 1.0f), m);
    }
    void _draw_clouds(SceneRenderer& r, const FrameContext& ctx, double theta) {
        if (!cloud_texture_) return;
        Material m = make_material(glm::vec3(1.0f), 0.0f, 0.02f);
        m.texture = cloud_texture_; m.alpha_from_luma = true;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        r.draw_mesh(sphere_, _earth_model(ctx, theta * 1.0 + 0.0, CLOUD_HEIGHT_FACTOR), m);
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    }
    void _draw_atmosphere(SceneRenderer& r, const FrameContext& ctx) {
        float atmo_radius = (float)(EARTH_RADIUS_M + ATMOSPHERE_TOP_M);
        r.atmosphere.use();
        WorldTransform t;
        glm::mat4 model = make_relative_model(t, glm::scale(glm::mat4(1.0f), glm::vec3(atmo_radius * 1.02f)), ctx.camera_position);
        r.atmosphere.set_mat4("u_model", model); r.atmosphere.set_mat4("u_view", ctx.view); r.atmosphere.set_mat4("u_proj", ctx.proj_space);
        r.atmosphere.set_vec3("u_earth_center_rel", glm::vec3(-ctx.camera_position));
        r.atmosphere.set_vec3("u_sun_dir", ctx.sun_dir);
        r.atmosphere.set_float("u_earth_radius", (float)EARTH_RADIUS_M);
        r.atmosphere.set_float("u_atmo_radius", atmo_radius);
        r.atmosphere.set_float("u_scale_height", 8000.0f);
        bool camera_inside = glm::length(ctx.camera_position) < atmo_radius * 1.02;
        glEnable(GL_CULL_FACE);
        glCullFace(camera_inside ? GL_FRONT : GL_BACK);   // inside: draw the far wall; outside: the near wall
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);   // glow is added on top
        sphere_.draw();
        glDisable(GL_CULL_FACE); glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
};
