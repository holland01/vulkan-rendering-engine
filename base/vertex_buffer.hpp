#pragma once

#include "common.hpp"
#include "util.hpp"

#include <glm/gtc/constants.hpp>

struct vertex_buffer {
    darray<vertex> data;

    mutable GLuint vbo;

    vertex_buffer()
        : vbo(0) {
    }

    void bind() const {
        GL_FN(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    }

    void unbind() const {
        GL_FN(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    void push(const vertex& v) {
        data.push_back(v);
    }

    void reset() const {
        if (vbo == 0) {
            GL_FN(glGenBuffers(1, &vbo));
        }

        bind();

        GL_FN(glBufferData(GL_ARRAY_BUFFER,
                           sizeof(data[0]) * data.size(),
                           &data[0],
                           GL_STATIC_DRAW));

        unbind();
    }

    auto num_vertices() const {
        return static_cast<int>(data.size());
    }

  auto add_triangle(const vec3_t& a_position, const vec4_t& a_color, const vec3_t& a_normal, const vec2_t& a_uv,
		    const vec3_t& b_position, const vec4_t& b_color,  const vec3_t& b_normal, const vec2_t& b_uv,
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