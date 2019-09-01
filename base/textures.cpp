#include "textures.hpp"
#include "stb_image.h"

textures g_textures{};

void textures::free_mem() {
    if (!tex_handles.empty()) {
        GL_FN(glDeleteTextures(static_cast<GLsizei>(tex_handles.size()), tex_handles.data()));
    }
}

void textures::bind(textures::index_type id, int slot) const {
    slots[id] = static_cast<GLenum>(slot);
    GL_FN(glActiveTexture(GL_TEXTURE0 + slots[id]));
    GL_FN(glBindTexture(types[id], tex_handles[id]));
}

void textures::unbind(textures::index_type id) const {
    GL_FN(glActiveTexture(GL_TEXTURE0 + slots.at(id)));
    GL_FN(glBindTexture(types[id], 0));
}
    
textures::index_type textures::new_texture(uint32_t width,
					   uint32_t height,
					   uint32_t channels,
					   GLenum type,
					   GLenum min_filter,
					   GLenum mag_filter) {
    GLuint handle = 0;
    GL_FN(glGenTextures(1, &handle));
    GL_FN(glBindTexture(type, handle));

    std::vector<GLenum> texparams;

    texparams.insert(texparams.end(), {
	GL_TEXTURE_MIN_FILTER, min_filter,
	  GL_TEXTURE_MAG_FILTER, mag_filter,
	  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE,
	  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE,
	  GL_TEXTURE_BASE_LEVEL, 0,
	  GL_TEXTURE_MAX_LEVEL,0
	  });

    if (type == GL_TEXTURE_CUBE_MAP) {
      texparams.insert(texparams.end(),
		       {
			 GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE
			   });
    }

    ASSERT((texparams.size() & 1) == 0);

    for (auto i = 0; i < texparams.size(); i += 2ULL) {
        GL_FN(glTexParameteri(type, texparams[i], texparams[i + 1ULL]));
    }

    GL_FN(glBindTexture(type, 0));

    auto index = static_cast<textures::index_type>(tex_handles.size());

    tex_handles.push_back(handle);
    widths.push_back(width);
    heights.push_back(height);
    num_channels.push_back(channels);
    types.push_back(type);
    slots.push_back(0);

    return index;
}

void textures::fill_cubemap_face(uint32_t offset, int w, int h, GLenum fmt, const uint8_t* data) {
    ASSERT_FMT(fmt);
    GLenum ifmt = fmt == GL_DEPTH_COMPONENT ? GL_DEPTH_COMPONENT24 : fmt; // ok for now as long as fmt is GL_RGBA, GL_RGB, GL_DEPTH_COMPONENT

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

int textures::channels_from_format(GLenum format) const {
    int channels = -1;

    switch (format) {
    case GL_RGBA:
    case GL_DEPTH_COMPONENT:
        channels = 4;
        break;
    case GL_RGB:
        channels = 3;
        break;
    }
        
    ASSERT(channels != -1);

    return channels;
}

GLenum textures::format_from_channels(int channels) const {
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

void fill_checkerboard(std::vector<uint8_t>& blank, int w, int h, glm::u8vec3 mask, int channels) {
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

// creates a blank cubemap
textures::index_type textures::new_cubemap(int w, int h, GLenum format) {
    int channels = channels_from_format(format);

    GLenum min_mag_filter = format == GL_DEPTH_COMPONENT ? GL_NEAREST : GL_LINEAR;
    
    auto cmap_id = new_texture(w,
			       h,
			       channels,
			       GL_TEXTURE_CUBE_MAP,
			       min_mag_filter,
			       min_mag_filter);

    bind(cmap_id);

    std::vector<uint8_t> blank(w * h * channels, 0);

    if (format == GL_RGBA) {
      fill_checkerboard(blank, w, h, glm::u8vec3(255, 0, 0), 4);     
      fill_cubemap_face(0, w, h, format, &blank[0]);

      fill_checkerboard(blank, w, h, glm::u8vec3(0, 255, 0), 4);
      fill_cubemap_face(1, w, h, format, &blank[0]);

      fill_checkerboard(blank, w, h, glm::u8vec3(0, 0, 255), 4);
      fill_cubemap_face(2, w, h, format, &blank[0]);

      fill_checkerboard(blank, w, h, glm::u8vec3(255, 0, 255), 4);
      fill_cubemap_face(3, w, h, format, &blank[0]);

      fill_checkerboard(blank, w, h, glm::u8vec3(255, 255, 0), 4);
      fill_cubemap_face(4, w, h, format, &blank[0]);

      fill_checkerboard(blank, w, h,  glm::u8vec3(0, 255, 255), 4);
      fill_cubemap_face(5, w, h, format, &blank[0]);
      
    } else if (format == GL_DEPTH_COMPONENT) {
      for (uint32_t i = 0; i < 6; ++i) {
	fill_cubemap_face(i, w, h, format, NULL);
      }
    } else if (format == GL_RGB) {
        for (auto y = 0; y < h; ++y) {
	  for (auto x = 0; x < w; ++x) {
	    auto p = (y * w + x) * channels;
	    blank[p + 0] = (x & 0x1) == 1 ? 0xFF : 0x7f;
	    blank[p + 1] = 0;
	    blank[p + 2] = 0;
	  }
        }
	
	for (uint32_t i = 0; i < 6; ++i) {
	  fill_cubemap_face(i, w, h, format, &blank[0]);
	}
    }

    unbind(cmap_id);

    return cmap_id;
}
    
textures::index_type textures::new_cubemap(cubemap_paths_type paths) {
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

void textures::set_tex_2d(textures::index_type tid, const uint8_t* pixels) const {
    bind(tid);
    GLenum format = format_from_channels(num_channels[tid]);
    ASSERT_FMT(format);
    GL_FN(glTexImage2D(GL_TEXTURE_2D,
                       0,
                       format,
                       widths[tid], heights[tid],
                       0,
                       format,
                       GL_UNSIGNED_BYTE,
                       pixels));
    unbind(tid);                           
}
    
textures::index_type textures::handle(textures::index_type i) const {
    return tex_handles.at(i);
}


