// renderer.h - SceneRenderer: owns the shaders and draws meshes. Also Material (how a surface looks)
// and WorldTransform (a position + rotation in double precision).
#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include "camera.h"
#include "mesh.h"
#include "shaders.h"

// Description: how a surface looks (colour, texture, shininess...).
struct Material {
    glm::vec3 color{1.0f};
    float ambient = 0.22f;       // light level in shadow (0..1)
    float specular = 0.25f;
    float shininess = 40.0f;
    GLuint texture = 0;          // 0 = plain colour
    glm::vec2 uv_scale{1.0f};
    float alpha = 1.0f;
    float fade_radius = 0.0f;    // >0: fade out near this distance from the local origin (ground patch edge)
    bool alpha_from_luma = false; // cloud layer: brightness becomes opacity
};

// Description: shortcut to build a plain Material from a colour.
inline Material make_material(const glm::vec3& color, float specular = 0.25f, float ambient = 0.22f) {
    Material m; m.color = color; m.specular = specular; m.ambient = ambient; return m;
}

// Description: a position (double) and rotation (double) in the world.
struct WorldTransform {
    dvec3 position{0.0};
    dmat3 rotation{1.0};
};

// Description: model matrix with the camera subtracted, so floats stay accurate.
// In: transform, local matrix (float, in the object's own space), camera position (double).
inline glm::mat4 make_relative_model(const WorldTransform& t, const glm::mat4& local, const dvec3& camera_position) {
    glm::mat4 m = glm::mat4(glm::mat3(t.rotation));
    m[3] = glm::vec4(glm::vec3(t.position - camera_position), 1.0f);
    return m * local;
}

class SceneRenderer {
public:
    ShaderProgram lit, flame, flat, sky, atmosphere;
    float time_seconds = 0.0f;

    // Description: build all shaders (call once after the window exists).
    void init() {
        lit.build(LIT_VERTEX_SRC, LIT_FRAGMENT_SRC);
        flame.build(LIT_VERTEX_SRC, FLAME_FRAGMENT_SRC);
        flat.build(FLAT_VERTEX_SRC, FLAT_FRAGMENT_SRC);
        sky.build(SKY_VERTEX_SRC, SKY_FRAGMENT_SRC);
        atmosphere.build(ATMOSPHERE_VERTEX_SRC, ATMOSPHERE_FRAGMENT_SRC);
    }

    // Description: remember camera matrices and sun for the draw calls that follow.
    void set_pass(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun_dir) {
        view_ = view; proj_ = proj; sun_dir_ = sun_dir;
    }

    // Description: draw a mesh with the lit shader. In: mesh, model matrix (camera-relative), material.
    void draw_mesh(const Mesh& mesh, const glm::mat4& model, const Material& mat) {
        lit.use();
        lit.set_mat4("u_model", model); lit.set_mat4("u_view", view_); lit.set_mat4("u_proj", proj_);
        lit.set_vec3("u_sun_dir", sun_dir_); lit.set_vec3("u_color", mat.color);
        lit.set_float("u_ambient", mat.ambient); lit.set_float("u_specular", mat.specular);
        lit.set_float("u_shininess", mat.shininess); lit.set_float("u_alpha", mat.alpha);
        lit.set_float("u_fade_radius", mat.fade_radius); lit.set_float("u_alpha_from_luma", mat.alpha_from_luma ? 1.0f : 0.0f);
        lit.set_vec2("u_uv_scale", mat.uv_scale);
        lit.set_int("u_use_texture", mat.texture ? 1 : 0);
        if (mat.texture) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mat.texture); lit.set_int("u_tex", 0); }
        mesh.draw();
    }

    // Description: draw a glowing exhaust flame (cone with its tip at y=1 pointing +Y; rotate it to point down).
    void draw_flame(const Mesh& cone, const glm::mat4& model, float intensity) {
        flame.use();
        flame.set_mat4("u_model", model); flame.set_mat4("u_view", view_); flame.set_mat4("u_proj", proj_);
        flame.set_float("u_intensity", intensity); flame.set_float("u_time", time_seconds);
        flame.set_vec3("u_core_color", glm::vec3(1.0f, 0.95f, 0.8f));
        flame.set_vec3("u_edge_color", glm::vec3(1.0f, 0.45f, 0.12f));
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
        cone.draw();
        glDepthMask(GL_TRUE); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Description: draw lines or points (positions already camera-relative).
    void draw_points(const PointBuffer& buffer, GLenum mode, const glm::vec4& color, float point_size = 1.0f) {
        flat.use();
        flat.set_mat4("u_view", view_); flat.set_mat4("u_proj", proj_);
        flat.set_vec4("u_flat_color", color); flat.set_float("u_point_size", point_size);
        buffer.draw(mode);
    }

    const glm::mat4& view() const { return view_; }
    const glm::mat4& proj() const { return proj_; }
    const glm::vec3& sun_dir() const { return sun_dir_; }

private:
    glm::mat4 view_{1.0f}, proj_{1.0f};
    glm::vec3 sun_dir_{1, 0, 0};
};

// Description: start the "local pass" (pad, rocket, flames): clears depth so near things never fight with
// planet-sized things, and uses the close-up projection. In: renderer, frame.
inline void begin_local_pass(SceneRenderer& r, const FrameContext& ctx) {
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    r.set_pass(ctx.view, ctx.proj_local, ctx.sun_dir);
}
