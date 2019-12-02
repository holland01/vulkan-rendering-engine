#pragma once

#include "common.hpp"
#include "textures.hpp"
#include "util.hpp"
#include "programs.hpp"
#include "frame.hpp"
#include "models.hpp"
#include "scene_graph.hpp"
#include "view_data.hpp"

#include "gapi.hpp"

#include <sstream>

struct duniform;

struct shader_uniform_storage {
  typedef uint8_t buffer_offset_t;

  enum uniform_type {
    uniform_mat4x4 = 0,
    uniform_pointlight,
    uniform_material,
    uniform_vec2,
    uniform_vec3,
    uniform_vec4,
    uniform_int32,
    uniform_float32
  };

  darray<mat4_t> mat4x4_store;
  darray<dpointlight> pointlight_store;
  darray<dmaterial> material_store;
  darray<vec2_t> vec2_store;
  darray<vec3_t> vec3_store;
  darray<vec4_t> vec4_store;
  darray<int32_t> int32_store;
  darray<float> float32_store;

  static const inline size_t MAX_BUFFER_OFFSET = (1 << ((8 * sizeof(buffer_offset_t)) - 1));

  struct datum {
    uniform_type uniform_buffer;
    buffer_offset_t uniform_buffer_offset;
  };

  std::unordered_map<std::string, datum> datum_store;

  template <class uniformType,
    uniform_type unif_type>
    void set_uniform(const std::string& name,
                     const uniformType& v,
                     darray<uniformType>& store);

  void set_uniform(const std::string& name, const mat4_t& m);
  void set_uniform(const std::string& name, const vec2_t& v);
  void set_uniform(const std::string& name, const vec3_t& v);
  void set_uniform(const std::string& name, const vec4_t& v);
  void set_uniform(const std::string& name, int32_t i);
  void set_uniform(const std::string& name, float f);
  void set_uniform(const std::string& name, const dpointlight& pl);
  void set_uniform(const std::string& name, const dmaterial& m);

  void set_uniform(const duniform& uniform);

  void upload_uniform(const std::string& name) const;
};

struct duniform {
  union {
    mat4_t m4;
    dpointlight pl;
    dmaterial mat;
    vec2_t v2;
    vec3_t v3;
    vec4_t v4;
    int32_t i32;
    float f32;
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

  duniform(dmaterial m, const std::string& n)
    : mat(m),
    name(n),
    type(shader_uniform_storage::uniform_material)
  {}

  duniform(vec2_t v, const std::string& n)
    : v2(v),
    name(n),
    type(shader_uniform_storage::uniform_vec2)
  {}

  duniform(vec3_t v, const std::string& n)
    : v3(v),
    name(n),
    type(shader_uniform_storage::uniform_vec3)
  {}

  duniform(vec4_t v, const std::string& n)
    : v4(v),
    name(n),
    type(shader_uniform_storage::uniform_vec4)
  {}

  duniform(int i, const std::string& n)
    : i32(i),
    name(n),
    type(shader_uniform_storage::uniform_int32)
  {}

  duniform(float f, const std::string& n)
    : f32(f),
    name(n),
    type(shader_uniform_storage::uniform_float32)
  {}
};

static inline darray<duniform> duniform_toggle_quad() {
  return {
    duniform {R4v(0, 1, 1, 1), "unif_ToggleQuadColor"},
    duniform {R2(0), "unif_ToggleQuadScreenXY"},
    duniform {I(0), "unif_ToggleQuadEnabled"}
  };
}

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
    frame_envmap,
    frame_texture2d,
    frame_render_to_quad
  };

  using draw_fn_type = std::function<void()>;
  using init_fn_type = std::function<void()>;

  std::string name;

  gapi::state state {};

  mutable darray<duniform> uniforms; // cleared after initial upload

  darray<bind_texture> tex_bindings;

  frame_type frametype {frame_user};

  module_programs::id_type shader;

  init_fn_type init_fn;

  scene_graph::predicate_fn_type select_draw_predicate; // determines which objects are to be rendered

  framebuffer_ops::index_type fbo_id {framebuffer_ops::k_uninit}; // optional

  bool active {true};

  scene_graph::permodel_unif_fn_type permodel_unif_fn;

  mutable darray<std::string> uniform_names; // no need to set this.

  void draw() const {
    g_m.graph->draw_all();
  }

  void add_pointlight(const dpointlight& pl, int which) {
    ASSERT(which < NUM_LIGHTS);
    std::string name = "unif_Lights[" + std::to_string(which) + "]";
    uniforms.push_back(duniform {pl, name});
  }

  void add_material(const std::string& name, const dmaterial& m) {
    uniforms.push_back(duniform {m, name});
  }

  void add_vec3(const std::string& name, const vec3_t& v) {
    uniforms.push_back(duniform {v, name});
  }

  void add_float(const std::string& name, float f) {
    uniforms.push_back(duniform {f, name});
  }

  void apply() const {
    if (active) {
      CLOG(logflag_render_pipeline_pass_info_apply, "pass: %s", name.c_str());

      if (frametype != frame_render_to_quad) {
        g_m.vertex_buffer->bind();
      }

      use_program u(shader);

      for (const auto& bind: tex_bindings) {
        CLOG(logflag_render_pipeline_pass_info_apply, "binding texture: %s", bind.to_string().c_str());
        g_m.textures->bind(bind.id, bind.slot);
      }

      if (!uniforms.empty()) {
        for (const auto& unif: uniforms) {
          g_m.uniform_store->set_uniform(unif);
          uniform_names.push_back(unif.name);
        }
      }

      uniforms.clear();

      if (init_fn) {
        init_fn();
      }

      for (const auto& name: uniform_names) {
        g_m.uniform_store->upload_uniform(name);
      }

      if (select_draw_predicate) {
        g_m.graph->select_draw(select_draw_predicate);
      }

      g_m.graph->permodel_unif_set_fn = permodel_unif_fn;

      switch (frametype) {
      case frame_user:
      {
        g_m.gpu->apply_state(state);
        draw();
      } break;

      case frame_render_to_quad:
        g_m.gpu->apply_state(state);
        g_m.gpu->vertex_array_draw_without_vertex_buffer(gapi::raster_method::triangle_strip,
                                                         0,
                                                         4);
        break;

      case frame_texture2d:
      {
        ASSERT(fbo_id != framebuffer_ops::k_uninit);
        g_m.framebuffer->fbos->bind(fbo_id);
        g_m.gpu->apply_state(state);
        draw();
        g_m.framebuffer->fbos->unbind(fbo_id);
      } break;

      case frame_envmap:
      {
        ASSERT(fbo_id != framebuffer_ops::k_uninit);
        g_m.models->framebuffer_pinned = true;
        g_m.framebuffer->rcube->bind(fbo_id);

        for (auto i = 0; i < 6; ++i) {
          g_m.view->bind_view(g_m.framebuffer->rcube->set_face(fbo_id, static_cast<framebuffer_ops::render_cube::axis>(i)));
          g_m.gpu->apply_state(state);
          draw();
        }

        g_m.framebuffer->rcube->unbind();
        g_m.view->unbind_view();
        g_m.models->framebuffer_pinned = false;
      } break;
      }

      g_m.graph->permodel_unif_set_fn = scene_graph::permodel_unif_fn_type();

      for (const auto& bind: tex_bindings) {
        g_m.textures->unbind(bind.id);
      }

      if (frametype != frame_render_to_quad) {
        g_m.vertex_buffer->unbind();
      }
    }
  }
};

