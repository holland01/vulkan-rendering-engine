#pragma once

#include <glm/glm.hpp>

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
