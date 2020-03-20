#include "geom.hpp"
#include "view_data.hpp"

void module_geom::frustum::update() {
  real_t tan_half_fovy = glm::tan(g_m.view->fovy * R(0.5));

  // horizontal planes

  {
    // We compute the reference angle since we want to base the sin/cosine on the angle from the x-axis;
    // without we have an angle from the z.
    real_t fov = glm::atan(g_m.view->calc_aspect() * R(0.75) * tan_half_fovy);

    vec3_t u{g_m.view->inverse_orient[0] * glm::cos(fov)};
    vec3_t v{g_m.view->inverse_orient[2] * -glm::sin(fov)};
    vec3_t w{g_m.view->inverse_orient[1]};

    {
      vec3_t plane_line{u + v};

      m_planes[plane_right].normal = glm::cross(plane_line, -w);
      m_planes[plane_right].d = glm::dot(g_m.view->position + plane_line, m_planes[plane_right].normal);
    }

    {
      vec3_t plane_line{-u + v};

      m_planes[plane_left].normal = glm::cross(plane_line, w);
      m_planes[plane_left].d = glm::dot(g_m.view->position + plane_line, m_planes[plane_left].normal);
    }
  }

  // vertical planes

  {
    // Z is the initial axis for the horizontal planes
    real_t fov = glm::atan(tan_half_fovy);

    vec3_t u{g_m.view->inverse_orient[2] * glm::cos(fov)};
    vec3_t v{g_m.view->inverse_orient[1] * -glm::sin(fov)};
    vec3_t w{g_m.view->inverse_orient[0]};

    {
      vec3_t plane_line{-u + v};
      
      m_planes[plane_top].normal = glm::cross(w, plane_line);
      m_planes[plane_top].d = glm::dot(g_m.view->position + plane_line, m_planes[plane_top].normal);
    }

    {
      vec3_t plane_line{u + v};
      
      m_planes[plane_bottom].normal = glm::cross(w, plane_line);
      m_planes[plane_bottom].d = glm::dot(g_m.view->position + plane_line, m_planes[plane_bottom].normal);
    }
  }

  m_mvp = g_m.view->view() * g_m.view->proj;
}
