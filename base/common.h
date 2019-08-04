#pragma once

#include <glm/glm.hpp>
#include <filesystem>
#include <vector>

namespace fs = std::experimental::filesystem;

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

using v2 = glm::vec2;
using v3 = glm::vec3;
using v4 = glm::vec4;

struct vertex {
  v3 position;
  v4 color;
  v2 uv;
};

struct type_module;

extern std::vector<type_module*> g_modules;

struct type_module {
  virtual void free_mem() = 0;
  void registermod() {
    g_modules.push_back(this);
  }
};
