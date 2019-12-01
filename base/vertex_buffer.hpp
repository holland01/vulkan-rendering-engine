#pragma once

#include "common.hpp"
#include "util.hpp"
#include "gapi.hpp"
#include <glm/gtc/constants.hpp>

struct module_vertex_buffer {
  darray<vertex> data;

  mutable gapi::buffer_object_handle vbo;

  module_vertex_buffer()
    : vbo(gapi::buffer_object_handle{}) {
  }

  void bind() const {
    g_m.gpu->buffer_object_bind(gapi::buffer_object_target::vertex, vbo);
  }

  void unbind() const {
    g_m.gpu->buffer_object_unbind(gapi::buffer_object_target::vertex);
  }

  void push(const vertex& v) {
    data.push_back(v);
  }

  void reset() const {
    if (vbo.is_null()) {
      vbo = g_m.gpu->buffer_object_new();
    }

    bind();

    g_m.gpu->buffer_object_set_data(gapi::buffer_object_target::vertex,
                                    sizeof(data[0]) * data.size(),
                                    data.data(),
                                    gapi::buffer_object_usage::static_draw);

    unbind();
  }

  auto num_vertices() const {
    return static_cast<int>(data.size());
  }

  auto add_triangle(const vec3_t& a_position, const vec4_t& a_color, const vec3_t& a_normal, const vec2_t& a_uv,
        const vec3_t& b_position, const vec4_t& b_color, const vec3_t& b_normal, const vec2_t& b_uv,
        const vec3_t& c_position, const vec4_t& c_color, const vec3_t& c_normal, const vec2_t& c_uv) {
    vertex a = {
      a_position,
      a_color,
      a_normal,
      a_uv
    };

    vertex b = {
      b_position,
      b_color,
      b_normal,
      b_uv
    };

    vertex c = {
      c_position,
      c_color,
      c_normal,
      c_uv
    };

    auto offset = num_vertices();

    data.push_back(a);
    data.push_back(b);
    data.push_back(c);

    return offset;

  }

  auto add_triangle(const vec3_t& a_position, const vec4_t& a_color,
                    const vec3_t& b_position, const vec4_t& b_color,
                    const vec3_t& c_position, const vec4_t& c_color) {
    vec2_t defaultuv(glm::zero<vec2_t>());

    return add_triangle(a_position, a_color, a_position, defaultuv,
                        b_position, b_color, b_position, defaultuv,
                        c_position, c_color, c_position, defaultuv);
  }

  auto add_triangle(const vec3_t& a_position, const vec4_t& a_color, const vec3_t& a_normal,
        const vec3_t& b_position, const vec4_t& b_color, const vec3_t& b_normal,
        const vec3_t& c_position, const vec4_t& c_color, const vec3_t& c_normal) {
    vec2_t defaultuv(glm::zero<vec2_t>());

    return add_triangle(a_position, a_color, a_normal, defaultuv,
      b_position, b_color, b_normal, defaultuv,
      c_position, c_color, c_normal, defaultuv);
  }


};