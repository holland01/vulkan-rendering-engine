#include "gapi.hpp"
#include "render_pipeline.hpp"

namespace gapi {
  void device::apply_state(const gl_state& s) {
    if (s.draw_buffers.fbo) {
      GLenum b[] = {
        GL_COLOR_ATTACHMENT0
      };
      GL_FN(glDrawBuffers(1, b));
    }
    else {
      GLenum b[] = {
        GL_BACK_LEFT
      };
      GL_FN(glDrawBuffers(1, b));
    }

    if (s.depth.test_enabled) {
      GL_FN(glEnable(GL_DEPTH_TEST));
      GL_FN(glDepthFunc(s.depth.func));
    }
    else {
      GL_FN(glDisable(GL_DEPTH_TEST));
    }

    if (s.depth.mask) {
      GL_FN(glDepthMask(GL_TRUE));
    } 
    else {
      GL_FN(glDepthMask(GL_FALSE));
    }

    GL_FN(glDepthRange(s.depth.range_near, s.depth.range_far));

    if (s.face_cull.enabled) {
      GL_FN(glEnable(GL_CULL_FACE));
      GL_FN(glCullFace(s.face_cull.face));
      GL_FN(glFrontFace(s.face_cull.wnd_order));
    }
    else {
      GL_FN(glDisable(GL_CULL_FACE));
    }

    if (s.clear_buffers.depth) {
      GL_FN(glClearDepth(s.clear_buffers.depth_value));
    }

    if (s.clear_buffers.color) {
      GL_FN(glClearColor(s.clear_buffers.color_value.r,
                         s.clear_buffers.color_value.g,
                         s.clear_buffers.color_value.b,
                         s.clear_buffers.color_value.a));
    }

    if (s.gamma.framebuffer_srgb) {
      GL_FN(glEnable(GL_FRAMEBUFFER_SRGB));
    }
    else {
      GL_FN(glDisable(GL_FRAMEBUFFER_SRGB));
    }

    {
      GLenum bits = 0;
      bits = s.clear_buffers.color ? (bits | GL_COLOR_BUFFER_BIT) : bits;
      bits = s.clear_buffers.depth ? (bits | GL_DEPTH_BUFFER_BIT) : bits;
      if (bits != 0) {
        GL_FN(glClear(bits));
      }
    }
  }
}