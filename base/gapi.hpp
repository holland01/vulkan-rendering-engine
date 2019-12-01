#pragma once

#include "common.hpp"

#include<variant>

struct gl_state;

// An overview of the type system for gapi follows.
//
// Because we want to be able to support both OpenGL and Vulkan (at least until the Vulkan implementation is complete),
// we have to have a graphics API independent layer that more or less acts as the middleware between the client 
// renderer logic (applying render passes, choosing which shader programs to use, selecting appropriate state variables
// controlled through the underlying graphics API such as face culling or depth buffer clear values, etc.).
//
// OpenGL has very little notion of type safety; for the most part, its object handles are represented by the GLuint type,
// which typically is just unsigned 32 bit integer. This GLuint can represent a texture, a vertex buffer, a program, or
// a framebuffer object - among other things.
//
// Normally, with OpenGL specifically, this is fairly easy to deal with. However, we also are integrating Vulkan into the picture.
//
// Safety is a big concern for this new implementation: Vulkan is a large API, but it's also designed to give the programmer
// as much control as is humanly possible without revealing hardware specific details that distract from the overall
// API semantics. 
//
// Part of this means that various features that we take for granted in OpenGL, such as synchronization or validation, are left (mostly) up to us.
// It's true that Vulkan allows for "layers", which are similar to extensions. Of these layers, a validation layer is often available. However,
// it's support is by no means guaranteed: it's perfectly possible to have a minimally working Vulkan implementation available on the target system,
// albeit without support for safety checks.
//
// To combat this possibility, and to also carve out another segment which allows for us to catch bugs more easily on the client side, 
// this API uses distinct handles which store graphics API independent values. Each handle has its own corresponding type. A texture object handle
// will be referred to as gapi::texture_object_handle, for example. 
//
// Each handle has its own operator bool implementation which performs a quick check against its value.
//
// Assuming that the value is what would be given to a handle that is uninitialized, we return false. Otherwise, we return true.
// It's important that we separate the idea of a handle which is used to unbind a particular API object and an invalid/uninitialized handle.
// 
// This way, we can define our own global variables that are used specifically for unbinding API state mappings to a given handle type. 
// For example, when we are finished using a shader program, we can call:
//
// device->use_program(k_program_none); // void gapi::device::use_program(program_handle h)
//
// This has its own distinct value that _isn't_ a none value that's used by OpenGL. It keeps the intent separate, and removes the burden
// of the user from having to rely on constant integer values to denote this kind of action. It also makes maintenance
// easier, since any underlying implementations in the gapi namespace can be changed without having to modify client code.
//
// So, it's important to recognize that "none" is _not_ the same thing as "invalid".
//
// The operator bool check on these "none" objects will pass, given that their values are totally different.
//
// The benefit of the operator bool check is that we reduce the risk of changing state in the actual graphics API that 
// may be either
//
// a) result in undefined behavior (since if the check failed we'd be using the handle's internal value to dictate some kind of potential dominoe effect), or
// b) just create erronous changes that will make diagnostics take longer.
// -----
// RULES
// -----
// At the end of the day, handle::operator bool() should return false ONLY in situations where:
//
// a) the handle hasn't been initialized yet, or 
// b) the handle has been invalidated (due to freed resources or error or whatever)
//
// handle::set_null() should only be called in cases where b) applies.
//------
// "none" constant values (e.g., gapi::k_program_none) for each handle types are used in situations where:
//
// a) a handle is mapped to an external key, it's queried with that key, and an entry using said key isn't found; or
// b) a previously bound handle is mapped to a set of specific API functions, and we want to use a "none" variant of the handle's _type_
//    to _unmap_ the currently mapped API functionality. 
//
//      An example of case b) would be in binding/unbinding a program handle, or a vertex buffer handle.

namespace gapi {

using handle_int_t = int64_t;
using int_t = int64_t;
using dimension_t = int64_t;
using miplevel_t = uint8_t;
using bytesize_t = int64_t;

enum class backend : uint8_t {
  vulkan = 0,
  opengl
};

enum class handle_type {
  undefined = 0,
  program_uniform,
  program_unit,
  program,
  vertex_binding_desc,
  buffer_object,
  framebuffer_object,
  texture_object
};

enum class shader_type {
  vertex = 0,
  fragment
};

enum class cmp_func_type : uint8_t {
  less = 0,
  greater,
  equal,
  always,
  none,
  gequal,
  lequal
};

enum class face_type {
  front,
  back,
  front_and_back
};

enum class winding_order {
  cw,
  ccw
};

enum class texture_fmt {
  rgba,
  srgb_a,
  depth_component,
};

enum class texture_int_fmt {
  rgba8,
  srgb8_alpha8,
  depth_component,
  depth_component16,
  depth_component24
};

enum class texture_mag_filter {
  linear,
  nearest
};

enum class texture_min_filter {
  linear, 
  nearest
};

enum class texture_wrap_mode {
  repeat,
  clamp_to_edge
};

enum class texture_object_target {
  texture_2d,

