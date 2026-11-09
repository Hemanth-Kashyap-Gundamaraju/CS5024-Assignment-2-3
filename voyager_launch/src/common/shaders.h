// shaders.h - all GLSL shader code as strings.
// Everything is drawn "camera-relative": the model matrix already has the camera subtracted (done in double
// on the CPU), and the view matrix has only rotation. This stops the 6,371 km numbers from making things shake.
#pragma once

// ---------- lit surface (textured or plain colour, sun light, double-sided) ----------
inline const char* LIT_VERTEX_SRC = R"(
#version 330 core
layout(location=0) in vec3 a_pos;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec2 a_uv;
uniform mat4 u_model, u_view, u_proj;
out vec3 v_normal; out vec2 v_uv; out vec3 v_pos_rel; out vec3 v_local;
void main(){
  vec4 p = u_model * vec4(a_pos, 1.0);
  v_pos_rel = p.xyz;
  v_normal = transpose(inverse(mat3(u_model))) * a_normal;
  v_uv = a_uv; v_local = a_pos;
  gl_Position = u_proj * u_view * p;
}
)";

inline const char* LIT_FRAGMENT_SRC = R"(
#version 330 core
in vec3 v_normal; in vec2 v_uv; in vec3 v_pos_rel; in vec3 v_local;
out vec4 FragColor;
uniform vec3 u_sun_dir, u_color;
uniform int u_use_texture;
uniform sampler2D u_tex;
uniform vec2 u_uv_scale;
uniform float u_ambient, u_specular, u_shininess, u_alpha, u_fade_radius, u_alpha_from_luma;
void main(){
  vec3 n = normalize(v_normal);
  vec3 view_dir = normalize(-v_pos_rel);
  if (dot(n, view_dir) < 0.0) n = -n;               // thin parts are lit from both sides
  vec3 base = u_color; float alpha = u_alpha;
  if (u_use_texture == 1) {
    vec3 t = texture(u_tex, v_uv * u_uv_scale).rgb;
    base *= t;
    if (u_alpha_from_luma > 0.5) alpha *= dot(t, vec3(0.3333));
  }
  float diff = max(dot(n, u_sun_dir), 0.0);
  vec3 h = normalize(u_sun_dir + view_dir);
  float spec = diff > 0.0 ? pow(max(dot(n, h), 0.0), u_shininess) * u_specular : 0.0;
  vec3 col = base * (u_ambient + (1.0 - u_ambient) * diff) + vec3(spec);
  if (u_fade_radius > 0.0) alpha *= 1.0 - smoothstep(0.7 * u_fade_radius, u_fade_radius, length(v_local.xz));
  FragColor = vec4(col, alpha);
}
)";

// ---------- rocket exhaust flame (glowing cone, added on top of the picture) ----------
inline const char* FLAME_FRAGMENT_SRC = R"(
#version 330 core
in vec3 v_normal; in vec2 v_uv; in vec3 v_pos_rel; in vec3 v_local;
out vec4 FragColor;
uniform float u_intensity, u_time;
uniform vec3 u_core_color, u_edge_color;
void main(){
  float t = v_uv.y;                                  // 0 at the nozzle, 1 at the flame tip
  float flicker = 0.85 + 0.15 * sin(u_time * 45.0 + v_uv.x * 25.0 + t * 12.0);
  float rim = abs(dot(normalize(v_normal), normalize(-v_pos_rel)));
  float a = (1.0 - t) * (1.0 - 0.5 * t) * u_intensity * flicker * (0.3 + 0.7 * rim);
  FragColor = vec4(mix(u_core_color, u_edge_color, t), a);
}
)";

// ---------- plain colour (trail lines, markers) ----------
inline const char* FLAT_VERTEX_SRC = R"(
#version 330 core
layout(location=0) in vec3 a_pos;
uniform mat4 u_view, u_proj;
uniform float u_point_size;
void main(){ gl_Position = u_proj * u_view * vec4(a_pos, 1.0); gl_PointSize = u_point_size; }
)";
inline const char* FLAT_FRAGMENT_SRC = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 u_flat_color;
void main(){ FragColor = u_flat_color; }
)";

