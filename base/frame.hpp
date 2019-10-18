#pragma once

#include "common.hpp"
#include "textures.hpp"
#include "util.hpp"
#include <memory>
#include <array>
#include <glm/gtc/matrix_transform.hpp>

struct framebuffer_ops {
  using index_type = int32_t;
  
  uint32_t count;
  uint32_t width;
  uint32_t height;

  mutable bool has_bind;

  using face_mats_type = std::array<mat4_t, 6>;

  static const inline index_type k_uninit = -1;

  struct fbodata {
    darray<uint8_t> data;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;

    u8vec4_t get(int x, int y) const {
      const uint8_t* p = &data[(y * width + x) * bpp];
      return u8vec4_t(p[0], p[1], p[2], p[3]);
    }
  };

  using fbodata_type = fbodata;
  
  #define PRE_BOUND ASSERT(self.has_bind)
  #define PRE_UNBOUND ASSERT(!self.has_bind)
  #define POST_BOUND self.has_bind = true
  #define POST_UNBOUND self.has_bind = false

  struct fbo2d {
    darray<GLuint> fbos;
    darray<uint32_t> widths;
    darray<uint32_t> heights;
    darray<module_textures::index_type> color_attachments;
    darray<module_textures::index_type> depth_attachments;

    const framebuffer_ops& self;

    fbo2d(const framebuffer_ops& s) 
      : self(s) {}

    ~fbo2d() {
      // TODO:
    }

    auto make_fbo(uint32_t width, uint32_t height) {
      GLuint fbo = 0;
      GL_FN(glGenFramebuffers(1, &fbo));
      GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

      auto depth_attachment = 
        g_m.textures->new_texture(g_m.textures->depthtexture_params(width, height));
      
      auto color_attachment = 
        g_m.textures->new_texture(g_m.textures->texture2d_rgba_params(width, height));

      GL_FN(glFramebufferTexture2D(GL_FRAMEBUFFER, 
                                    GL_COLOR_ATTACHMENT0, 
                                    GL_TEXTURE_2D, 
                                    g_m.textures->handle(color_attachment), 
                                    0));

      GL_FN(glFramebufferTexture2D(GL_FRAMEBUFFER, 
                                    GL_DEPTH_ATTACHMENT, 
                                    GL_TEXTURE_2D, 
                                    g_m.textures->handle(depth_attachment), 
                                    0));

      GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 0));

      index_type new_handle = fbos.size();
      
      fbos.push_back(fbo);
      widths.push_back(width);
      heights.push_back(height);
      color_attachments.push_back(color_attachment);
      depth_attachments.push_back(depth_attachment);

