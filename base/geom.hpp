#pragma once

#include "common.hpp"
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/projection.hpp>
#include <sstream>
#include <string>

struct module_geom {
  struct ray {
    vec3_t orig {0.0f};
    vec3_t dir {0.0f}; // normalized
    real_t t0 {0.0f};
    real_t t1 {0.0f};

    std::string to_string(const std::string& prefix = "Ray") const {
      std::stringstream ss;
      ss << prefix << ": { orig: " << glm::to_string(orig) << ", dir: " << glm::to_string(dir) << ", t0: " << t0 << ", t1: " << t1 << " }";
      return ss.str();
    }
  };

  struct plane {
    real_t d;
    vec3_t normal;
    vec3_t point;
  };

  const plane k_plane_yz {R(0), R3v(1, 0, 0), R3v(0, 1, 1)};

  struct bvol {
    enum volume_type {
      type_sphere = 0,
      type_aabb
    };

    vec3_t center;

    union {
      vec3_t extents; // aabb 
      real_t radius; // bounds sphere
    };

    volume_type type {type_sphere};

    std::string to_string(volume_type t) const {
      std::string r;
      switch (t) {
      case type_sphere: r = "type_sphere"; break;
      case type_aabb: r = "type_aabb"; break;
      default: ASSERT(false); break;
      }
      return r;
    }

    std::string to_string(const std::string& prefix = "bvol") const {
      std::stringstream ss;

      ss << prefix << ": { center: " << glm::to_string(center);

      switch (type) {
      case type_sphere:
        ss << ", radius: " <<  radius;
        break;
      case type_aabb:
        ss << ", extents: " << glm::to_string(extents);
        break;
      }

      ss << ", type: " << to_string(type) << " }";

      return ss.str();
    }
  };

  bvol make_bsphere(float radius, const vec3_t& center) {
    bvol b {};
    b.type = bvol::type_sphere;
    b.radius = radius;
    b.center = center;
    return b;
  }

  bool test_ray_sphere(ray& r, const bvol& s) {
    ASSERT(s.type == bvol::type_sphere);

    real_t t0 = 0.0f;
    real_t t1 = 0.0f; // solutions for t if the ray intersects 
    real_t radius2 = s.radius * s.radius;
    vec3_t L = s.center - r.orig;
    real_t tca = glm::dot(L, r.dir);
    real_t d2 = glm::dot(L, L) - tca * tca;
    if (d2 > radius2) return false;
    real_t thc = sqrt(radius2 - d2);
    t0 = tca - thc;
    t1 = tca + thc;

    if (t0 > t1) std::swap(t0, t1);

    if (t0 < 0) {
      t0 = t1; // if t0 is negative, let's use t1 instead 
      if (t0 < 0) return false; // both t0 and t1 are negative 
    }

    r.t0 = t0;
    r.t1 = t1;

    return true;
  }

  real_t dist_point_plane(const vec3_t& p, const vec3_t& normal, const vec3_t& plane_p) const {
    vec3_t pl2p {p - plane_p};
    return glm::length(glm::proj(pl2p, normal));
  }

  real_t dist_point_plane(const vec3_t& p, const plane& P) const {
    return dist_point_plane(p, P.normal, P.point);
  }

  vec3_t proj_point_plane(const vec3_t& p, const vec3_t& normal, const vec3_t& plane_p) const {
    ASSERT(glm::length(normal) <= R(1.2) && glm::length(normal) >= R(0.8));

    real_t alpha = dist_point_plane(p, normal, plane_p);
    vec3_t N {normal * alpha};
    return p + (-N);
  }

  vec3_t proj_point_plane(const vec3_t& p, const plane& P) const {
    return proj_point_plane(p, P.normal, P.point);
  }

  // http://www.ambrsoft.com/TrigoCalc/Sphere/SpherePlaneIntersection_.htm
  vec3_t sphere_plane_intersection(const bvol& v, const plane& p) const {
    ASSERT(v.type == bvol::type_sphere);

    real_t D = glm::dot(v.center, p.normal) - p.d;
    real_t L = R(1) / R(glm::dot(p.normal, p.normal));
    // intersection circle center
    vec3_t c;
    c.x = v.center.x - (p.normal.x * D) * L;
    c.y = v.center.y - (p.normal.y * D) * L;
    c.z = v.center.z - (p.normal.z * D) * L;
    return c;
  }

  bool sphere_intersects_plane(const bvol& v, const plane& p) const {
    vec3_t c{sphere_plane_intersection(v, p)};
    return glm::length(c - v.center) <= v.radius;
  }

  real_t sdist_point_plane(const vec3_t& p, const plane& plane_p) const {
    return (glm::dot(p, plane_p.normal) - plane_p.d) / glm::length(plane_p.normal);
  }

  // a, b, and c are assumed to be
  // laid out in a counter clockwise ordering.
  // on the plane which they create. It's also
  // assumed that, in the counter clock wise direction,
  // the angular distance D for each vertex to the
  // start of the axis parellel to the circle's
  // X-axis holds the relationship D_a < D_b < D_c.
  // D_c should thus hold the highest angle for the
  // circle that is formed formed in the plane created by the triangle,
  // and is large enough to contain the three points.

  vec3_t tri_normal(const vec3_t& a, const vec3_t& b, const vec3_t& c) {
    vec3_t v1 {glm::normalize(a - b)};
    vec3_t v0 {glm::normalize(c - b)};

    // right hand rule was used here to determine
    // correct ordering.
    return glm::normalize(glm::cross(v1, v0));
  }

  class frustum {
    enum
      {
       plane_top = 0,
       plane_bottom,
       plane_right,
       plane_left,
       plane_near,
       plane_far
      };
    std::array<plane, 6> m_planes{};
    mat4_t m_mvp{};
    period_counter<uint32_t> m_display_tick{600, 0, 1};
    mutable uint32_t m_accept_count{0};
    mutable uint32_t m_reject_count{0};
    bool m_display_info{true};
    
  public:
    void update();
    bool intersects_sphere(const bvol& s) const;
  };
};
