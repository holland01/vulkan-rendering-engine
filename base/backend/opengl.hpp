#pragma once

#include "common.hpp"
#include "gapi.hpp"

GLenum gl_primitive_type_to_enum(gapi::primitive_type ptype);

GLenum gl_fmt_to_enum(gapi::texture_fmt fmt);

GLint gl_int_fmt_to_int(gapi::texture_int_fmt fmt);

template <class enumType>
GLint gl_filter_to_int(enumType filter);

GLint gl_wrap_mode_to_int(gapi::texture_wrap_mode mode);

GLenum gl_target_to_enum(gapi::texture_target target);

//------------------------------------------------------

template <class enumType>
GLint gl_filter_to_int(enumType filter) {
  GLint ret = GL_NONE;
  switch (filter) {
    case enumType::linear:
      ret = GL_LINEAR;
      break;
    case enumType::nearest:
      ret = GL_NEAREST;
      break;
    default:
      __FATAL__("Unknown enum value passed");
      break;
  }
  return ret;
}