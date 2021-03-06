#include "backend/opengl.hpp"

using gapi::primitive_type;
using gapi::texture_fmt;
using gapi::texture_int_fmt;
using gapi::texture_wrap_mode;
using gapi::texture_object_target;
using gapi::fbo_target;
using gapi::fbo_attach_type;
using gapi::buffer_object_target;
using gapi::raster_method;
using gapi::buffer_object_usage;

#define BAD_ENUM __FATAL__("Unknown enum value passed")

GLenum gl_primitive_type_to_enum(primitive_type ptype) {
  GLenum ret = GL_NONE;
  switch (ptype) {
    case primitive_type::unsigned_byte:
      ret = GL_UNSIGNED_BYTE;
      break;
    case primitive_type::floating_point:
      ret = GL_FLOAT;
      break;
    default:
      BAD_ENUM;
      break;
  }
  return ret;
}

GLenum gl_fmt_to_enum(texture_fmt fmt) {
  GLenum ret = GL_NONE;
  switch (fmt) {
    case texture_fmt::rgba: 
      ret = GL_RGBA;
      break;
    case texture_fmt::srgb_a:
      ret = GL_SRGB_ALPHA;
      break;
    case texture_fmt::depth_component:
      ret = GL_DEPTH_COMPONENT;
      break;
    default:
      BAD_ENUM;
      break;
  }
  return ret;
}

GLint gl_int_fmt_to_int(texture_int_fmt fmt) {
  GLint ret = GL_NONE;
  switch (fmt) {
    case texture_int_fmt::rgba8:
      ret = GL_RGBA8;
      break;
    case texture_int_fmt::srgb8_alpha8:
      ret = GL_SRGB8_ALPHA8;
      break;
    case texture_int_fmt::depth_component:
      ret = GL_DEPTH_COMPONENT;
      break;
    case texture_int_fmt::depth_component16:
      ret = GL_DEPTH_COMPONENT16;
      break;
    case texture_int_fmt::depth_component24:
      ret = GL_DEPTH_COMPONENT24;
      break;
    default:
      BAD_ENUM;
      break;
  }
  return ret;
}

GLint gl_wrap_mode_to_int(texture_wrap_mode mode) {
  GLint ret = GL_NONE;
  switch (mode) {
    case texture_wrap_mode::clamp_to_edge:
      ret = GL_CLAMP_TO_EDGE;
      break;
    
    case texture_wrap_mode::repeat:
      ret = GL_REPEAT;
      break;

    default:
      BAD_ENUM;
      break;
  }
  return ret;
}

GLenum gl_texture_target_to_enum(texture_object_target target) {
  GLenum ret = GL_NONE;
  switch (target) {
    case texture_object_target::texture_2d:
      ret = GL_TEXTURE_2D;
      break;
    case texture_object_target::texture_cube_map:
      ret = GL_TEXTURE_CUBE_MAP;
      break;
    case texture_object_target::texture_cube_map_px:
      ret = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
      break;
    case texture_object_target::texture_cube_map_nx:
      ret = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
      break;
    case texture_object_target::texture_cube_map_py:
      ret = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
      break;
    case texture_object_target::texture_cube_map_ny:
      ret = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
      break;
    case texture_object_target::texture_cube_map_pz:
      ret = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
      break;
    case texture_object_target::texture_cube_map_nz:
      ret = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
      break;
    default:
      BAD_ENUM;
      break;
  }
  return ret;
}

GLenum gl_fbo_target_to_enum(fbo_target t) {
  GLenum ret = 0;

  switch (t) {
    case fbo_target::read:
      ret = GL_READ_FRAMEBUFFER;
      break;
    
    case fbo_target::write:
    case fbo_target::readwrite:
      ret = GL_FRAMEBUFFER;
      break;

    default:
      BAD_ENUM;
      break;
  }

  return ret;
}

GLenum gl_fbo_attach_to_enum(gapi::fbo_attach_type a) {
  GLenum ret = 0;

  switch (a) {
    case fbo_attach_type::color0:
      ret = GL_COLOR_ATTACHMENT0;
      break;
    
    case fbo_attach_type::depth:
      ret = GL_DEPTH_ATTACHMENT;
      break;

    default:
      BAD_ENUM;
      break;
  }

  return ret;
}

GLenum gl_buffer_target_to_enum(gapi::buffer_object_target b) {
  GLenum ret = 0;

  switch (b) {
    case buffer_object_target::vertex:
      ret = GL_ARRAY_BUFFER;
      break;

    default:
      BAD_ENUM;
      break;
  }

  return ret;
}

GLenum gl_raster_method_to_enum(gapi::raster_method r) {
    GLenum ret = 0;

  switch (r) {
    case raster_method::triangles:
      ret = GL_TRIANGLES;
      break;

    case raster_method::triangle_strip:
      ret = GL_TRIANGLE_STRIP;
      break;

    case raster_method::lines:
      ret = GL_LINE_STRIP;
      break;

    default:
      BAD_ENUM;
      break;
  }

  return ret;
}

GLenum gl_buffer_usage_to_enum(gapi::buffer_object_usage b) {
  GLenum ret = 0;
  switch (b) {
    case buffer_object_usage::dynamic_draw:
      ret = GL_DYNAMIC_DRAW;
      break;

    case buffer_object_usage::static_draw:
      ret = GL_STATIC_DRAW;
      break;
      
    default:
      BAD_ENUM;
      break;
  }

  return ret;
}


GLenum gl_winding_order_to_enum(gapi::winding_order w) {
  GLenum ret = 0;

  switch (w) {
    case gapi::winding_order::ccw:
      ret = GL_CCW;
      break;

    case gapi::winding_order::cw:
      ret = GL_CW;
      break;
      
    default:
      BAD_ENUM;
      break;
  }

  return ret;
}

GLenum gl_face_type_to_enum(gapi::face_type f) {
  GLenum ret = 0;

  switch (f) {
    case gapi::face_type::back:
      ret = GL_BACK;
      break;

    case gapi::face_type::front:
      ret = GL_FRONT;
      break;
    
    case gapi::face_type::front_and_back:
      ret = GL_FRONT_AND_BACK;
      break;

    default:
      BAD_ENUM;
      break;
  }

  return ret;
}

static std::unordered_map<gapi::cmp_func_type, GLenum> g_cmp_func_type {
  { gapi::cmp_func_type::always, GL_ALWAYS },
  { gapi::cmp_func_type::equal, GL_EQUAL },
  { gapi::cmp_func_type::gequal, GL_GEQUAL },
  { gapi::cmp_func_type::greater, GL_GREATER },
  { gapi::cmp_func_type::lequal, GL_LEQUAL },
  { gapi::cmp_func_type::less, GL_LESS },
  { gapi::cmp_func_type::none, GL_NONE }
};

GLenum gl_cmp_func_type_to_enum(gapi::cmp_func_type c) {
  return g_cmp_func_type[c];
}