  texture_cube_map,
  
  texture_cube_map_px,
  texture_cube_map_nx,

  texture_cube_map_py,
  texture_cube_map_ny,

  texture_cube_map_pz,
  texture_cube_map_nz
};

enum class primitive_type {
  unsigned_byte,
  floating_point
};

enum class fbo_attach_type {
  color0,
  depth
};

enum class fbo_target {
  read,
  write,
  readwrite
};

class texture_param {
public:
  enum class type {
    mag_filter,
    min_filter,
    wrap_mode_s,
    wrap_mode_t,
    wrap_mode_r,
    mipmap_base_level,
    mipmap_max_level
  };
  
  using value_type = std::variant<uint8_t, 
                                  texture_mag_filter, 
                                  texture_min_filter,
                                  texture_wrap_mode>;
private:
  type m_type{type::mipmap_base_level};
  value_type m_value{0};

public:
  texture_param& mag_filter(texture_mag_filter f) {
    m_type = type::mag_filter;
    m_value = f;
    return *this;
  }

  texture_param& min_filter(texture_min_filter f) {
    m_type = type::min_filter;
    m_value = f;
    return *this;
  }

  texture_param& wrap_mode_s(texture_wrap_mode m) {
    m_type = type::wrap_mode_s;
    m_value = m;
    return *this;
  }

  texture_param& wrap_mode_t(texture_wrap_mode m) {
    m_type = type::wrap_mode_t;
    m_value = m;
    return *this;
  }

  texture_param& wrap_mode_r(texture_wrap_mode m) {
    m_type = type::wrap_mode_r;
    m_value = m;
    return *this;
  }

  texture_param& mip_base_level(uint8_t l) {
    m_type = type::mipmap_base_level;
    m_value = l;
    return *this;
  }

  texture_param& mip_max_level(uint8_t l) {
    m_type = type::mipmap_max_level;
    m_value = l;
    return *this;
  }

  template <class valueType>
  valueType value() const { return std::get<valueType>(m_value); }

  type param_type() const { return m_type; }
};

typedef const texture_param& texture_param_ref;

class handle {
private:
  handle_type m_type;
  handle_int_t m_value;

public:
  static constexpr handle_int_t k_null_value = -1;

  handle(handle_type t, handle_int_t v) 
    : m_type(t), 
      m_value(v) {}

  handle() : handle(handle_type::undefined, k_null_value) {}

  operator bool() const {
    bool ret = m_value != k_null_value && m_type != handle_type::undefined;
    ASSERT(ret);
    return ret;
  }

  handle_int_t value() const { return m_value; }

  template <typename value_type>
  value_type value_as() const { return static_cast<value_type>(m_value); }

  handle_type type() const { return m_type; }

  void set_value(GLuint x) { m_value = static_cast<handle_int_t>(x); }

  void set_value(GLint x) { m_value = static_cast<handle_int_t>(x); }

  void set_null() { m_value = k_null_value; }

