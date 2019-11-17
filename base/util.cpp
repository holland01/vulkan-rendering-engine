#include "util.hpp"

unsigned long long g_log_mask = 
  logflag_programs_load | 
  logflag_programs_use_program | 
  logflag_textures_bind | 
  logflag_render_pipeline_pass_info_apply;

static std::vector<std::string> g_gl_err_msg_cache;

static std::vector<std::string> g_msg_cache;

static constexpr bool g_cache_disabled = true;

static void maybe_print(std::vector<std::string>& cache, const std::string& msg, int line, const char* func, const char* file) {

  if (g_cache_disabled || std::find(cache.begin(),
                          cache.end(),
                          msg)  == cache.end()) {

    fprintf(stdout, "\n[ %s@%s:%i ]: ", func, file, line);
    printf("%s", msg.c_str());
    fputs("\n", stdout);

    cache.push_back(msg);
  }
}

void logf_impl(int line, const char* func, const char* file,
                              const char* fmt, ...);

void assert_impl(bool cond, int line, const char* func, const char* file, const char* expr) {
  if (!cond) {
    logf_impl(line, func, file, "ASSERT FAILURE: %s", expr);
    exit(EXIT_FAILURE);
  }
}

void logf_impl(int line, const char* func, const char* file,
    const char* fmt, ...)
{
  va_list arg;

  va_start(arg, fmt);

  char buffer[4096] = {0};
  vsprintf(buffer, fmt, arg);
  std::string wrapper(buffer);

  va_end(arg);

  maybe_print(g_msg_cache, wrapper, line, func, file);
}

void report_gl_error(GLenum err, int line, const char* func, const char* file,
    const char* expr)
{
  if (err != GL_NO_ERROR) {
    char msg[256];
    memset(msg, 0, sizeof(msg));

    sprintf(
            &msg[0],
            "GL ERROR (%x) [%s]: %s\n",
            err,
            expr,
            ( const char*) gluErrorString(err));

    std::string smsg(msg);

    maybe_print(g_gl_err_msg_cache, smsg, line, func, file);

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
    GL_FN(glGetShaderInfoLog(shader, ( GLsizei) (log_msg.size() - 1),
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
    GL_FN(glGetProgramInfoLog(program, ( GLsizei) (log_msg.size() - 1),
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

