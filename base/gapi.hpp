#pragma once

#include "common.hpp"

struct gl_state;

namespace gapi {

struct handle {
  GLuint m_value{0};

  operator bool() const { return m_value != 0; }
};

enum class backend : uint8_t {
  vulkan = 0,
  opengl
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

class device {
public:
  void apply_state(const gl_state& s);
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

