#pragma once

#include "common.hpp"
#include "gapi.hpp"

GLenum gl_primitive_type_to_enum(gapi::primitive_type ptype);

GLenum gl_fmt_to_enum(gapi::texture_fmt fmt);

GLint gl_int_fmt_to_int(gapi::texture_int_fmt fmt);

template <class enumType>
GLint gl_filter_to_int(enumType filter);

GLint gl_wrap_mode_to_int(gapi::texture_wrap_mode mode);

GLenum gl_texture_target_to_enum(gapi::texture_object_target target);

GLenum gl_fbo_target_to_enum(gapi::fbo_target t);

GLenum gl_fbo_attach_to_enum(gapi::fbo_attach_type a);

template <class handleType, void glGenFn(GLsizei n, GLuint* pids)>
void gl_gen_handle(handleType& h);

template <class handleType, void (** glGenFn)(GLsizei n, GLuint* pids)>
void glew_gen_handle(handleType& h);

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

template <class handleType, void glGenFn(GLsizei n, GLuint* pids)>
void gl_gen_handle(handleType& h) {
  GLuint value = 0;
  GL_FN(glGenFn(1, &value));

  if (value != 0) {
    h.set_value(value);
  } else {
    h.set_null();
  }
}

template <class handleType, void (** glGenFn)(GLsizei n, GLuint* pids)>
void glew_gen_handle(handleType& h) {
  GLuint value = 0;

  void (* g)(GLsizei n, GLuint* pids) = *glGenFn;

  GL_FN(g(1, &value));

  if (value != 0) {
    h.set_value(value);
  } else {
    h.set_null();
  }
}