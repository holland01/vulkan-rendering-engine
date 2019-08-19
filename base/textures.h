#pragma once

#include <GL/glew.h>

#include "stb_image.h"
#include "common.h"
#include "util.h"

#include <stdint.h>
#include <vector>
#include <array>
#include <string>

struct textures: public type_module {
    std::vector<GLuint> tex_handles;

    std::vector<uint32_t> widths;
    std::vector<uint32_t> heights;
    std::vector<uint32_t> num_channels;

    std::vector<GLenum> types;

    using index_type = int16_t;

    static const inline index_type k_uninit = -1;
    static const inline fs::path k_root_path = fs::path("resources") / fs::path("textures");

    void free_mem() override {
        if (!tex_handles.empty()) {
            GL_FN(glDeleteTextures(static_cast<GLsizei>(tex_handles.size()), tex_handles.data()));
        }
    }

    void bind(index_type id, int slot = 0) const {
        GL_FN(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot)));
        GL_FN(glBindTexture(types[id], tex_handles[id]));
    }

    void unbind(index_type id, int slot = 0) const {
        GL_FN(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot)));
        GL_FN(glBindTexture(types[id], 0));
    }

    index_type new_texture(uint32_t width, uint32_t height, uint32_t channels, GLenum type) {
        GLuint handle = 0;
        GL_FN(glGenTextures(1, &handle));
        GL_FN(glBindTexture(type, handle));

        std::vector<GLenum> texparams;

        switch (type) {
        case GL_TEXTURE_CUBE_MAP:
            texparams.insert(texparams.end(), {
                    GL_TEXTURE_MIN_FILTER, GL_LINEAR,
                        GL_TEXTURE_MAG_FILTER, GL_LINEAR,
                        GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE,
                        GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE,
                        GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE,
                        GL_TEXTURE_BASE_LEVEL, 0,
                        GL_TEXTURE_MAX_LEVEL,0
              });
            break;
        case GL_TEXTURE_2D:
            texparams.insert(texparams.end(), {
                    GL_TEXTURE_MIN_FILTER, GL_LINEAR,
                    GL_TEXTURE_MAG_FILTER, GL_LINEAR,
                    GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE,
                    GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE,
                    GL_TEXTURE_BASE_LEVEL, 0,
                    GL_TEXTURE_MAX_LEVEL,0

                  });
            break;
        }

        ASSERT((texparams.size() & 1) == 0);

        for (auto i = 0; i < texparams.size(); i += 2ULL) {
            GL_FN(glTexParameteri(type, texparams[i], texparams[i + 1ULL]));
        }

        GL_FN(glBindTexture(type, 0));

        auto index = static_cast<index_type>(tex_handles.size());

        tex_handles.push_back(handle);
        widths.push_back(width);
        heights.push_back(height);
        num_channels.push_back(channels);
        types.push_back(type);

        return index;
    }

    using cubemap_paths_type = std::array<fs::path, 6>;

    auto new_cubemap(cubemap_paths_type paths) {
        auto cmap_id = new_texture(0, 0, 0, GL_TEXTURE_CUBE_MAP);

        bind(cmap_id);

        auto offset = 0;
        for (const auto& path: paths) {
            int w, h, chan;

            auto fullpath = k_root_path / path;
            auto fullpath_string = fullpath.string();

            unsigned char* data = stbi_load(fullpath_string.c_str(),
                                            &w,
                                            &h,
                                            &chan,
                                            0);
            ASSERT(w <= (1 << 12));
            ASSERT(h <= (1 << 12));
            ASSERT(chan <= 4);

            if (data != nullptr) {
                GLenum fmt, ifmt;
                switch (chan) {
                case 3:
                    ifmt = fmt = GL_RGB;
                    break;
                case 4:
                    ifmt = fmt = GL_RGBA;
                    break;
                default:
                    ASSERT(false);
                    break;
                }

                GL_FN(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(offset),
                                   0,
                                   ifmt,
                                   w,
                                   h,
                                   0,
                                   fmt,
                                   GL_UNSIGNED_BYTE,
                                   data));

                stbi_image_free(data);
            }
            else {
                ASSERT(false);
            }

            // we also do an integrity check to ensure that we have consistency
            // among all images. The repeated assignment is otherwise redundant,
            // but honestly: this isn't performanec critical code
#define new_cubemap_set_item(buffer, param) \
      if (buffer[cmap_id] != 0) ASSERT(buffer[cmap_id] == static_cast<uint32_t>(param)); \
      buffer[cmap_id] = static_cast<uint32_t>(param)

            new_cubemap_set_item(widths, w);
            new_cubemap_set_item(heights, h);
            new_cubemap_set_item(num_channels, chan);

#undef new_cubemap_set_item

            offset++;
        }

        unbind(cmap_id);

        return cmap_id;
    }

} extern g_textures;

