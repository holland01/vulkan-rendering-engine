#pragma once

#include "vk_common.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace vulkan {
  // NOTE:
  // right handed system,
  // so positive rotation about a given axis
  // is counter-clockwise
  // also note that winding order is clockwise.
  // texture coordinates are ranged in [0, 1]
  // and begin in the upper left region, increasing
  // downward on the v axis (right on the u axis).
  
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
    // texture coordinates: top left, top right, bottom right, bottom left.
    static constexpr inline vec2_t k_tc_tl = R2v(0, 0);
    static constexpr inline vec2_t k_tc_tr = R2v(1, 0);
    static constexpr inline vec2_t k_tc_br = R2v(1, 1);
    static constexpr inline vec2_t k_tc_bl = R2v(1, 0);
    
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

    mesh_builder& forward(const mesh_builder& mb) {
      color = mb.color;
      taccum = mb.taccum;
      return *this;
    }
    
    mesh_builder& triangle(const vec3_t& a, const vec3_t& b, const vec3_t& c,
			   const vec2_t& ta, const vec2_t& tb, const vec2_t& tc,
			   const vec3_t& ca, const vec3_t& cb, const vec3_t& cc,
			   const vec3_t& na, const vec3_t& nb, const vec3_t& nc) {
      vertex_list_t v{ { a,
			 ta,
			 ca,
			 na },
		       
		       { b,
			 tb,
			 cb,
			 nb },
		       
		       { c,
			 tc,
			 cc,
			 nc } };

      vertices = vertices + v;
      return *this;
    }
    
    mesh_builder& triangle() {
      triangle(R3v(-k_tri_ps, k_tri_ps, 0.0),  // positions
	       R3v(k_tri_ps, -k_tri_ps, 0.0),
	       R3v(-k_tri_ps, -k_tri_ps, 0.0),
	       // texture coordinates
	       k_tc_tl,
	       k_tc_br,
	       k_tc_bl,
	       // colors
	       color,
	       color,
	       color,
	       // normals
	       R3v(0.0, 0.0, 1.0),
	       R3v(0.0, 0.0, 1.0),
	       R3v(0.0, 0.0, 1.0));
      
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

    mesh_builder& sphere() {
      mesh_builder tmp{};      
      
      real_t step = 0.1f;

      auto cart = [](real_t phi, real_t theta) -> vec3_t {
		    vec3_t ret;
		    ret.x = glm::cos(theta) * glm::cos(phi);
		    ret.y = glm::sin(phi);
		    ret.z = glm::sin(theta) * glm::cos(phi);
		    return ret;
		  };

      for (real_t phi = -glm::half_pi<real_t>(); phi <= glm::half_pi<real_t>(); phi += step) {
	for (real_t theta = 0.0f; theta <= glm::two_pi<real_t>(); theta += step) {
	  auto bl = cart(phi, theta); 
	  auto br = cart(phi, theta + step);
	  auto tr = cart(phi + step, theta + step);
	  auto tl = cart(phi + step, theta);

	  // upper triangle
	  tmp.triangle(// positions
		       tl,  
		       tr,
		       br,
		       // texture coordinates
		       k_tc_tl,
		       k_tc_tr,
		       k_tc_br,
		       // colors
		       color,
		       color,
		       color,
		       // normals
		       tl,
		       br,
		       bl);

	  // lower triangle
	  tmp.triangle(// positions
		       tl,  
		       br,
		       bl,
		       // texture coordinates
		       k_tc_tl,
		       k_tc_br,
		       k_tc_bl,
		       // colors
		       color,
		       color,
		       color,
		       // normals
		       tl,
		       br,
		       bl);
	}
      }

      vertices = vertices + tmp.vertices;
      
      return *this;
    }
    
    mesh_builder& quad() {
      mesh_builder tmp{};

      // flip first triangle to top,
      // since initially it will be
      // the lower half by default
      
      tmp
	.forward(*this)
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
