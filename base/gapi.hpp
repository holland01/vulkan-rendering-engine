#pragma once

#include "common.hpp"

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
// "none" constant values for each handle types are used in situations where:
//
// a) a handle is mapped to an external key, it's queried with that key, and an entry using said key isn't found; or
// b) a previously bound handle is mapped to a set of specific API functions, and we want to use a "none" variant of the handle's _type_
//    to _unmap_ the currently mapped API functionality. 
//
//      An example of case b) would be in binding/unbinding a program handle, or a vertex buffer handle.

namespace gapi {

using handle_int_t = int64_t;
using int_t = int64_t;

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
  using __name__##_handle = typename __name__##_gen::gen_type;                               \
  using __name__##_ref = typename __name__##_gen::reference_type;                            \
  using __name__##_mut_ref = typename __name__##_gen::mut_reference_type

#define DEF_HANDLE_TYPES(__name__)                         \
  using __name__##_gen = handle_gen<handle_type::__name__>;\
  using __name__##_handle = typename __name__##_gen::gen_type;              \
  using __name__##_ref = typename __name__##_gen::reference_type;           \
  using __name__##_mut_ref = typename __name__##_gen::mut_reference_type

DEF_HANDLE_TYPES(program_uniform);

DEF_HANDLE_TYPES(program);
DEF_HANDLE_TYPES(vertex_binding_desc);
DEF_HANDLE_TYPES(buffer_object);
DEF_HANDLE_TYPES(framebuffer_object);
DEF_HANDLE_TYPES(texture_object);

DEF_TRAITED_HANDLE_TYPES(program_unit, program_unit_traits);

#define DEF_HANDLE_OPS(__type__) \
  static inline bool operator == (__type__##_ref a, __type__##_ref b) {\
    return a.value() == b.value();                                     \
  }                                                                    \
  static inline bool operator != (__type__##_ref a, __type__##_ref b) {\
    return !(a == b);                                                  \
  }

DEF_HANDLE_OPS(program)
DEF_HANDLE_OPS(program_uniform)

class device {
public:
  void apply_state(const gl_state& s);

  void set_active_texture_unit(int_t unit);


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
};

extern const program_handle k_program_none;
extern const program_uniform_handle k_program_uniform_none;

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

