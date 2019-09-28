#pragma once

#include "common.hpp"
#include "util.hpp"

#include <stdint.h>
#include <vector>
#include <array>
#include <string>

#define ASSERT_FMT(fmt) ASSERT(fmt == GL_RGBA || fmt == GL_RGB || fmt == GL_DEPTH_COMPONENT)

struct module_textures: public type_module {
    std::vector<GLuint> tex_handles;

    std::vector<uint32_t> widths;
    std::vector<uint32_t> heights;
    std::vector<uint32_t> num_channels;
    mutable std::vector<GLenum> slots;
    std::vector<GLenum> types;

    using index_type = int16_t;
    using cubemap_paths_type = std::array<fs::path, 6>;
    
    static const inline index_type k_uninit = -1;
    static const inline fs::path k_root_path = fs::path("resources") / fs::path("textures");

    ~module_textures();

    void bind(index_type id, int slot = 0) const;

    void unbind(index_type id) const;
    
    index_type new_texture(uint32_t width,
			   uint32_t height,
			   uint32_t channels,
			   GLenum type,
			   GLenum min_filter = GL_LINEAR,
			   GLenum mag_filter = GL_LINEAR);

    void fill_cubemap_face(uint32_t offset, int w, int h, GLenum fmt, const uint8_t* data);

    int channels_from_format(GLenum format) const;

    GLenum format_from_channels(int channels) const;
    
    // creates a blank cubemap
    index_type new_cubemap(int w, int h, GLenum format);
    
    index_type new_cubemap(cubemap_paths_type paths);
    
    void set_tex_2d(index_type tid, const uint8_t* pixels) const;

    index_type handle(index_type i) const;
};

