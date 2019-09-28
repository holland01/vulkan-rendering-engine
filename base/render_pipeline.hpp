#pragma once

#include "common.hpp"
#include "textures.hpp"
#include "util.hpp"
#include "programs.hpp"
#include "frame.hpp"
#include "models.hpp"
#include "scene_graph.hpp"
#include "view_data.hpp"

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
    vec4_t color_value{R(1.0)};
    real_t depth_value{R(1.0)};

    bool depth{false};
    bool color{false};
  } clear_buffers;

  struct {
    bool fbo{false};
  } draw_buffers;

  void apply() const {
    if (draw_buffers.fbo) {
      GLenum b[] = {
        GL_COLOR_ATTACHMENT0
      };
      GL_FN(glDrawBuffers(1, b));
    } else {
      GLenum b[] = {
        GL_BACK_LEFT
      };
      GL_FN(glDrawBuffers(1, b));
    }

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

    if (clear_buffers.depth) {
      GL_FN(glClearDepth(clear_buffers.depth_value));
    }

    if (clear_buffers.color) {
      GL_FN(glClearColor(clear_buffers.color_value.r,
                         clear_buffers.color_value.g,
                         clear_buffers.color_value.b,
                         clear_buffers.color_value.a));
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
  module_textures::index_type id;
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

struct pass_info {
  using ptr_type = std::unique_ptr<pass_info>;
  
  enum frame_type {
    frame_user = 0,
    frame_envmap
  };
  
  using draw_fn_type = std::function<void()>;

  std::string name;
  
  gl_state state{};
  
  darray<duniform> uniforms; // cleared after initial upload

  darray<bind_texture> tex_bindings;
  
  frame_type frametype{frame_user};
  
  module_programs::id_type shader;

  std::function<void()> init_fn;

  scene_graph::predicate_fn_type select_draw_predicate; // determines which objects are to be rendered

  framebuffer_ops::index_type envmap_id{framebuffer_ops::k_uninit}; // optional

  bool active{true};
  
  darray<std::string> uniform_names; // no need to set this.

  darray<pass_info> subpasses;

  void draw() const {
    g_m.graph->draw_all();
  }

  void add_pointlight(const dpointlight& pl, int which) {
    ASSERT(which < NUM_LIGHTS);
    std::string name = "unif_Lights[" + std::to_string(which) + "]";
    uniforms.push_back(duniform{pl, name});
  }
  
  void apply() {
    if (active) {
      write_logf("pass: %s", name.c_str());
      
      g_m.vertex_buffer->bind();
      use_program u(shader);
      
      for (const auto& bind: tex_bindings) {
        g_m.textures->bind(bind.id, bind.slot);
      }
      
      if (!uniforms.empty()) {
        for (const auto& unif: uniforms) {
          switch (unif.type) {
            case shader_uniform_storage::uniform_mat4x4:
              g_m.uniform_store ->set_uniform(unif.name, unif.m4);
              break;
            case shader_uniform_storage::uniform_pointlight:
              g_m.uniform_store ->set_uniform(unif.name, unif.pl);
              break;
            case shader_uniform_storage::uniform_vec3:
              g_m.uniform_store ->set_uniform(unif.name, unif.v3);
              break;
            case shader_uniform_storage::uniform_int32:
              g_m.uniform_store ->set_uniform(unif.name, unif.i32);
              break;	  
          }

          uniform_names.push_back(unif.name);
        }
      }

	    uniforms.clear();

      init_fn();
      
      for (const auto& name: uniform_names) {
	      g_m.uniform_store ->upload_uniform(name);
      }

      g_m.graph->select_draw(select_draw_predicate);
      
      switch (frametype) {
        case frame_user: {
	        GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	        state.apply();
	        draw();
        } break;

        case frame_envmap: {
          ASSERT(envmap_id != framebuffer_ops::k_uninit);
          g_m.models->framebuffer_pinned = true;
          g_m.framebuffer->rcube->bind(envmap_id);
         
          for (auto i = 0; i < 6; ++i) {
            g_m.view->bind_view(g_m.framebuffer->rcube->set_face(envmap_id, static_cast<framebuffer_ops::render_cube::axis>(i)));
            state.apply();
            draw();
          }
          g_m.framebuffer->rcube->unbind();
          g_m.view->unbind_view();
          g_m.models->framebuffer_pinned = false;
        } break;
      }
      for (const auto& bind: tex_bindings) {
        g_m.textures->unbind(bind.id);
      }

      g_m.vertex_buffer->unbind();
    }
  }
};