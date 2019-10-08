#include "textures.hpp"
#include "stb_image.h"

module_textures::~module_textures() {
    if (!tex_handles.empty()) {
        GL_FN(glDeleteTextures(static_cast<GLsizei>(tex_handles.size()), tex_handles.data()));
    }
}

void module_textures::bind(module_textures::index_type id, int slot) const {
    slots[id] = static_cast<GLenum>(slot);
    GL_FN(glActiveTexture(GL_TEXTURE0 + slots[id]));
    GL_FN(glBindTexture(types[id], tex_handles[id]));
}

void module_textures::unbind(module_textures::index_type id) const {
    GL_FN(glActiveTexture(GL_TEXTURE0 + slots.at(id)));
    GL_FN(glBindTexture(types[id], 0));
}

module_textures::params module_textures::cubemap_params(uint32_t width, uint32_t height) {
    params p{};

    p.width = width;
    p.height = height;
    p.num_channels = 4;
    p.type = GL_TEXTURE_CUBE_MAP;
    p.data.resize(p.width * p.height * p.num_channels, 0xFF);
    
    puts("cmp");

    return p;
}

module_textures::params module_textures::depthtexture_params(uint32_t width, uint32_t height) {
    params p{};

    p.width = width;
    p.height = height;
    p.num_channels = 4;
    p.type = GL_TEXTURE_2D;
    p.min_filter = GL_NEAREST;
    p.mag_filter = GL_NEAREST;
    p.format = GL_DEPTH_COMPONENT;
    p.internal_format = GL_DEPTH_COMPONENT16;
    p.texel_type = GL_FLOAT;
    p.data.resize(p.width * p.height * sizeof(float), 0);
    
    float* f = reinterpret_cast<float*>(p.data.data());
    
    for (auto x = 0; x < p.width; ++x) {
        for (auto y = 0; y < p.height; ++y) {
            f[y * width + x] = 1.0f;
        }
    }

    puts("dtp");

    return p;
}
    
module_textures::index_type module_textures::new_texture(const module_textures::params& p) {
    GLuint handle = 0;

    GL_FN(glGenTextures(1, &handle));
    GL_FN(glBindTexture(p.type, handle));

    auto iterable = p.post();

    for (auto i = 0; i < iterable.size(); i += 2ULL) {
        GL_FN(glTexParameteri(p.type, iterable[i], iterable[i + 1ULL]));
    }

    GL_FN(glBindTexture(p.type, 0));

    auto index = static_cast<module_textures::index_type>(tex_handles.size());

    tex_handles.push_back(handle);
    widths.push_back(p.width);
    heights.push_back(p.height);
    num_channels.push_back(p.num_channels);
    internal_formats.push_back(p.internal_format);
    formats.push_back(p.format);
    num_levels.push_back(p.num_levels);
    min_filters.push_back(p.min_filter);
    mag_filters.push_back(p.mag_filter);
    types.push_back(p.type);
    texel_types.push_back(p.texel_type);
    slots.push_back(0);

    bind(index);
    switch (p.type) {
        case GL_TEXTURE_CUBE_MAP: {
            for (auto i = 0; i < 6; ++i) {
                fill_texture2d( GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(i),
                                index,
                                p.data.data());
            }
        } break;

        case GL_TEXTURE_2D:
            fill_texture2d( GL_TEXTURE_2D, index, p.data.data());
            break;
    }
    unbind(index);

    return index;
}

void module_textures::fill_cubemap_face(uint32_t offset, int w, int h, GLenum fmt, const uint8_t* data) {
    ASSERT_FMT(fmt);
    GLenum ifmt = (fmt == GL_DEPTH_COMPONENT
                        ? GL_DEPTH_COMPONENT24
                        : (fmt == GL_SRGB
                                ? GL_SRGB8_ALPHA8
                                : fmt));

    // ok for now as long as fmt is GL_RGBA, GL_RGB, GL_DEPTH_COMPONENT

    GL_FN(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(offset),
                       0,
                       ifmt,
                       w,
                       h,
                       0,
                       fmt,
                       fmt == GL_DEPTH_COMPONENT ? GL_FLOAT: GL_UNSIGNED_BYTE,
                       data));

}

void module_textures::fill_texture2d(GLenum paramtype, index_type tid, const uint8_t* data) {
    GL_FN(glTexImage2D(paramtype,
                       0,
                       internal_formats[tid],
                       widths[tid],
                       heights[tid],
                       0,
                       formats[tid],
                       texel_types[tid],
                       data));
}

GLenum module_textures::format_from_channels(int channels) const {
    GLenum format = -1;

    switch (channels) {
    case 4:
        format = GL_RGBA;
        break;
    case 3:
        format = GL_RGB;
        break;
    }
        
    ASSERT(format != -1);

    return format;
}

void fill_checkerboard(darray<uint8_t>& blank, int w, int h, glm::u8vec3 mask, int channels) {
  for (auto y = 0; y < h; ++y) {
    for (auto x = 0; x < w; ++x) {
      auto p = (y * w + x) * channels;

      uint8_t c = ((x + (y & 1)) & 1) == 1 ? 0x0 : 0xFF;
      
      blank[p + 0] = c & mask[0];
      blank[p + 1] = c & mask[1];
      blank[p + 2] = c & mask[2];
      
      if (channels == 4) {
        blank[p + 3] = 0xFF;
      }
    }
  }
}


    
module_textures::index_type module_textures::new_cubemap(cubemap_paths_type paths) {
    auto cmap_id = new_texture(cubemap_params(0, 0));

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
            auto format = format_from_channels(chan);
            fill_cubemap_face(offset, w, h, format, data);
            stbi_image_free(data);
        }
        else {
            ASSERT(false);
        }

        // we also do an integrity check to ensure that we have consistency
        // among all images. The repeated assignment is otherwise redundant,
        // but honestly: this isn't performanec critical code
#define new_cubemap_set_item(buffer, param)                             \
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

module_textures::index_type module_textures::handle(module_textures::index_type i) const {
    return tex_handles.at(i);
}


