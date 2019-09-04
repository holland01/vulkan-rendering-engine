#pragma once

#include "common.hpp"
#include "textures.hpp"
#include "util.hpp"
#include "programs.hpp"

#include <sstream>

struct shader_uniform_storage {  
  typedef uint8_t buffer_offset_t; 
  
  enum uniform_type {
    uniform_mat4x4 = 0,
    uniform_pointlight,
    uniform_vec3,
    uniform_int32
  };

  std::vector<mat4_t> mat4x4_store;
  std::vector<dpointlight> pointlight_store;
  std::vector<vec3_t> vec3_store;
  std::vector<int32_t> int32_store;


  static const inline size_t MAX_BUFFER_OFFSET = (1 << ( (8 * sizeof(buffer_offset_t)) - 1 ));
  
  struct datum {
    uniform_type uniform_buffer;
    buffer_offset_t uniform_buffer_offset;
  };

  std::unordered_map<std::string, datum> datum_store;

  template <class uniformType,
	    uniform_type unif_type>
  void set_uniform(const std::string& name,
		   const uniformType& v,
		   std::vector<uniformType>& store);

  void set_uniform(const std::string& name, const mat4_t& m);
  void set_uniform(const std::string& name, const vec3_t& v);
  void set_uniform(const std::string& name, int32_t i);
  void set_uniform(const std::string& name, const dpointlight& pl);
  
  void upload_uniform(const std::string& name) const;
};

struct duniform {
  union {
    mat4_t m4;
    dpointlight pl;
    vec3_t v3;
    int32_t i32;
  };

  std::string name;
  
  shader_uniform_storage::uniform_type type;
  
  duniform(mat4_t m, const std::string& n)
    : m4(m),
      name(n),
      type(shader_uniform_storage::uniform_mat4x4)
  {}

  duniform(dpointlight p, const std::string& n)
    : pl(p),
      name(n),
      type(shader_uniform_storage::uniform_pointlight)
  {}

  duniform(vec3_t v, const std::string& n)
    : v3(v),
      name(n),
      type(shader_uniform_storage::uniform_vec3)
  {}

  duniform(int i, const std::string& n)
    : i32(i),
      name(n),
      type(shader_uniform_storage::uniform_int32)
  {}
};

// TODO:
// make these more type safe
struct gl_state {
  
  struct {
    double range_near{0.0}; // [0, 1.0]
    double range_far{1.0}; // [0, 1.0] (far can be less than near as well)

    float clear{1.0f};
    
    GLenum func{GL_LEQUAL}; // GL_LESS, GL_LEQUAL, GL_GEQUAL, GL_GREATER, GL_ALWAYS, GL_NEVER

    // GL_TRUE -> buffer will be written to if test passes;
    // GL_FALSE -> no write occurs regardless of the test result
    GLboolean mask{GL_TRUE};

    bool test_enabled{true};
    
  } depth{};

  struct {
    bool enabled{false};
    GLenum face{GL_BACK}; // GL_BACK, GL_FRONT, GL_FRONT_AND_BACK
    GLenum wnd_order{GL_CCW}; // GL_CCW or GL_CW
  } face_cull{};

  struct {
    bool depth{false};
    bool color{false};
  } clear_buffers;

  void apply() const {
    GL_FN(glClearDepth(depth.clear));

    if (depth.test_enabled) {
      GL_FN(glEnable(GL_DEPTH_TEST));
      GL_FN(glDepthFunc(depth.func));
    } else {
      GL_FN(glDisable(GL_DEPTH_TEST));
    }

    GL_FN(glDepthMask(depth.mask));
    GL_FN(glDepthRange(depth.range_near, depth.range_far));

    if (face_cull.enabled) {
      GL_FN(glEnable(GL_CULL_FACE));
      GL_FN(glCullFace(face_cull.face));
      GL_FN(glFrontFace(face_cull.wnd_order));
    } else {
      GL_FN(glDisable(GL_CULL_FACE));
    }

    {
      GLenum bits = 0;
      bits = clear_buffers.color ? (bits | GL_COLOR_BUFFER_BIT) : bits;
      bits = clear_buffers.depth ? (bits | GL_DEPTH_BUFFER_BIT) : bits;
      if (bits != 0) {
	GL_FN(glClear(bits));
      }
    }
  }
};

struct bind_texture {
  textures::index_type id;
  int slot;

  std::string to_string() const {
    std::stringstream ss;
    ss << "bind_texture {\n"
       << AS_STRING_SS(id) SEP_SS "\n"
       << AS_STRING_SS(slot) << "\n"
       << "}";
    return ss.str();
  }
};