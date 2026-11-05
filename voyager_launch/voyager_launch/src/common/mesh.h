// mesh.h - a Mesh (GPU triangles) plus builders for the basic shapes: sphere, box, cylinder, revolve, ground cap.
#pragma once
#include <cmath>
#include <utility>
#include <vector>
#include "gl_utils.h"
#include "frames.h"

struct Vertex { glm::vec3 position; glm::vec3 normal; glm::vec2 uv; };

// Description: triangles stored on the GPU. Move-only (cannot be copied by accident).
class Mesh {
public:
    Mesh() = default;
    ~Mesh() { _release(); }
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& o) noexcept { _swap(o); }
    Mesh& operator=(Mesh&& o) noexcept { if (this != &o) { _release(); _swap(o); } return *this; }

    // Description: send vertices and indices to the GPU.
    void upload(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices) {
        _release();
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
        index_count_ = (GLsizei)indices.size();
        glBindVertexArray(0);
    }
    void draw() const { glBindVertexArray(vao_); glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr); }

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei index_count_ = 0;
    void _swap(Mesh& o) { std::swap(vao_, o.vao_); std::swap(vbo_, o.vbo_); std::swap(ebo_, o.ebo_); std::swap(index_count_, o.index_count_); }
    void _release() {
        if (vao_) glDeleteVertexArrays(1, &vao_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (ebo_) glDeleteBuffers(1, &ebo_);
        vao_ = vbo_ = ebo_ = 0;
    }
};

// Description: a list of 3D points drawn as lines/points (used for flight trails and markers).
class PointBuffer {
public:
    ~PointBuffer() { if (vao_) { glDeleteVertexArrays(1, &vao_); glDeleteBuffers(1, &vbo_); } }
    // Description: replace the points. In: points (already camera-relative).
    void update(const std::vector<glm::vec3>& points) {
        if (!vao_) {
            glGenVertexArrays(1, &vao_);
            glGenBuffers(1, &vbo_);
            glBindVertexArray(vao_);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
        }
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW);
        count_ = (GLsizei)points.size();
    }
    void draw(GLenum mode) const { if (count_ > 0) { glBindVertexArray(vao_); glDrawArrays(mode, 0, count_); } }
private:
    GLuint vao_ = 0, vbo_ = 0;
    GLsizei count_ = 0;
};

// ---------------- shape builders ----------------
constexpr float CREASE_COS = 0.6f;   // sharper corners than ~53 degrees stay sharp, softer ones are smoothed

