#pragma once

#include <GL/glew.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#if defined(_MSC_VER)
#define OS_WINDOWS
#elif defined(__linux__)
#define OS_LINUX
#else
#pragma error "Unsupported OS; must be either Windows with the latest MSVC or Ubuntu 18.04 with gcc-7 or later"
#endif

#if defined(OS_WINDOWS)
#include <filesystem>
#else
#include <experimental/filesystem>
#endif

#include <vector>
#include <algorithm>

namespace fs = std::experimental::filesystem;

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 1024

#if defined (__GNUC__)
#define TR_NOINLINE __attribute__ ((noinline))
#else
#error "Check for noinline support for this compiler"
#define TR_NOINLINE
#endif

// Assumes that v is an ADT with a to_string(std::string) method,
// and the string parameter for that method acts as a prefix
#define AS_STRING(v) v.to_string(#v)
#define AS_STRING_GLM_SS(v) #v << ": " << glm::to_string(v) 

#define AS_STRING_SS(v) #v << ": " << v
#define SEP_SS << ", " <<

#define MAT4V3(m, v) vec3_t((m) * vec4_t((v), real_t(1.0)))

#define DEBUGLINE write_logf("FILE:%s,LINE:%i\n", __FILE__, __LINE__)


#define NOP ;

using vec2_t = glm::vec2;
using vec3_t = glm::vec3;
using boolvec3_t = glm::bvec3;
using vec4_t = glm::vec4;

using mat4_t = glm::mat4;
using mat3_t = glm::mat3;

using real_t = float;

template <class T>
using darray = std::vector<T>;

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

#define OPENGL_VERSION_MAJOR 4
#define OPENGL_VERSION_MINOR 3

#define OPENGL_VERSION_MAJOR_STR "4"
#define OPENGL_VERSION_MINOR_STR "3"

template <typename T>
void safe_del(T*& t) {
  if (t != nullptr) {
    delete t;
    t = nullptr;
  }
}
 
struct vertex {
  vec3_t position;
  vec4_t color;
  vec3_t normal;
  vec2_t uv;
};

struct type_module;
struct framebuffer_ops;

struct modules {
  framebuffer_ops* framebuffer{nullptr};

  void init();
  void free();
} extern g_m;

//extern std::vector<type_module*> g_modules;

struct type_module {
  virtual void free_mem() = 0;
  void registermod() {
    //g_modules.push_back(this);
  }
};

template <typename numType>
static inline numType unset() {
  return static_cast<numType>(-1);
}

static inline mat4_t m4i() {
  return mat4_t{R(1)};
}

template <typename T>
static inline bool vec_contains(const std::vector<T>& v, const T& t) {
  return std::find(v.begin(), v.end(), t) != v.end();
}

template <typename T>
static inline std::vector<T> vec_join(const std::vector<T>& a, const std::vector<T>& b) {
  std::vector<T> c;
  c.insert(c.end(), a.begin(), a.end());
  c.insert(c.end(), b.begin(), b.end());
  return c;
}

template <typename T>
static inline std::vector<T> operator+(const std::vector<T>& a, const std::vector<T>& b) {
  return vec_join(a, b);
}