#pragma once

#include "common.hpp"

struct gl_state;


namespace gapi {

using handle_int_t = uint32_t;
using int_t = int64_t;

enum class backend : uint8_t {
  vulkan = 0,
  opengl
};

enum class handle_type {
  undefined = 0,
  compiled_shader,
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
  handle(handle_type t, handle_int_t v) : m_type(t), m_value(v) {}

  handle() : handle(handle_type::undefined, 0) {}

  operator bool() const { 
    return m_value != 0 && m_type != handle_type::undefined; 
  }

  handle_int_t value() const { return m_value; }

  template <typename value_type>
  value_type value_as() const { return static_cast<value_type>(m_value); }

  handle_type type() const { return m_type; }

  void set_value(GLuint x) { m_value = static_cast<handle_int_t>(x); }

  void assert_ok() const {
    ASSERT(static_cast<bool>(*this));
  }
};

// Provides a handle subclass that can be used for type safety.
// Also a reference type for convenience...
#define DEF_HANDLE_TYPE(__type__)                                                           \
  class __type__##_handle : public handle {                                                 \
    public:                                                                                 \
      explicit __type__##_handle (handle_int_t v = 0) : handle(handle_type::__type__, v) {} \
  };                                                                                        \
  typedef const __type__##_handle& __type__##_ref;                                          \
  typedef __type__##_handle& __type__##_mut_ref

#define DEF_HANDLE_TYPE_MIXIN(__type__, __mixin__) \
  class __type__##_handle : public __mixin__, public handle {                               \
    public:                                                                                 \
      explicit __type__##_handle (handle_int_t v = 0) : handle(handle_type::__type__, v) {} \
  };                                                                                        \
  typedef const __type__##_handle& __type__##_ref;                                          \
  typedef __type__##_handle& __type__##_mut_ref

struct compiled_shader_traits {
  std::string source;
};

DEF_HANDLE_TYPE_MIXIN(compiled_shader, compiled_shader_traits);
DEF_HANDLE_TYPE(program);
DEF_HANDLE_TYPE(vertex_binding_desc);
DEF_HANDLE_TYPE(buffer_object);
DEF_HANDLE_TYPE(framebuffer_object);
DEF_HANDLE_TYPE(texture_object);

class device {
public:
  void apply_state(const gl_state& s);

  void set_active_texture_unit(int_t unit);


  // shaders

  compiled_shader_handle create_shader(shader_type type);

  void delete_shader(compiled_shader_mut_ref program);

  void attach_shader(program_ref program, 
                     compiled_shader_ref shader);


  void detach_shader(program_ref program,
                     compiled_shader_ref shader);


  void compile_shader(compiled_shader_ref shader);

  bool compile_shader_success(compiled_shader_mut_ref shader);

  void set_shader_source( compiled_shader_mut_ref shader, 
                          const std::string& source);

  // programs

  program_handle create_program();

  void delete_program(program_mut_ref program);

  void link_program(program_ref program);

  bool link_program_success(program_mut_ref program);

  void use_program(program_ref program);

  program_handle make_program(const std::string& vertex, 
                                     const std::string& fragment);
};

extern const program_handle k_null_program;

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

