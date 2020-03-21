#include "geom.hpp"
#include "view_data.hpp"

#define calc_plane_normal(a, b) glm::cross(a, b)
#define calc_plane_dist(plane_index) glm::dot(m_planes.at(plane_index).point, m_planes.at(plane_index).normal)

void module_geom::frustum::update() {
  real_t tan_half_fovy = glm::tan(g_m.view->fovy * R(0.5));

  // horizontal planes

  {
    // We compute the reference angle since we want to base the sin/cosine on the angle from the x-axis;
    // without we have an angle from the z.
    real_t fov = glm::atan(g_m.view->calc_aspect() * R(0.75) * tan_half_fovy);

    // again, imagine the x-axis as the starting point for the
    // arc on the unit circle.
    vec3_t u{g_m.view->inverse_orient[0] * glm::cos(fov)};
    vec3_t v{g_m.view->inverse_orient[2] * -glm::sin(fov)};
    vec3_t w{g_m.view->inverse_orient[1]};

    // right plane
    {
      vec3_t plane_line{u + v};

      m_planes[plane_right].point = g_m.view->position + plane_line;
      m_planes[plane_right].normal = calc_plane_normal(plane_line, -w);
      m_planes[plane_right].d = calc_plane_dist(plane_right);
    }

    // left plane
    {
      vec3_t plane_line{-u + v};

      m_planes[plane_left].point = g_m.view->position + plane_line;
      m_planes[plane_left].normal = calc_plane_normal(plane_line, w);
      m_planes[plane_left].d = calc_plane_dist(plane_left);
    }
  }

  // vertical planes

  {
    // Z is the initial axis for the vertical planes.
    // Remember that in the right hand system, +Z is facing
    // backward (relative to the cardinal world axes). Since
    // we're letting +Z mark the origin, or 0 radians,
    // in order to continue the convention of counter-clockwise
    // rotation, a positive rotation moves the arc downard toward
    // the -Y axis. This is why we use -sin(fov) in the calculation
    // for v: we want it to be parallel to the +Y axis.
    real_t fov = glm::atan(tan_half_fovy);

    vec3_t u{g_m.view->inverse_orient[2] * glm::cos(fov)};
    vec3_t v{g_m.view->inverse_orient[1] * -glm::sin(fov)};
    vec3_t w{g_m.view->inverse_orient[0]};

    // top plane
    {
      vec3_t plane_line{-u + v};

      m_planes[plane_top].point = g_m.view->position + plane_line;
      m_planes[plane_top].normal = calc_plane_normal(w, plane_line);
      m_planes[plane_top].d = calc_plane_dist(plane_top);
    }

    // bottom plane
    {
      vec3_t plane_line{u + v};

      m_planes[plane_bottom].point = g_m.view->position + plane_line;
      m_planes[plane_bottom].normal = calc_plane_normal(w, plane_line);
      m_planes[plane_bottom].d = calc_plane_dist(plane_bottom);
    }
  }

  m_mvp = g_m.view->view() * g_m.view->proj;

  // far, near

  {
    // far
    {
      vec3_t plane_line{-g_m.view->inverse_orient[2] * g_m.view->farp};

      m_planes[plane_far].point = g_m.view->position + plane_line;
      m_planes[plane_far].normal = g_m.view->inverse_orient * V3_BACKWARD;
      m_planes[plane_far].d = calc_plane_dist(plane_far);
    }

    // near
    {
      vec3_t plane_line{-g_m.view->inverse_orient[2] * g_m.view->nearp};

      m_planes[plane_near].point = g_m.view->position + plane_line;
      m_planes[plane_near].normal = g_m.view->inverse_orient * V3_FORWARD;
      m_planes[plane_near].d = calc_plane_dist(plane_near);
    }
  }
  
  if (m_display_info) {
    if (m_display_tick()) {
      std::stringstream ss;
      ss << "frustum\n"
	 << "\t" << AS_STRING_SS(m_accept_count) << "\n"
	 << "\t" << AS_STRING_SS(m_reject_count);
      write_logf("%s", ss.str().c_str());
    }
    ++m_display_tick;
  }
}

#undef calc_plane_normal
#undef calc_plane_dist

// Use the plane's normal to compute a "best fit"
// offset vector that's scaled to the sphere's radius,
// and then added to the sphere's center.
// This will produce a position on the point which can be tested against
// the plane. 
bool module_geom::frustum::intersects_sphere(const bvol& s) const {
  ASSERT(s.type == bvol::type_sphere);
  bool ret = true;
  uint32_t i = 0;
  // all intersections must pass
  while (i < 4 && ret) {
    // TODO: the offset needs to test if the normal is facing toward the sphere
    // or away from it.
    
    vec3_t offset{(-glm::normalize(m_planes.at(i).normal)) * s.radius};
    vec3_t p{s.center + offset};
    
    ret = g_m.geom->sdist_point_plane(p, m_planes.at(i)) >= 0;

    if (ret) {
      i = i + 1;
    }
  }
  if (ret) {
    m_accept_count++;
  }
  else {
    m_reject_count++;
  }
  return ret;
}
