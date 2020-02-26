#pragma once

#include "vk_common.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace vulkan {
  // NOTE:
  // right handed system,
  // so positive rotation about a given axis
  // is counter-clockwise
  
  struct transform {
  private:
    mat4_t m;
    
  public:
    transform(const mat4_t& m = mat4_t{R(1.0)})
      : m{m}
    {}
    
    transform& translate(vec3_t t) {
      m = glm::translate(m, t);
      return *this;
    }

    transform& scale(vec3_t s) {
      m = glm::scale(m, s);
      return *this;
    }

    transform& rotate(vec3_t ax, real_t theta) {
      m = glm::rotate(m, theta, ax);
      return *this;
    }

    transform& operator *=(const transform& t) {
      m *= t.m;
      return *this;
    }

    transform& reset() {
      m = mat4_t{R(1.0)};
      return *this;
    }

    const mat4_t& operator()() const {
      return m;
    }
  };

  static inline transform operator * (const transform& a, const transform& b) {
    transform m(a() * b());
    return m;
  }
  
  struct mesh_builder {
    static constexpr inline real_t k_tri_ps = R(1);
    darray<transform> transforms;
    darray<vertex_list_t> models;

    transform taccum{};
    vertex_list_t vertices{};
    

    vec3_t color{R(1.0)};

    mesh_builder& set_color(vec3_t c) {
      color = c;
      return *this;
    }

    mesh_builder& set_transform(transform t) {
      taccum = t;
      return *this;
    }
    
    mesh_builder& triangle() {
      vertex_list_t v{ { R3v(-k_tri_ps, k_tri_ps, 0.0),
			  R2v(0.0, 0.0),
			  color,
			  R3v(0.0, 0.0, 1.0) }, // top left position, top left texture
			{ R3v(k_tri_ps, -k_tri_ps, 0.0),
			  R2v(1.0, 1.0),
			  color,
			  R3v(0.0, 0.0, 1.0) }, // bottom right position, bottom right texture
			{ R3v(-k_tri_ps, -k_tri_ps, 0.0),
			  R2v(0.0, 1.0),
			  color,
			  R3v(0.0, 0.0, 1.0) } };

      vertices = vertices + v;
      
      return *this;
    }    

    mesh_builder& with_translate(vec3_t t) {
      for (vertex_data& v: vertices) {
	v.position += t;
      }
      return *this;
    }

    mesh_builder& with_rotate(vec3_t ax, real_t rad) {
      mat4_t R{glm::rotate(mat4_t(R(1)), rad, ax)};
      for (vertex_data& v: vertices) {
	v.position = MAT4V3(R, v.position);
	v.normal = MAT4V3(R, v.normal);
      }
      return *this;
    }

    mesh_builder& with_scale(vec3_t s) {
      for (vertex_data& v: vertices) {
	v.position *= s;
      }
      return *this;
    }

    mesh_builder& quad() {
      mesh_builder tmp{};

      // flip first triangle to top,
      // since initially it will be
      // the lower half by default
      
      tmp
	.set_color(color)
	.set_transform(taccum)
	.triangle();

      // HACK: in place modify;
      // this should be done using a protocol
      // of some kind that makes mapping easier,
      // but for now it's less of an issue.

      tmp.vertices[1].position.y = R(k_tri_ps);
      tmp.vertices[1].st.y = R(0);
      tmp.vertices[2].position.x = R(k_tri_ps);
      tmp.vertices[2].st.x = R(1);

      // second triangle;
      // we let this remain as is
      tmp
	.triangle();

      vertices = vertices + tmp.vertices;
      
      return *this;
    }

    mesh_builder& cube() {
      mesh_builder tmp;

      tmp
	.set_color(color)
	// left face
	.quad()
	.with_rotate(R3v(0, 1, 0), // axis
		     glm::half_pi<real_t>())
	.with_translate(R3v(-1, 0, 0))
	.push()
	// right face
	.quad()
	.with_rotate(R3v(0, 1, 0), // axis
		     glm::half_pi<real_t>())
	.with_translate(R3v(1, 0, 0))
	.push()
	// up face
	.quad()
	.with_rotate(R3v(1, 0, 0),
		     -glm::half_pi<real_t>())
	.with_translate(R3v(0, 1, 0))
	.push()
	// down face
	.quad()
	.with_rotate(R3v(1, 0, 0),
		     glm::half_pi<real_t>())
	.with_translate(R3v(0, -1, 0))
	.push()
	// front face
	.quad()
	.with_translate(R3v(0, 0, 1))
	.push()
	// back face
	.quad()
	.with_translate(R3v(0, 0, -1))
	.push()
	// 
	.flatten();
      
      vertices = vertices + tmp.vertices;
      
      return *this;
    }

    mesh_builder& reset() {
      vertices.clear();
      taccum.reset();
      return *this;
    }
    
    mesh_builder& push() {
      if (c_assert(!vertices.empty())) {
	transforms.push_back(taccum);
	models.push_back(vertices);
	
	reset();
      }
      return *this;
    }

    mesh_builder& flatten() {
      if (c_assert(vertices.empty()) &&
	  c_assert(transforms.size() == models.size())) {
	
	for (const auto& v: models) {
	  vertices = vertices + v;
	}

	models.clear();

	for (const auto& t: transforms) {
	  taccum *= t;
	}

	transforms.clear();
      }
      return *this;
    }
  };  
}
