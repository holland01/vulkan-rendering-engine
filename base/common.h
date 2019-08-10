#pragma once

#include <glm/glm.hpp>
#include <filesystem>
#include <vector>

namespace fs = std::experimental::filesystem;

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

using vec2_t = glm::vec2;
using vec3_t = glm::vec3;
using vec4_t = glm::vec4;

using mat4_t = glm::mat4;
using mat3_t = glm::mat3;

using real_t = float;

#define R(x) static_cast<real_t>(x)

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
