#pragma once
#if !defined(BASE_ENABLE_VULKAN)
#define BASE_ENABLE_VULKAN
#endif // BASE_ENABLE_VULKAN

// We need to include the Vulkan headers here,
// since GLFW3 contains vulkan specific
// information that's dependent on the inclusion of a 
// Vulkan header. At some point, reorganizing header includes
// will definitely be the way to go. For now,
// this works.
#if defined(BASE_ENABLE_VULKAN)
  #include <vulkan/vulkan.h>
#endif // BASE_USE_VULKAN

#include <GL/glew.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#define DEBUG_ASSERTS

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
#include <limits>
#include <sstream>
#include <iomanip>
#include <array>
#include <functional>
#include <stdint.h>
#include <initializer_list>

#include "util.hpp"


namespace fs = std::experimental::filesystem;

#define SCREEN_WIDTH 1366
#define SCREEN_HEIGHT 768

#if defined (__GNUC__)
#define TR_NOINLINE __attribute__ ((noinline))
#elif defined (_MSC_VER)
#define TR_NOINLINE __declspec(noinline)
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

#define SS_HEX(value) "0x" << std::uppercase << std::setfill('0') << std::setw((sizeof(value)) << 1) << std::hex << (value) << std::dec

#define SS_HEX_NAME(value) #value << ":" << SS_HEX(value)

#define MAT4V3(m, v) vec3_t((m) * vec4_t((v), real_t(1.0)))

#define DEBUGLINE write_logf("FILE:%s,LINE:%i\n", __FILE__, __LINE__)

#ifdef DEBUG_ASSERTS
#define ASSERT_CODE(expr) do { expr; } while (0)
#else
#define ASSERT_CODE(expr)
#endif

#define NOP ;


using vec2_t = glm::vec2;
using vec3_t = glm::vec3;
using boolvec3_t = glm::bvec3;
using vec4_t = glm::vec4;

using u8vec4_t = glm::u8vec4;

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
#define R2v(x,y) vec2_t{R(x), R(y)}

#define PI_OVER_2 glm::half_pi<real_t>()
#define PI glm::pi<real_t>()

extern const real_t PI_OVER_6;

#define I(x) static_cast<int>(x)
#define U32(x) static_cast<uint32_t>(x)

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

struct vertex {
  vec3_t position;
  vec4_t color;
  vec3_t normal;
  vec2_t uv;
};

struct type_module;
struct framebuffer_ops;
struct module_programs;
struct module_textures;
struct module_geom;
struct module_models;
struct module_vertex_buffer;
struct scene_graph;
struct shader_uniform_storage;
struct view_data;

namespace gapi {
class device;

// hardly ideal in terms of organization,
// but we'll deal with that later.
enum class backend : uint8_t {
  vulkan = 0,
  opengl
};
}

class device_context;

struct render_loop;

struct modules {
  framebuffer_ops* framebuffer {nullptr};
  module_programs* programs {nullptr};
  module_textures* textures {nullptr};
  module_geom* geom {nullptr};
  module_models* models {nullptr};
  module_vertex_buffer* vertex_buffer {nullptr};
  scene_graph* graph {nullptr};
  shader_uniform_storage* uniform_store {nullptr};
  view_data* view {nullptr};
  device_context* device_ctx {nullptr};
  gapi::device* gpu {nullptr};
  render_loop* loop{nullptr};

  bool init();
  void free();
} extern g_m;

enum class render_loop_type {
  complete,
  triangle
};

struct runtime_config {
  enum drawmode {
    drawmode_normal,
    drawmode_debug_mousepick
  };
 
#if CONFIG_QUAD_CLICK_CURSOR == 1
  bool quad_click_cursor {true};
#else
  bool quad_click_cursor {false};
#endif

  bool fullscreen {false};

  gapi::backend api_backend{gapi::backend::vulkan};

  render_loop_type loop {render_loop_type::triangle};

  drawmode dmode {drawmode_normal};
} extern g_conf;

//extern std::vector<type_module*> g_modules;

struct type_module {
};

template <typename numType>
static inline numType unset() {
  return static_cast<numType>(-1);
}

static inline mat4_t m4i() {
  return mat4_t {R(1)};
}

static const real_t k_to_rgba8 = R(1) / R(255);

template <typename T>
static inline bool c_in(const T& needle, const darray<T>& haystack) {
  bool k = false;
  for (const T& x: haystack) {
    if (needle == x) {
      k = true;
      break;
    }
  }
  return k;
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

template <typename T>
static inline const T* null_if_empty(const std::vector<T>& v) {
  return v.empty() ? nullptr : v.data();
}

template <class Type>
static inline darray<Type> darray_clone(const darray<Type>& in) {
  darray<Type> x {in.begin(), in.end()};
  return x;
}


template <class T>
static inline bool in_range(const T& a, const T& x, const T& b) {
  return a <= x && x <= b;
}

template <class T>
static inline T neg_1_to_1(const T& x) {
  static_assert(std::numeric_limits<T>::is_signed);
  ASSERT(T(0) <= x && x <= T(1));
  return T(2) * x - T(1);
}

static inline bool reqeps(real_t a, real_t b) {
  real_t e = R(0.01);
  real_t d = a - b;
  return -e <= d && d <= e;
}

template <class intType>
static inline intType next_power_2(const intType& x) {
  static_assert(std::is_integral<intType>::value, "type must be integral");

  constexpr size_t max_bits = sizeof(x) * 8;
  constexpr size_t term_bits = max_bits / 2;
  
  intType y{x};
  y--;

  intType iter{1};

  while (iter <= term_bits) {
    y = y | (y >> iter);
    iter <<= 1;
  }

  y++;
  
  return y;
}

template <class intType>
static inline bool is_power_2(const intType& x) {
  return x == next_power_2(x);
}

template <class numType>
static constexpr inline numType num_max() {
  return std::numeric_limits<numType>::max();
}

template <class intType, class structType, template <class T> class containerType>
static intType sum(const containerType<structType>& v, std::function<intType (const structType&)> f) {
  intType k = intType(0);
  for (const auto& u: v) {
    k += f(u);
  }
  return k;
}

template <class intType, class sizeDriverType>
class index_traits {
private:
  const sizeDriverType& m_size_driver;
  
public:
  typedef intType index_type;
  typedef index_traits<intType, sizeDriverType> index_traits_this_type;

  index_traits(const sizeDriverType& driver)
    : m_size_driver{driver}
  {}
  
  static_assert(std::is_integral<index_type>::value, "indices must be an integer type.");
  static constexpr inline index_type k_unset = static_cast<index_type>(-1);

  template <class T>
  T length() const {
    static_assert(std::is_integral<T>::value, "T must be an integer.");
    return static_cast<T>(m_size_driver.size());
  }

  index_type length() const {
    return length<index_type>();
  }

  bool ok_index(index_type index) const {
    bool b =
      index != k_unset &&
      index < length();
    ASSERT(b);
    return b;
  }
};

template <class srcType, class destType>
static inline darray<destType> c_fmap(const darray<srcType>& in, std::function<destType(const srcType&)> map_fn) {
  darray<destType> ret{};
  for (const srcType& x: in) {
    ret.push_back(map_fn(x));
  }
  return ret;
}
