#pragma once

#include "common.hpp"
#include "vertex_buffer.hpp"
#include "programs.hpp"
#include "view_data.hpp"

#include <functional>

struct model_material {
  float smooth; // for phong, range is (0, inf)
};

struct module_models {
  enum model_type {
    model_unknown = 0,
    model_tri,
    model_sphere,
    model_cube,
    model_quad
  };

  enum wall_type {
    wall_front = 0,
    wall_left,
    wall_right,
    wall_back,
    wall_top,
    wall_bottom
  };

  using index_type = int32_t;
  using transform_fn_type = std::function<mat4_t(index_type model)>;
  using index_list_type = darray<index_type>;
  using predicate_fn_type = std::function<bool(const index_type&)>;

  static const inline index_type k_uninit = -1;

  darray<model_type> model_types;
  darray<index_type> vertex_offsets;
  darray<index_type> vertex_counts;
  darray<model_material> material_info;

  vec3_t model_select_reset_pos {glm::zero<vec3_t>()};

  index_type model_count = 0;

  index_type modind_tri = k_uninit;
  index_type modind_sphere = k_uninit;
  index_type modind_skybox = k_uninit;
  index_type modind_area_sphere = k_uninit;
  index_type modind_selected = k_uninit;

  mutable bool framebuffer_pinned = false;

  // It's assumed that vertices
  // have been already added to the vertex
  // buffer when this function is caglled,
  // so we explicitly reallocate the needed
  // VBO memory every time we add new
  // model data with this function.
  auto new_model(
    model_type mt,
    index_type vbo_offset = 0,
    index_type num_vertices = 0,
    model_material m = model_material {}) {

    index_type id = static_cast<index_type>(model_count);

    model_types.push_back(mt);

    vertex_offsets.push_back(vbo_offset);
    vertex_counts.push_back(num_vertices);

    material_info.push_back(m);

    model_count++;

    g_m.vertex_buffer->reset();

    return id;
  }

  auto new_sphere(vec4_t color = vec4_t {R(1.0)}) {
    auto offset = g_m.vertex_buffer->num_vertices();

    real_t step = 0.05f;

    auto count = 0;

    auto cart = [](real_t phi, real_t theta) {
      vec3_t ret;
      ret.x = glm::cos(theta) * glm::cos(phi);
      ret.y = glm::sin(phi);
      ret.z = glm::sin(theta) * glm::cos(phi);
      return ret;
    };

    for (real_t phi = -glm::half_pi<real_t>(); phi <= glm::half_pi<real_t>(); phi += step) {
      for (real_t theta = 0.0f; theta <= glm::two_pi<real_t>(); theta += step) {
        auto a = cart(phi, theta);
        auto b = cart(phi, theta + step);
        auto c = cart(phi + step, theta + step);
        auto d = cart(phi + step, theta);

        g_m.vertex_buffer->add_triangle(a, color, a,
                                        d, color, d,
                                        c, color, c);

        g_m.vertex_buffer->add_triangle(c, color, c,
                                        a, color, a,
                                        b, color, b);

        count += 6;
      }
    }

    return new_model(model_sphere, offset, count);
  }

