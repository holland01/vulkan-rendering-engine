#pragma once

#include "common.h"
#include "textures.h"
#include "util.h"
#include <memory>
#include <array>
#include <glm/gtc/matrix_transform.hpp>

struct frame {
    using index_type = int32_t;
    
    uint32_t count;
    uint32_t width;
    uint32_t height;

    using face_mats_type = std::array<glm::mat4, 6>;

    static const inline index_type k_uninit = -1;
    
    struct render_cube {
        std::vector<textures::index_type> tex_color_handles;
        std::vector<textures::index_type> tex_depth_handles;
        std::vector<GLuint> fbos;
        std::vector<vec3_t> positions;
        std::vector<face_mats_type> faces;

        const frame& self;

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

    render_cube(const frame& f) : self(f) {}

        auto calc_look_at_mats(const vec3_t& position, real_t radius) {
            real_t offset{R(1.0)};
            
            face_mats_type m;

            m[pos_x] = glm::lookAt(position + SPHERE_RIGHT(radius),
                                       position + SPHERE_RIGHT(radius + offset),
                                       V3_UP);
            
            m[neg_x] = glm::lookAt(position + SPHERE_LEFT(radius),
                                   position + SPHERE_LEFT(radius + offset),
                                   V3_UP);

            m[pos_y] = glm::lookAt(position + SPHERE_UP(radius * 3),
                                   position + SPHERE_UP(radius * 5),
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
        
        auto add(const frame& self, const vec3_t& position, real_t radius) {           
            auto rcubeid = tex_color_handles.size();
            
            tex_color_handles.push_back(g_textures.new_cubemap(self.width,
                                                         self.height,
                                                         GL_RGBA));
            
            tex_depth_handles.push_back(g_textures.new_cubemap(self.width,
                                                               self.height,
                                                               GL_DEPTH_COMPONENT));
            
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
            GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, fbos.at(cube_id)));
        }

        std::vector<uint8_t> get_pixels(index_type cube_id) const {
            auto sz = self.width * self.height * 4 * 6;
            std::vector<uint8_t> all_faces(static_cast<size_t>(sz), 0x7f);

            GL_FN(glGetTextureImage(g_textures.handle(tex_color_handles.at(cube_id)),
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
                                         g_textures.handle(tex_color_handles.at(cube_id)),
                                         0));

            GL_FN(glFramebufferTexture2D(GL_FRAMEBUFFER,
                                         GL_DEPTH_ATTACHMENT,
                                         GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face),
                                         g_textures.handle(tex_depth_handles.at(cube_id)),
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
            GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        }
    };
    

    std::unique_ptr<render_cube> rcube;
    
    frame(uint32_t w, uint32_t h)
        :   count(0),
            width(w),
        height(h),
        rcube{new render_cube(*this)}
    {}
    
    void screenshot();

    auto add_render_cube(const vec3_t& position, real_t radius) {
        return rcube->add(*this, position, radius);
    }

    auto render_cube_color_tex(index_type r) const {
        return rcube->tex_color_handles.at(r);
    }

} extern g_frame;
