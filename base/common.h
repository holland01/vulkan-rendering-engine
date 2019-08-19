#pragma once

#include <glm/glm.hpp>
#include <filesystem>
#include <vector>

namespace fs = std::experimental::filesystem;

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 1024

// Assumes that v is an ADT with a to_string(std::string) method,
// and the string parameter for that method acts as a prefix
#define AS_STRING(v) v.to_string(#v)
#define AS_STRING_GLM_SS(v) #v << ": " << glm::to_string(v) 

#define AS_STRING_SS(v) #v << ": " << v
#define SEP_SS << ", " <<

#define MAT4V3(m, v) vec3_t((m) * vec4_t((v), real_t(1.0)))

using vec2_t = glm::vec2;
using vec3_t = glm::vec3;
using vec4_t = glm::vec4;

using mat4_t = glm::mat4;
using mat3_t = glm::mat3;

using real_t = float;

#define R(x) static_cast<real_t>(x)
#define R2(x) vec2_t{R(x)}
#define R3(x) vec3_t{R(x)}
#define R4(x) vec4_t{R(x)}

#define R4v(x,y,z,w) vec4_t{R(x), R(y), R(z), R(w)}
#define R3v(x,y,z) vec3_t{R(x), R(y), R(z)}

#define V3_UP R3v(0.0, 1.0, 0.0)
#define V3_DOWN R3v(0.0, -1.0, 0.0)

#define V3_LEFT R3v(-1.0, 0.0, 0.0)
#define V3_RIGHT R3v(1.0, 0.0, 0.0)

#define V3_FORWARD R3v(0.0, 0.0, -1.0)
#define V3_BACKWARD R3v(0.0, 0.0, 1.0)

#define SPHERE_UP(radius) V3_UP * R((radius))
#define SPHERE_DOWN(radius) V3_DOWN * R((radius))
#define SPHERE_LEFT(radius) V3_LEFT * R((radius))
#define SPHERE_RIGHT(radius) V3_RIGHT * R((radius))
#define SPHERE_FORWARD(radius) V3_FORWARD * R((radius))
#define SPHERE_BACKWARD(radius) V3_BACKWARD * R((radius))

#define OPENGL_REAL GL_FLOAT

struct vertex {
  vec3_t position;
  vec4_t color;
  vec2_t uv;
};

struct type_module;

extern std::vector<type_module*> g_modules;

struct type_module {
  virtual void free_mem() = 0;
  void registermod() {
    g_modules.push_back(this);
  }
};