  void assert_ok() const {
    ASSERT(static_cast<bool>(*this));
  }
};

struct program_unit_traits {
  std::string source;
};

//
// Here we resort to some metaprogramming and a few
// macros to aid us in the generation of a slew of boilerplate.
// This obviously warrants an explanation.
//
// The handle_gen struct creates an inner class, gen_type, whose actual type
// _definition_ (as far as the compiler is concerned) is dependent
// on the template parameters passed to it. As shown below,
// these are HandleType and Traits.
//
// The HandleType can be any one of the handle_type enums defined above 
// (toward the beginning of this header file).
//
// Let's assume we didn't have the macros defined below,
// and we wanted to generate a handle for the handle_type::vertex_binding_desc entry.
// We'd (first) do the following:
//
// using vertex_binding_desc_gen = handle_gen<handle_type::vertex_binding_desc>;
//
// This "caches" the type definition into a type variable; handle_gen's template 
// parameters can be thought of as instantiating a type through a "type" constructor.
//
// the Meta type members that reside in vertex_binding_desc_gen are:
//
// 1) k_handle_type
// 2) gen_type
// 3) reference_type
// 4) mut_reference_type
//
// The "gen_type" parameter is actually vertex_binding_desc's true handle type. We can see in the
// actual struct that it inherits from two classes: the base handle class, and a Traits class
// that's defined as a template parameter with a _default value_. This default value is
// the __dummy_mixin__ struct that's shown below.
//
// C++17 in its standard is designed to optimize out __dummy_mixin__, given that it
// technically doesn't actually take up any space: it's an empty struct. It is possible
// that this optimization fails, but the reasons for it are outside the scope of this comment,
// and, at the time of this writing, there's no reason for it to not succeed (for structs that inherit
// from __dummy_mixin__)
//
// (further information here: https://en.cppreference.com/w/cpp/language/ebo)
//
// It's purpose is to provide the _option_ for additional class members
// to be added to trait handles through this metaprogramming system.
//
// Moving on, 
//
// 3 other type variables (aka aliases) must be declared as follows:
//
//  using vertex_binding_desc_handle = typename vertex_binding_desc_gen::gen_type;
//  using vertex_binding_desc_ref = typename vertex_binding_desc_gen::reference_type;
//  using vertex_binding_desc_mut_ref = typename vertex_binding_descgen::mut_reference_type;
//
// The typename parameter implies that the vertex_binding_desc_gen::gen_type definition is dependent on vertex_binding_desc_gen.
// This aspect is what keeps handle types _separate_ from each other, despite being all generated using the same handle_gen class.
//
// We then just move along and define vertex_binding_desc_ref and vertex_binding_desc_mut_ref types;
// These are the constant reference and non constant reference types, respectively. (immutability by default is favored here; hence the extra "mut" classification)
// 
// The macros then essentially remove the need to do all of the above and simply just specify
// the enum value you want to define a handle type for. They also define operator == and operator != overloads
// for the handle type, which is absolutely necessary: otherwise, the operator bool() overload that's used
// in the base handle class will be evaulated for any two handle types a and b in the following expressions:
//
// 1) a == b (and b == a)
// 2) a != b (and b != a)
// 
// For instance, if a and b are both of type vertex_binding_desc_handle, 
// and we compute "a == b", a's operator bool() function will be called, and a itself
// be replaced with the boolean result obtained the from the function (using a's data). The same exact
// thing will happen to b, using b's data on operator bool().
//
// So, we'll effectively have two boolean comparisons occurring, instead of actual
// handle values occurring (the values stored in the handles are integers)
//
// Hopefully this clears things up. Seriously, the amount of boilerplate removed is quite
// a bit here.

struct __dummy_mixin__ {};

template <handle_type HandleType, class Traits = __dummy_mixin__>
struct handle_gen {
  static constexpr handle_type k_handle_type = HandleType;
  
  class gen_type :  public Traits,
                    public handle {
  public:
    explicit gen_type(handle_int_t v = handle::k_null_value) 
      : handle(k_handle_type, v)
    {}
  };

  typedef const gen_type& reference_type;
  typedef gen_type& mut_reference_type;
};

#define DEF_TRAITED_HANDLE_TYPES(__name__, __traits_type__)                 \
  using __name__##_gen = handle_gen<handle_type::__name__, __traits_type__>;\
  using __name__##_handle = typename __name__##_gen::gen_type;              \
  using __name__##_ref = typename __name__##_gen::reference_type;           \
  using __name__##_mut_ref = typename __name__##_gen::mut_reference_type;   \
  extern const __name__##_handle k_##__name__##_none;                       \
  static inline bool operator == (__name__##_ref a, __name__##_ref b) {     \
    return a.value() == b.value();                                          \
  }                                                                         \
  static inline bool operator != (__name__##_ref a, __name__##_ref b) {     \
    return !(a == b);                                                       \
  }

#define DEF_HANDLE_TYPES(__name__) DEF_TRAITED_HANDLE_TYPES(__name__, __dummy_mixin__)

DEF_HANDLE_TYPES(program_uniform)

DEF_HANDLE_TYPES(program)
DEF_HANDLE_TYPES(vertex_binding_desc)
DEF_HANDLE_TYPES(buffer_object)
DEF_HANDLE_TYPES(framebuffer_object)
DEF_HANDLE_TYPES(texture_object)

DEF_TRAITED_HANDLE_TYPES(program_unit, program_unit_traits)

//
// For the device class.
//
// Provides the following:
//
// 1) a handle for the given handle type
// 2) a bound function which ensures that the handle type value is not equal
//    to its corresponding none type
// 3) an enforcing bound function which will crash the app if the type isn't bound
// 4) an enforcing unbound function which will crash the app if the type is bound.
//
// These are used throughout the various state machine functions

#define DEVICE_HANDLE_OPS(__name__)                           \
  __name__##_handle m_curr_##__name__ {k_##__name__##_none}; \
  bool __name__##_bound() const {                             \
    return m_curr_##__name__ != k_##__name__##_none;        \
  }                                                           \
  bool __name__##_bound_enforced() const {                    \
    auto h = __name__##_bound();                              \
    ASSERT(h);                                                \
    return h;                                                 \
  }                                                           \
  bool __name__##_unbound_enforced() const {                  \
    auto h = ! __name__##_bound();                            \
    ASSERT(h);                                                \
    return h;                                                 \
  }

