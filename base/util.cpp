#include "common.hpp"
#include "device_context.hpp"
#include "render_loop.hpp"

#include <fstream>

#include <string.h>

#if defined(BASE_DEBUG)
unsigned long long g_log_mask = 
  logflag_programs_load | 
  logflag_programs_use_program | 
  logflag_textures_bind | 
  logflag_render_pipeline_pass_info_apply;
#else 
unsigned long long g_log_mask = 0;
#endif

static std::vector<std::string> g_gl_err_msg_cache;

static std::vector<std::string> g_msg_cache;

static constexpr bool g_cache_disabled = true;

static void die() {
  if (g_m.loop != nullptr) {
    write_logf("%s", "Terminating main loop...");
    g_m.loop->set_running(false);
  }
  else {
    exit(1);
  }
}

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
    die();
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

    die();
  }
}

std::vector<uint8_t> read_file(const std::string& path) {
  // std::ios::ate opens the file already seeked
  // to the very end
  std::ifstream file(path, std::ios::ate | std::ios::binary);

  std::vector<uint8_t> ret{};
  
  if (file.is_open()) {
    size_t fsz = file.tellg();

    ret.resize(fsz);
    
    file.seekg(0);
    file.read(reinterpret_cast<char*>(ret.data()), fsz);
    file.close();
  }

  return ret;
}

darray<std::string> string_split(const std::string& str, char split) {
  darray<std::string> v{};

  size_t marker = 0;
  size_t m = std::string::npos;
  size_t n = std::string::npos;

  m = str.find_first_of(split, marker);

  if (m != std::string::npos) {
    v.push_back(str.substr(0, m));
    
    marker = m;
    
    while (marker < str.size()) {
      m = str.find_first_of(split, marker);
    
      if (m != std::string::npos) {
	n = str.find_first_of(split, m + 1);
      
	if (n != std::string::npos) {
	  size_t len = n - (m + 1); 
	  v.push_back(str.substr(m + 1, len));
	  marker = n;
	}
	else {
	  size_t len = str.size() - (m + 1);
	  v.push_back(str.substr(m + 1, len));
	  marker = str.size();
	}
      }
      else {
	size_t len = str.size() - (marker + 1);
	if (len > 0) {
	  v.push_back(str.substr(marker, len));
	}
	marker = str.size();
      }
    }
  }

  return v;
}
