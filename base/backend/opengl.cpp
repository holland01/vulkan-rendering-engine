#include "backend/opengl.hpp"

using gapi::primitive_type;
using gapi::texture_fmt;
using gapi::texture_int_fmt;
using gapi::texture_wrap_mode;
using gapi::texture_target;
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

GLenum gl_texture_target_to_enum(texture_target target) {
  GLenum ret = GL_NONE;
  switch (target) {
    case texture_target::texture_2d:
      ret = GL_TEXTURE_2D;
      break;
    case texture_target::texture_cube_map:
      ret = GL_TEXTURE_CUBE_MAP;
      break;
    case texture_target::texture_cube_map_px:
      ret = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
      break;
    case texture_target::texture_cube_map_nx:
      ret = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
      break;
    case texture_target::texture_cube_map_py:
      ret = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
      break;
    case texture_target::texture_cube_map_ny:
      ret = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
      break;
    case texture_target::texture_cube_map_pz:
      ret = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
      break;
    case texture_target::texture_cube_map_nz:
      ret = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
      break;
    default:
      BAD_ENUM;
      break;
  }
  return ret;
}
  return ret;
}