      return new_handle;
    }

    void bind(index_type handle) const {
      PRE_UNBOUND;

      GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, fbos[handle]));
      GL_FN(glViewport(0, 0, widths[handle], heights[handle]));
      
      POST_BOUND;
    }

    fbodata_type dump(index_type handle) const {
      PRE_UNBOUND;

      auto C = color_attachments.at(handle);
      
      darray<uint8_t> buffer(g_m.textures->width(C) * 
                          g_m.textures->height(C) * 
                          g_m.textures->bytes_per_pixel(C), 
                          0);

      bind(handle);
      GL_FN(glReadBuffer(GL_COLOR_ATTACHMENT0));

      GL_FN(glReadPixels(0, 
                         0, 
                         g_m.textures->width(C), 
                         g_m.textures->height(C),
                         g_m.textures->format(C), 
                         g_m.textures->texel_type(C),
                         buffer.data()));
      unbind(handle);
      
      return {  buffer, 
                g_m.textures->width(C), 
                g_m.textures->height(C), 
                g_m.textures->bytes_per_pixel(C) };
    }

    void unbind(index_type handle) const {
      PRE_BOUND;

      GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 0));

      ASSERT_CODE(
        GLint viewport[4] = { 0 }; 
        GL_FN(glGetIntegerv(GL_VIEWPORT, &viewport[0]));
        
        ASSERT(viewport[0] == 0);
        ASSERT(viewport[1] == 0);
        ASSERT(viewport[2] == widths[handle]);
        ASSERT(viewport[3] == heights[handle]);
      );

      GL_FN(glViewport(0, 0, self.width, self.height));

      POST_UNBOUND;
    }
  };

  struct render_cube {
    std::vector<module_textures::index_type> tex_color_handles;
    std::vector<module_textures::index_type> tex_depth_handles;
    std::vector<GLuint> fbos;
    std::vector<vec3_t> positions;
    std::vector<face_mats_type> faces;

    const framebuffer_ops& self;

    uint32_t cwidth;
    uint32_t cheight;

    // According to  https://www.khronos.org/registry/OpenGL/api/GL/glext.h,
    // the GL_TEXTURE_CUBE_MAP_*_(X|Y|Z)
    // enums are provided in consecutive order.
    // At the time of this writing, that order is as follows
    enum axis {
        pos_x = 0,
        neg_x,
        pos_y,
        neg_y,
        pos_z,
        neg_z
    };

    render_cube(const framebuffer_ops& f) : self(f),
      cwidth(256),
      cheight(256) {
    }

    auto calc_look_at_mats(const vec3_t& position, real_t radius) {
      real_t offset{R(1.0)};
      
      face_mats_type m;

      m[pos_x] = glm::lookAt(position + SPHERE_RIGHT(radius),
                                  position + SPHERE_RIGHT(radius + offset),
                                  V3_UP);
      
      m[neg_x] = glm::lookAt(position + SPHERE_LEFT(radius),
                              position + SPHERE_LEFT(radius + offset),
                              V3_UP);

      m[pos_y] = glm::lookAt(position + SPHERE_UP(radius),
                              position + SPHERE_UP(radius + R(5.0)),
                              V3_BACKWARD);

      m[neg_y] = glm::lookAt(position + SPHERE_DOWN(radius),
                              position + SPHERE_DOWN(radius + offset),
                              V3_FORWARD);

      m[pos_z] = glm::lookAt(position + SPHERE_BACKWARD(radius),
                              position + SPHERE_BACKWARD(radius + offset),
                              V3_UP);

      m[neg_z] = glm::lookAt(position + SPHERE_FORWARD(radius),
                              position + SPHERE_FORWARD(radius + offset),
                              V3_UP);

      return m;
    }
        
    auto add(const vec3_t& position, real_t radius) {           
      auto rcubeid = tex_color_handles.size();

      tex_color_handles.push_back(g_m.textures->new_texture(g_m.textures->cubemap_params(cwidth, cheight)));

#if defined(ENVMAP_CUBE_DEPTH)
      tex_depth_handles.push_back(g_m.textures->new_cubemap(cwidth,
                                                          cheight,
                                                          GL_DEPTH_COMPONENT));
#else
      tex_depth_handles.push_back(g_m.textures->new_texture(g_m.textures->depthtexture_params(cwidth, cheight)));
#endif // ENVMAP_CUBE_DEPTH

      positions.push_back(position);
      
      faces.push_back(calc_look_at_mats(position, radius));

      {
        GLuint fbo = 0;
        GL_FN(glGenFramebuffers(1, &fbo));
        fbos.push_back(fbo);
      }

      return static_cast<index_type>(rcubeid);
    }

    void bind(index_type cube_id) const {
      PRE_UNBOUND;

      GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 
                              fbos.at(cube_id)));

      GL_FN(glViewport( 0,
                        0,
                        cwidth,
                        cheight));

      POST_BOUND;
    }

    std::vector<uint8_t> get_pixels(index_type cube_id) const {
      auto sz = cwidth * cheight * 4 * 6;
      std::vector<uint8_t> all_faces(static_cast<size_t>(sz), 0x7f);

      GL_FN(glGetTextureImage(g_m.textures->handle(tex_color_handles.at(cube_id)),
                              0,
                              GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              static_cast<GLsizei>(all_faces.size()),
                              &all_faces[0]));

      return std::move(all_faces);
    }
        
    glm::mat4 set_face(index_type cube_id, axis face) const {
      GL_FN(glFramebufferTexture2D(GL_FRAMEBUFFER,
                                    GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face),
                                    g_m.textures->handle(tex_color_handles.at(cube_id)),
                                    0));
#ifdef ENVMAP_CUBE_DEPTH
      auto target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face);
#else
      auto target = GL_TEXTURE_2D;
#endif
  
      GL_FN(glFramebufferTexture2D(GL_FRAMEBUFFER,
                                    GL_DEPTH_ATTACHMENT,
                                    target,
                                    g_m.textures->handle(tex_depth_handles.at(cube_id)),
                                    0));

      auto fbcheck = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (fbcheck != GL_FRAMEBUFFER_COMPLETE) {
        write_logf("FRAMEBUFFER BIND ERROR: code returned = 0x%x", fbcheck);
      }
      ASSERT(fbcheck == GL_FRAMEBUFFER_COMPLETE);

      const auto& viewmats = faces.at(cube_id);
      
      return viewmats.at(face);
    }

    void unbind() {
      PRE_BOUND;

      GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 0));
      GL_FN(glViewport(0, 0, self.width, self.height));

      POST_UNBOUND;
    }
  };
    
  std::unique_ptr<render_cube> rcube;
  std::unique_ptr<fbo2d> fbos;
  
  framebuffer_ops(uint32_t w, uint32_t h)
    : count(0),
      width(w),
      height(h),
      has_bind{false},
      rcube{new render_cube(*this)},
      fbos{new fbo2d(*this)}
  {}
  
  void screenshot();

  auto add_render_cube(const vec3_t& position, real_t radius) {
    return rcube->add(position, radius);
  }

  auto add_fbo(uint32_t w, uint32_t h) {
    return fbos->make_fbo(w, h);
  }

  auto render_cube_color_tex(index_type r) const {
    return rcube->tex_color_handles.at(r);
  }
};