// ---------- sky: stars (texture if available, otherwise made in the shader) and the Sun ----------
inline const char* SKY_VERTEX_SRC = R"(
#version 330 core
layout(location=0) in vec3 a_pos;
uniform mat4 u_view, u_proj;
out vec3 v_dir;
void main(){ v_dir = a_pos; vec4 p = u_proj * u_view * vec4(a_pos, 1.0); gl_Position = p.xyww; }
)";
inline const char* SKY_FRAGMENT_SRC = R"(
#version 330 core
in vec3 v_dir;
out vec4 FragColor;
uniform int u_use_texture;
uniform sampler2D u_tex;
uniform float u_star_brightness;
uniform vec3 u_sun_dir;
float hash13(vec3 p){ p = fract(p * 0.1031); p += dot(p, p.zyx + 31.32); return fract((p.x + p.y) * p.z); }
float star_layer(vec3 d, float scale, float density){
  vec3 q = d * scale, id = floor(q), f = fract(q);
  if (hash13(id) < density) return 0.0;
  vec3 c = vec3(hash13(id + 1.7), hash13(id + 5.3), hash13(id + 9.1));
  float size = 0.07 + 0.10 * hash13(id + 3.3);
  return smoothstep(size, 0.0, length(f - c)) * (0.35 + 0.65 * hash13(id + 8.8));
}
void main(){
  vec3 d = normalize(v_dir);
  vec3 col;
  if (u_use_texture == 1) {
    vec2 uv = vec2(atan(-d.z, d.x) / 6.2831853 + 0.5, acos(clamp(d.y, -1.0, 1.0)) / 3.1415927);
    col = texture(u_tex, uv).rgb * 1.2;
  } else {
    float s = star_layer(d, 60.0, 0.80) + star_layer(d, 130.0, 0.85) + 0.6 * star_layer(d, 260.0, 0.88);
    float band = exp(-pow(dot(d, normalize(vec3(0.3, 0.8, 0.5))) * 5.0, 2.0));
    col = vec3(s) * vec3(1.0, 0.95, 0.9) + vec3(0.03, 0.035, 0.05) * band;
  }
  col *= u_star_brightness;
  float sd = dot(d, u_sun_dir);
  col += vec3(1.0, 0.95, 0.85) * (smoothstep(0.99990, 0.99996, sd) * 6.0 + pow(max(sd, 0.0), 300.0) * 0.4);
  FragColor = vec4(col, 1.0);
}
)";

// ---------- atmosphere: simple single-scattering sky glow, works from space and from the ground ----------
inline const char* ATMOSPHERE_VERTEX_SRC = R"(
#version 330 core
layout(location=0) in vec3 a_pos;
uniform mat4 u_model, u_view, u_proj;
out vec3 v_pos_rel;
void main(){ vec4 p = u_model * vec4(a_pos, 1.0); v_pos_rel = p.xyz; gl_Position = u_proj * u_view * p; }
)";
inline const char* ATMOSPHERE_FRAGMENT_SRC = R"(
#version 330 core
in vec3 v_pos_rel;
out vec4 FragColor;
uniform vec3 u_earth_center_rel;     // Earth centre as seen from the camera
uniform vec3 u_sun_dir;
uniform float u_earth_radius, u_atmo_radius, u_scale_height;
const vec3 BETA = vec3(5.8e-6, 13.5e-6, 33.1e-6);   // blue scatters most
const float SUN_POWER = 24.0;
bool ray_sphere(vec3 o, vec3 d, float r, out float t0, out float t1){
  float b = dot(o, d), c = dot(o, o) - r * r, disc = b * b - c;
  if (disc < 0.0) return false;
  float s = sqrt(disc); t0 = -b - s; t1 = -b + s; return true;
}
void main(){
  vec3 o = -u_earth_center_rel;                     // camera relative to Earth centre
  vec3 d = normalize(v_pos_rel);
  float a0, a1;
  if (!ray_sphere(o, d, u_atmo_radius, a0, a1)) discard;
  a0 = max(a0, 0.0);
  float e0, e1;
  if (ray_sphere(o, d, u_earth_radius, e0, e1) && e0 > 0.0) a1 = min(a1, e0);
  if (a1 <= a0) discard;
  const int N = 16;
  float ds = (a1 - a0) / float(N);
  vec3 sum = vec3(0.0); float tau_v = 0.0;
  for (int i = 0; i < N; i++) {
    vec3 p = o + d * (a0 + (float(i) + 0.5) * ds);
    float dens = exp(-(length(p) - u_earth_radius) / u_scale_height) * ds;
    tau_v += dens;
    float l0, l1, g0, g1;
    ray_sphere(p, u_sun_dir, u_atmo_radius, l0, l1);
    if (ray_sphere(p, u_sun_dir, u_earth_radius, g0, g1) && g0 > 0.0) continue;   // in Earth's shadow
    float tau_l = 0.0; float ls = l1 / 4.0;
    for (int k = 0; k < 4; k++) tau_l += exp(-(length(p + u_sun_dir * (float(k) + 0.5) * ls) - u_earth_radius) / u_scale_height) * ls;
    sum += exp(-BETA * (tau_v + tau_l)) * dens;
  }
  float mu = dot(d, u_sun_dir);
  float phase = 0.0596831 * (1.0 + mu * mu);        // Rayleigh phase
  vec3 col = sum * BETA * phase * SUN_POWER * 0.9;
  FragColor = vec4(1.0 - exp(-col), 1.0);
}
)";