// Description: spins a 2D outline (radius, height) around the Y axis. Great for rockets, nose cones, dishes.
// In: profile points (x = radius, y = height, going bottom->top), slices, angle range (default full circle).
// Out: Mesh.  Uses: every rocket part.
inline Mesh make_revolve(const std::vector<glm::vec2>& profile, int slices, float angle_start = 0.0f, float angle_end = 6.2831853f) {
    std::vector<Vertex> verts;
    std::vector<unsigned> idx;
    int segs = (int)profile.size() - 1;
    std::vector<glm::vec2> seg_normal(segs);
    for (int s = 0; s < segs; s++) {
        glm::vec2 d = profile[s + 1] - profile[s];
        glm::vec2 n(d.y, -d.x);
        float len = glm::length(n);
        seg_normal[s] = len > 1e-9f ? n / len : glm::vec2(0, 1);
    }
    for (int s = 0; s < segs; s++) {
        for (int e = 0; e < 2; e++) {
            int p = s + e, other = e == 0 ? s - 1 : s + 1;
            glm::vec2 n = seg_normal[s];
            if (other >= 0 && other < segs && glm::dot(seg_normal[s], seg_normal[other]) > CREASE_COS)
                n = glm::normalize(seg_normal[s] + seg_normal[other]);
            for (int j = 0; j <= slices; j++) {
                float t = (float)j / slices, a = angle_start + (angle_end - angle_start) * t;
                float c = std::cos(a), sn = std::sin(a);
                verts.push_back({glm::vec3(profile[p].x * c, profile[p].y, profile[p].x * sn),
                                 glm::vec3(n.x * c, n.y, n.x * sn), glm::vec2(t, (float)p / segs)});
            }
        }
    }
    for (int s = 0; s < segs; s++) {
        unsigned base = s * 2 * (slices + 1);
        for (int j = 0; j < slices; j++) {
            unsigned a = base + j, b = base + (slices + 1) + j;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    Mesh m;
    m.upload(verts, idx);
    return m;
}

// Description: cylinder or cone-frustum standing on the XZ plane, from y=0 to y=height.
inline Mesh make_cylinder(float bottom_radius, float top_radius, float height, int slices = 32, bool capped = true) {
    std::vector<glm::vec2> p;
    if (capped) p.push_back({0, 0});
    p.push_back({bottom_radius, 0});
    p.push_back({top_radius, height});
    if (capped) p.push_back({0, height});
    return make_revolve(p, slices);
}

// Description: flat disc on the XZ plane facing up.
inline Mesh make_disc(float radius, int slices = 48) { return make_revolve({{radius, 0}, {0, 0}}, slices); }

// Description: box centred on the origin with flat sides.
inline Mesh make_box(float sx, float sy, float sz) {
    glm::vec3 h(sx / 2, sy / 2, sz / 2);
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    struct Face { glm::vec3 n, u, w; };
    Face faces[6] = {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
                     {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
                     {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}, {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}};
    for (auto& f : faces) {
        unsigned base = (unsigned)v.size();
        for (int i = 0; i < 4; i++) {
            float a = (i == 1 || i == 2) ? 1.f : -1.f, b = (i >= 2) ? 1.f : -1.f;
            v.push_back({(f.n + f.u * a + f.w * b) * h, f.n, glm::vec2((a + 1) / 2, (b + 1) / 2)});
        }
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    Mesh m;
    m.upload(v, idx);
    return m;
}

// Description: unit sphere (radius 1) with lat/lon texture coordinates (u = longitude, v = from north pole).
inline Mesh make_sphere(int stacks, int slices) {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    for (int i = 0; i <= stacks; i++)
        for (int j = 0; j <= slices; j++) {
            float u = (float)j / slices, t = (float)i / stacks;
            float lon = (u - 0.5f) * 6.2831853f, lat = (0.5f - t) * 3.1415927f;
            glm::vec3 p(std::cos(lat) * std::cos(lon), std::sin(lat), -std::cos(lat) * std::sin(lon));
            v.push_back({p, p, glm::vec2(u, t)});
        }
    for (int i = 0; i < stacks; i++)
        for (int j = 0; j < slices; j++) {
            unsigned a = i * (slices + 1) + j, b = a + slices + 1;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    Mesh m;
    m.upload(v, idx);
    return m;
}

// Description: a patch of Earth's curved surface around the launch pad (pad-local coordinates: +Y up).
// Rings are packed tightly near the pad. In: angle radius (rad), rings, sectors, texture tile size in metres.
inline Mesh make_ground_cap(double angular_radius, int rings, int sectors, float tile_metres) {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    for (int i = 0; i <= rings; i++) {
        double f = (double)i / rings, alpha = angular_radius * f * f;
        for (int j = 0; j <= sectors; j++) {
            double beta = 6.283185307 * j / sectors;
            double x = EARTH_RADIUS_M * std::sin(alpha) * std::cos(beta), z = EARTH_RADIUS_M * std::sin(alpha) * std::sin(beta);
            double y = -2.0 * EARTH_RADIUS_M * std::sin(alpha / 2) * std::sin(alpha / 2);   // how far the ground curves down
            glm::vec3 n((float)(std::sin(alpha) * std::cos(beta)), (float)std::cos(alpha), (float)(std::sin(alpha) * std::sin(beta)));
            v.push_back({glm::vec3((float)x, (float)y, (float)z), n, glm::vec2((float)x / tile_metres, (float)z / tile_metres)});
        }
    }
    for (int i = 0; i < rings; i++)
        for (int j = 0; j < sectors; j++) {
            unsigned a = i * (sectors + 1) + j, b = a + sectors + 1;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    Mesh m;
    m.upload(v, idx);
    return m;
}
