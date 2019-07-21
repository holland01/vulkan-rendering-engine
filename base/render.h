#pragma once

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <algorithm>
#include <vector>
#include <string>
#include <stdarg.h>
#include <stdio.h>

#define GLSL(src) "#version 450 core\n" #src

#define GL_FN(expr)                                     \
  do {                                                  \
  expr;                                                 \
  report_gl_error(__LINE__, __func__, __FILE__, #expr); \
 } while (0)


static inline void logf_impl( int line, const char* func, const char* file,
    const char* fmt, ... )
{
    va_list arg;

    va_start( arg, fmt );
    fprintf( stdout, "\n[ %s@%i ]: ", func, line );
    vfprintf( stdout, fmt, arg );
    fputs( "\n", stdout );
    va_end( arg );
}

#define write_logf(...) logf_impl(__LINE__, __func__, __FILE__, __VA_ARGS__) 

std::vector<std::string> g_gl_err_msg_cache;

  static inline void report_gl_error(int line, const char* func, const char* file,
    const char* expr)
{
  GLenum err = glGetError();

  if (err != GL_NO_ERROR) {
    char msg[256];
    memset(msg, 0, sizeof(msg));
        
    sprintf(
            &msg[0],
            "GL ERROR (%x) in %s@%s:%i [%s]: %s\n",
            err,
            func,
            file,
            line,
            expr,
            (const char* )gluErrorString(err));

    std::string smsg(msg);

    if (std::find(g_gl_err_msg_cache.begin(),
                  g_gl_err_msg_cache.end(),
                  smsg)
        == g_gl_err_msg_cache.end()) {
      printf("%s", smsg.c_str());
      g_gl_err_msg_cache.push_back(smsg);
    }

    exit(EXIT_FAILURE);
  }
}

GLuint make_shader(const char* source, GLenum type) {
  GLuint shader = 0;
  GL_FN(shader = glCreateShader(type));

  GL_FN(glShaderSource(shader, 1, &source, NULL));
  
  GL_FN(glCompileShader(shader));
  GLint compile_success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_success);
  
  if (compile_success == GL_FALSE) {
    GLint info_log_len;
    GL_FN(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_len));

    std::vector<char> log_msg(info_log_len + 1, 0);
    GL_FN(glGetShaderInfoLog(shader, (GLsizei)(log_msg.size() - 1),
                              NULL, &log_msg[0]));

    write_logf("COMPILE ERROR: %s\n\nSOURCE\n\n---------------\n%s\n--------------",
               &log_msg[0], source);

    GL_FN(glDeleteShader(shader));
    shader = 0;
  }

  return shader;
}

GLuint make_program(std::vector<GLuint> shaders) {
  GLuint program = 0;
  GL_FN(program = glCreateProgram());

  for (auto shader: shaders) {
    GL_FN(glAttachShader(program, shader));
  }
  
  GL_FN(glLinkProgram(program));

  for (auto shader: shaders) {
    GL_FN(glDetachShader(program, shader));
    GL_FN(glDeleteShader(shader));
  }
  
  GLint link_success = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &link_success);
  
  if (link_success == GL_FALSE) {
    GLint info_log_len;
    GL_FN(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_len));

    std::vector<char> log_msg(info_log_len + 1, 0);
    GL_FN(glGetProgramInfoLog(program, (GLsizei)(log_msg.size() - 1),
                              NULL, &log_msg[0]));

    write_logf("LINKER ERROR: \n---------------\n%s\n--------------\n",
               &log_msg[0]);

    GL_FN(glDeleteProgram(program));
    program = 0;
  }

  return program;
}
                                          
GLuint make_program(const char* vertex_src, const char* fragment_src) {
  std::vector<GLuint> shaders;

  shaders.push_back(make_shader(vertex_src, GL_VERTEX_SHADER));
  shaders.push_back(make_shader(fragment_src, GL_FRAGMENT_SHADER));

  return make_program(std::move(shaders));
}
