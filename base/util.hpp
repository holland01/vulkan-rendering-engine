#pragma once

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GL/glu.h>

#include <algorithm>
#include <vector>
#include <string>
#include <stdarg.h>
#include <stdio.h>

#define write_logf(...) logf_impl(__LINE__, __func__, __FILE__, __VA_ARGS__) 

// Macro'd out incase non-c++17 compilers are used.
#define STATIC_IF(cond) if constexpr ((cond))

#define GLSL(src) "#version 450 core\n" #src

#define GL_FN(expr)                                     \
  do {                                                  \
  expr;                                                 \
  report_gl_error(glGetError(), __LINE__, __func__, __FILE__, #expr); \
 } while (0)

#define ASSERT(cond) assert_impl((cond), __LINE__, __func__, __FILE__, #cond)

void assert_impl(bool cond,
                 int line,
                 const char* func,
                 const char* file,
                 const char* expr);

void logf_impl(int line,
               const char* func,
               const char* file,
               const char* fmt, ... );


void report_gl_error(GLenum err,
                     int line,
                     const char* func,
                     const char* file,
                     const char* expr);

GLuint make_shader(const char* source, GLenum type);

GLuint make_program(std::vector<GLuint> shaders);
                                          
GLuint make_program(const char* vertex_src, const char* fragment_src);