  auto new_wall(
    wall_type type,
    const vec4_t& color = R4(1.0)) {

    std::array<vec3_t, 6> normals = {
      R3v(0, 0, -1), // front
      R3v(-1, 0, 0), // left
      R3v(1, 0, 0),  // right
      R3v(0, 0, 1),  // back
      R3v(0, 1, 0),  // top
      R3v(0, -1, 0)  // bottom
    };

    // TODO:
    // rewrite this so that the API
    // takes planes XY, XZ, and YZ
    // instead of wall types,
    // since the scaling that's used
    // is going to offset the faces
    // in directions (in terms of translation)
    // which we don't want them to be offset
    // in.
    std::array<real_t, 36 * 3> vertices = {
      // positions
      // front
      -1.0f, 1.0f, 0.0f,
      -1.0f, -1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,
      -1.0f, 1.0f, 0.0f,

      // left
      0.0f, -1.0f, 1.0f,
      0.0f, -1.0f, -1.0f,
      0.0f, 1.0f, -1.0f,
      0.0f, 1.0f, -1.0f,
      0.0f, 1.0f, 1.0f,
      0.0f, -1.0f, 1.0f,

      // right
      0.0f, -1.0f, -1.0f,
      0.0f, -1.0f, 1.0f,
      0.0f, 1.0f, 1.0f,
      0.0f, 1.0f, 1.0f,
      0.0f, 1.0f, -1.0f,
      0.0f, -1.0f, -1.0f,

      // back
      -1.0f, -1.0f, 0.0f,
      -1.0f, 1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      -1.0f, -1.0f, 0.0f,

      // top
      -1.0f, 0.0f, -1.0f,
      1.0f, 0.0f, -1.0f,
      1.0f, 0.0f, 1.0f,
      1.0f, 0.0f, 1.0f,
      -1.0f, 0.0f, 1.0f,
      -1.0f, 0.0f, -1.0f,

      // bottom
      -1.0f, 0.0f, -1.0f,
      -1.0f, 0.0f, 1.0f,
      1.0f, 0.0f, -1.0f,
      1.0f, 0.0f, -1.0f,
      -1.0f, 0.0f, 1.0f,
      1.0f, 0.0f, 1.0f
    };

    auto vbo_offset = g_m.vertex_buffer->num_vertices();

    real_t* offset = &vertices[type * 18];

    vec3_t normal = normals[type];

    vec3_t a(offset[0], offset[1], offset[2]);
    vec3_t b(offset[3], offset[4], offset[5]);
    vec3_t c(offset[6], offset[7], offset[8]);

    g_m.vertex_buffer->add_triangle(a, color, normal,
                                 b, color, normal,
                                 c, color, normal);

    vec3_t d(offset[9], offset[10], offset[11]);
    vec3_t e(offset[12], offset[13], offset[14]);
    vec3_t f(offset[15], offset[16], offset[17]);

    g_m.vertex_buffer->add_triangle(d, color, normal,
                            e, color, normal,
                            f, color, normal);

    return new_model(model_quad,
                     vbo_offset,
                     6 // vertex count 
    );
  }

  auto new_cube(const vec4_t& color = vec4_t(1.0f)) {
    std::array<real_t, 36 * 3> vertices = {
      // positions          
      -1.0f, 1.0f, -1.0f,
      -1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,

      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,
      -1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,

      -1.0f, 1.0f, -1.0f,
      1.0f, 1.0f, -1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f, -1.0f,

      -1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, 1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, 1.0f,
      1.0f, -1.0f, 1.0f
    };

    auto offset = g_m.vertex_buffer->num_vertices();

    for (auto i = 0; i < vertices.size(); i += 9) {
      vec3_t a(vertices[i + 0], vertices[i + 1], vertices[i + 2]);
      vec3_t b(vertices[i + 3], vertices[i + 4], vertices[i + 5]);
      vec3_t c(vertices[i + 6], vertices[i + 7], vertices[i + 8]);

      g_m.vertex_buffer->add_triangle(a, color,
                                       b, color,
                                       c, color);
    }

    return new_model(model_cube, offset, 36);
  }

  void render(index_type model, const mat4_t& world) const {
    mat4_t mv = g_m.view->view() * world;

    if (g_m.programs->uniform("unif_Model") != -1) {
      g_m.programs->up_mat4x4("unif_Model", world);
    }

    g_m.programs->up_mat4x4("unif_ModelView", mv);
    g_m.programs->up_mat4x4("unif_Projection",
      (model == modind_skybox
       ? g_m.view->skyproj
       : (framebuffer_pinned
          ? g_m.view->cubeproj
          : g_m.view->proj)));

    auto ofs = vertex_offsets[model];
    auto count = vertex_counts[model];

    GL_FN(glDrawArrays(GL_TRIANGLES,
                       ofs,
                       count));
  }

  model_type type(index_type i) const {
    return model_types[i];
  }
};
