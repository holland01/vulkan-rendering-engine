#pragma once

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GL/glu.h>

#include <algorithm>
#include <vector>
#include <string>
#include <stdarg.h>
#include <stdio.h>

#include <inttypes.h>


// We use the set of flags shown here to group/categorize
// arbitrary log function calls. For any flag defined, simply add it 
// as a bitwise OR to the variable g_log_mask (initialized in base/util.cpp).
// Then invoke the CLOG() macro defined below, with the flag set as its first parameter.
 
extern unsigned long long g_log_mask;

enum {
  logflag_programs_load = 1 << 0,
  logflag_programs_use_program = 1 << 1,
  logflag_textures_bind = 1 << 2,
  logflag_render_pipeline_pass_info_apply = 1 << 3
};

#define write_logf(...) logf_impl(__LINE__, __func__, __FILE__, __VA_ARGS__) 

#define CLOG(flag, ...) do { if ((g_log_mask & (flag)) != 0) { write_logf("CLOG|" __VA_ARGS__); } } while (0)
#define CLOG_CODE(code) code

// Macro'd out incase non-c++17 compilers are used.
#define STATIC_IF(cond) if constexpr ((cond))

#define __FATAL__(...) do { write_logf(__VA_ARGS__); ASSERT(false); } while (0)

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
               const char* fmt, ...);


void report_gl_error(GLenum err,
                     int line,
                     const char* func,
                     const char* file,
                     const char* expr);

GLuint make_shader(const char* source, GLenum type);

GLuint make_program(std::vector<GLuint> shaders);

GLuint make_program(const char* vertex_src, const char* fragment_src);
