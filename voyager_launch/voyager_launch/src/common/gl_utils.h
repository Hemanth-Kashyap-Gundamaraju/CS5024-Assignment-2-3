// gl_utils.h - small OpenGL helpers: shader programs and textures.
#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "config.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Description: compiles one shader and prints the error text if it fails.
inline GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return shader;
}

// Description: a vertex shader + fragment shader linked together, with easy uniform setters.
class ShaderProgram {
public:
    // Description: compile and link. In: vertex source, fragment source.
    void build(const char* vertex_source, const char* fragment_source) {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_source);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
        id_ = glCreateProgram();
        glAttachShader(id_, vs);
        glAttachShader(id_, fs);
        glLinkProgram(id_);
        GLint ok = 0;
        glGetProgramiv(id_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
            std::cerr << "Shader link error:\n" << log << "\n";
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
    void use() const { glUseProgram(id_); }
    void set_int(const char* n, int v) { glUniform1i(_loc(n), v); }
    void set_float(const char* n, float v) { glUniform1f(_loc(n), v); }
    void set_vec2(const char* n, const glm::vec2& v) { glUniform2fv(_loc(n), 1, glm::value_ptr(v)); }
    void set_vec4(const char* n, const glm::vec4& v) { glUniform4fv(_loc(n), 1, glm::value_ptr(v)); }
    void set_vec3(const char* n, const glm::vec3& v) { glUniform3fv(_loc(n), 1, glm::value_ptr(v)); }
    void set_mat4(const char* n, const glm::mat4& m) { glUniformMatrix4fv(_loc(n), 1, GL_FALSE, glm::value_ptr(m)); }

private:
    GLint _loc(const char* name) {
        auto it = cache_.find(name);
        if (it != cache_.end()) return it->second;
        GLint l = glGetUniformLocation(id_, name);
        cache_[name] = l;
        return l;
    }
    GLuint id_ = 0;
    std::unordered_map<std::string, GLint> cache_;
};

// Description: uploads RGB pixels as a mipmapped texture. In: pixels (3 bytes each), size, repeat flag.
inline GLuint create_texture_rgb(const unsigned char* pixels, int width, int height, bool repeat_s = true, int channels = 3) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLenum format = (channels == 4) ? GL_RGBA : (channels == 1 ? GL_RED : GL_RGB);
    glTexImage2D(GL_TEXTURE_2D, 0, format == GL_RGBA ? GL_RGBA8 : (format == GL_RED ? GL_R8 : GL_RGB8),
                 width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    if (format == GL_RED) {   // grey texture: show the one channel as r=g=b
        GLint swizzle[4] = {GL_RED, GL_RED, GL_RED, GL_ONE};
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    }
    return tex;
}

// Description: loads a picture from assets/. Returns 0 if the file is missing.
// In: file name inside assets/, repeat flag.  Out: texture id or 0.
inline GLuint load_texture_file(const std::string& file_name, bool repeat = true) {
    std::string path = std::string(ASSET_DIR) + file_name;
    int w, h, n;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 3);
    if (!data) return 0;
    GLuint tex = create_texture_rgb(data, w, h, repeat, 3);
    stbi_image_free(data);
    std::cout << "Loaded texture " << path << "\n";
    return tex;
}
