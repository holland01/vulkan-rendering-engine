#include "textures.hpp"
#include "stb_image.h"

using rgba4_t = std::array<uint8_t, 4>;

namespace {
const rgba4_t k_room_red = {200, 20, 0, 255};
const rgba4_t k_room_white = {200, 200, 150, 255};
const rgba4_t k_room_blue = {0, 50, 150, 255};

void fill_texture_data_buffer_rgb4(module_textures::texture_data_buffer& buffer,
                                   uint32_t width,
                                   uint32_t height,
                                   const rgba4_t& color) {
  buffer.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4ull, 0);
  uint8_t* pdata = buffer.data();
  for (auto y{0u}; y < height; ++y) {
    for (auto x{0u}; x < width; ++x) {
      auto ofs = (y * width + x) << 2;

      pdata[ofs + 0] = color.at(0);
      pdata[ofs + 1] = color.at(1);
      pdata[ofs + 2] = color.at(2);
      pdata[ofs + 3] = color.at(3);
    }
  }
}
}

module_textures::~module_textures() {
  if (!tex_handles.empty()) {
    GL_FN(glDeleteTextures(static_cast<GLsizei>(tex_handles.size()), tex_handles.data()));
  }
}

void module_textures::bind(module_textures::index_type id, int slot) const {
  CLOG(logflag_textures_bind, "BINDING texture index %i with API handle %i @ slot %i\n", id, tex_handles[id], slot);
  slots[id] = static_cast<GLenum>(slot);
  GL_FN(glActiveTexture(GL_TEXTURE0 + slots[id]));
  GL_FN(glBindTexture(types[id], tex_handles[id]));
}

void module_textures::unbind(module_textures::index_type id) const {
  CLOG(logflag_textures_bind, "UNBINDING texture index %i with API handle %i @ slot %i\n", id, tex_handles[id], slots[id]);
  GL_FN(glActiveTexture(GL_TEXTURE0 + slots.at(id)));
  GL_FN(glBindTexture(types[id], 0));
}

module_textures::params module_textures::cubemap_params(uint32_t width, uint32_t height) {
  auto channels = 4;
  return cubemap_params(width,
                          height,
                          channels,
                          cubemap_data::all(width * height * channels, 0xFF));
}

module_textures::params module_textures::cubemap_params(uint32_t width, uint32_t height, cubemap_preset preset) {
  cubemap_data d {};
  params p {};
  auto channels = 4;

  switch (preset) {
  case cubemap_preset_test_room_0:
  {
    fill_texture_data_buffer_rgb4(d.px, width, height, k_room_white);
    fill_texture_data_buffer_rgb4(d.nx, width, height, k_room_white);

    fill_texture_data_buffer_rgb4(d.py, width, height, k_room_blue);
    fill_texture_data_buffer_rgb4(d.ny, width, height, k_room_blue);

    fill_texture_data_buffer_rgb4(d.pz, width, height, k_room_red);
    fill_texture_data_buffer_rgb4(d.nz, width, height, k_room_red);

    // the colors used here are currently specified in linear space,
    // so it's best we _don't_ perform linearization when sampling automatically.
    p.internal_format = GL_RGBA8;
  } break;

  default:
    ASSERT(false);
    break;
  }

  p.width = width;
  p.height = height;
  p.num_channels = channels;
  p.type = GL_TEXTURE_CUBE_MAP;
  p.data.data = d;

  return p;
}

module_textures::params module_textures::cubemap_params(uint32_t width,
                                                        uint32_t height,
                                                        uint32_t num_channels,
                                                        cubemap_data data) {
  params p {};

  p.width = width;
  p.height = height;

  // important assumption: a cubemap framebuffer should have these auto gamma correction properties,
  // since writes to the primary framebuffer will perform auto de-linearization/gamma correction
  // after the fragment shader has written its fragment. 
  p.internal_format = GL_SRGB8_ALPHA8;
  p.num_channels = num_channels;
  p.type = GL_TEXTURE_CUBE_MAP;
  p.data.data = data;

  return p;
}