class device {
private:
  DEVICE_HANDLE_OPS(framebuffer_object);

public:
  void apply_state(const gl_state& s);

  // shaders
  program_unit_handle create_shader(shader_type type);

  void delete_shader(program_unit_mut_ref program);

  void attach_shader(program_ref program, 
                     program_unit_ref shader);


  void detach_shader(program_ref program,
                     program_unit_ref shader);


  void compile_shader(program_unit_ref shader);

  bool compile_shader_success(program_unit_mut_ref shader);

  void set_shader_source( program_unit_mut_ref shader, 
                          const std::string& source);

  // programs

  program_handle create_program();

  void delete_program(program_mut_ref program);

  void link_program(program_ref program);

  bool link_program_success(program_mut_ref program);

  void use_program(program_ref program);

  program_handle make_program(const std::string& vertex, 
                              const std::string& fragment);

  // global program variables

  program_uniform_handle program_query_uniform(program_ref program, const std::string& name);

  void program_set_uniform_int(program_uniform_ref uniform, int value);

  void program_set_uniform_float(program_uniform_ref uniform, float value);

  void program_set_uniform_vec2(program_uniform_ref uniform, const vec2_t& v);

  void program_set_uniform_vec3(program_uniform_ref uniform, const vec3_t& v);

  void program_set_uniform_vec4(program_uniform_ref uniform, const vec4_t& v);

  void program_set_uniform_matrix4(program_uniform_ref uniform, const mat4_t& m);

  // textures

  texture_object_handle texture_new();

  void texture_set_param(texture_object_target target, texture_param_ref param);

  void texture_set_active_unit(int_t unit);

  void texture_bind(texture_object_target target, texture_object_ref texture);

  void texture_delete(texture_object_mut_ref texture);

  void texture_image_2d(texture_object_target target, 
                        miplevel_t mip, 
                        texture_int_fmt internal, 
                        dimension_t width, 
                        dimension_t height, 
                        texture_fmt format, 
                        primitive_type type, 
                        const void *pixels);

  void texture_get_image(texture_object_ref texture, 
                         miplevel_t level, 
                         texture_fmt fmt,
                         primitive_type ptype,
                         bytesize_t size,
                         void* out_pixels);

  // framebuffers

  framebuffer_object_handle framebuffer_new();

  void framebuffer_bind(fbo_target type, framebuffer_object_ref fbo);

  // Assumes that a framebuffer is bound
  void framebuffer_texture_2d(fbo_target target, 
                              fbo_attach_type attachment,
                              texture_object_target texture_target,
                              texture_object_ref texture,
                              miplevel_t mip);

  // Assumes that a framebuffer is bound
  void framebuffer_read_buffer(fbo_attach_type attachment);

  // Assumes that a framebuffer is bound
  void framebuffer_read_pixels(dimension_t x, 
                               dimension_t y,
                               dimension_t width,
                               dimension_t height, 
                               texture_fmt fmt,
                               primitive_type type,
                               void* pixels);

  // Assumes that a framebuffer is bound
  bool framebuffer_ok() const;

  

  // viewport

  void viewport_set(dimension_t x, dimension_t y, dimension_t width, dimension_t height);

  void viewport_get(dimension_t& out_x, dimension_t& out_y, dimension_t& out_width, dimension_t& out_height);
};



struct state {

  struct {
    bool framebuffer_srgb {true};
  } gamma {};

  struct {
    double range_near {0.0}; // [0, 1.0]
    double range_far {1.0}; // [0, 1.0] (far can be less than near as well)

    gapi::cmp_func_type func { gapi::cmp_func_type::lequal }; // GL_LESS, GL_LEQUAL, GL_GEQUAL, GL_GREATER, GL_ALWAYS, GL_NEVER

    // GL_TRUE -> buffer will be written to if test passes;
    // GL_FALSE -> no write occurs regardless of the test result
    bool mask {true};

    bool test_enabled {true};
  } depth {};

  struct {
    bool enabled {false};
    gapi::face_type face { gapi::face_type::back }; // GL_BACK, GL_FRONT, GL_FRONT_AND_BACK
    gapi::winding_order wnd_order { gapi::winding_order::ccw }; // GL_CCW or GL_CW
  } face_cull {};

  struct {
    vec4_t color_value {R(1.0)};
    real_t depth_value {R(1.0)};

    bool depth {false};
    bool color {false};
  } clear_buffers;

  struct {
    bool fbo {false};
  } draw_buffers;
};

}