module_textures::params module_textures::texture2d_rgba_params(uint32_t width,
                                                                uint32_t height) {
  params p {};

  p.width = width;
  p.height = height;
  p.num_channels = 4;
  p.type = GL_TEXTURE_2D;
  p.min_filter = GL_LINEAR;
  p.mag_filter = GL_LINEAR;
  p.format = GL_RGBA;
  p.internal_format = GL_RGBA8;
  p.texel_type = GL_UNSIGNED_BYTE;

  texture_data_buffer buffer(p.width * p.height * p.num_channels, 0);
  p.data.data = buffer;

  return p;
}

module_textures::params module_textures::depthtexture_params(uint32_t width, uint32_t height) {
  params p {};

  p.width = width;
  p.height = height;
  p.num_channels = 4;
  p.type = GL_TEXTURE_2D;
  p.min_filter = GL_NEAREST;
  p.mag_filter = GL_NEAREST;
  p.format = GL_DEPTH_COMPONENT;
  p.internal_format = GL_DEPTH_COMPONENT;
  p.texel_type = GL_FLOAT;

  texture_data_buffer buffer;
  buffer.resize(p.width * p.height * sizeof(float), 0);

  float* f = reinterpret_cast<float*>(buffer.data());

  for (auto x{0u}; x < p.width; ++x) {
    for (auto y{0u}; y < p.height; ++y) {
      f[y * width + x] = 1.0f;
    }
  }

  p.data.data = buffer;

  return p;
}

module_textures::index_type module_textures::new_texture(const module_textures::params& p) {
  GLuint handle = 0;

  GL_FN(glGenTextures(1, &handle));
  GL_FN(glBindTexture(p.type, handle));

  auto iterable = p.post();

  for (size_t i = 0; i < iterable.size(); i += 2ULL) {
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
  case GL_TEXTURE_CUBE_MAP:
  {
    const auto& cubemap_d = std::get<cubemap_data>(p.data.data);

    fill_texture2d(GL_TEXTURE_CUBE_MAP_POSITIVE_X, index, cubemap_d.px.data());
    fill_texture2d(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, index, cubemap_d.nx.data());

    fill_texture2d(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, index, cubemap_d.py.data());
    fill_texture2d(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, index, cubemap_d.ny.data());

    fill_texture2d(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, index, cubemap_d.pz.data());
    fill_texture2d(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, index, cubemap_d.nz.data());
  } break;

  case GL_TEXTURE_2D:
  {
    const auto& d = std::get<texture_data_buffer>(p.data.data);

    fill_texture2d(GL_TEXTURE_2D, index, d.data());
  } break;
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
  GLenum format = static_cast<GLenum>(-1);

  switch (channels) {
  case 4:
    format = GL_RGBA;
    break;
  case 3:
    format = GL_RGB;
    break;
  }

  ASSERT(format != static_cast<GLenum>(-1));

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

uint32_t module_textures::width(index_type i) const {
  return widths.at(i);
}

uint32_t module_textures::height(index_type i) const {
  return heights.at(i);
}

GLenum module_textures::format(index_type i) const {
  return formats.at(i);
}

GLenum module_textures::type(index_type i) const {
  return types.at(i);
}

GLenum module_textures::texel_type(index_type i) const {
  return texel_types.at(i);
}

uint32_t module_textures::bytes_per_pixel(index_type i) const {

  switch (internal_formats[i]) {
  case GL_RGBA:
  case GL_RGBA8:
  case GL_SRGB8_ALPHA8:
    break;
  case GL_DEPTH_COMPONENT:
    break;
  default:
    __FATAL__("unexpected format found: 0x% " PRIx64,
              internal_formats[i]);
    break;
  }

  return num_channels.at(i);
}


