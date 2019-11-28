#include "textures.hpp"
#include "stb_image.h"
#include "backend/opengl.hpp"

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
  for (size_t i = 0; i < tex_handles.size(); ++i) {
    // unbind to ensure that the resources aren't contested
    // when we free them.
    g_m.gpu->texture_bind(types[i], gapi::k_texture_object_none);
    g_m.gpu->texture_delete(tex_handles[i]);
  }
}

void module_textures::bind(module_textures::index_type id, int slot) const {
  CLOG(logflag_textures_bind, "BINDING texture index %i with API handle %i @ slot %i\n", id, tex_handles[id], slot);
  slots[id] = static_cast<gapi::int_t>(slot);
  g_m.gpu->texture_set_active_unit(slots[id]);
  g_m.gpu->texture_bind(types[id], tex_handles[id]);
}

void module_textures::unbind(module_textures::index_type id) const {
  CLOG(logflag_textures_bind, "UNBINDING texture index %i with API handle %i @ slot %i\n", id, tex_handles[id], slots[id]);
  g_m.gpu->texture_set_active_unit(slots[id]);
  g_m.gpu->texture_bind(types[id], gapi::k_texture_object_none);
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
    case cubemap_preset_test_room_0: {
      fill_texture_data_buffer_rgb4(d.px, width, height, k_room_white);
      fill_texture_data_buffer_rgb4(d.nx, width, height, k_room_white);

      fill_texture_data_buffer_rgb4(d.py, width, height, k_room_blue);
      fill_texture_data_buffer_rgb4(d.ny, width, height, k_room_blue);

      fill_texture_data_buffer_rgb4(d.pz, width, height, k_room_red);
      fill_texture_data_buffer_rgb4(d.nz, width, height, k_room_red);

      // the colors used here are currently specified in linear space,
      // so it's best we _don't_ perform linearization when sampling automatically.
      p.internal_format = gapi::texture_int_fmt::rgba8;
    } break;

    default:
      ASSERT(false);
      break;
  }

  p.width = width;
  p.height = height;
  p.num_channels = channels;
  p.type = gapi::texture_target::texture_cube_map;
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
  p.internal_format = gapi::texture_int_fmt::srgb8_alpha8;
  p.num_channels = num_channels;
  p.type = gapi::texture_target::texture_cube_map;
  p.data.data = data;

  return p;
}

module_textures::params module_textures::texture2d_rgba_params(uint32_t width,
                                                                uint32_t height) {
  params p {};

  p.width = width;
  p.height = height;
  p.num_channels = 4;
  p.type = gapi::texture_target::texture_2d;
  p.min_filter = gapi::texture_min_filter::linear;
  p.mag_filter = gapi::texture_mag_filter::linear;
  p.format = gapi::texture_fmt::rgba;
  p.internal_format = gapi::texture_int_fmt::rgba8;
  p.texel_type = gapi::primitive_type::unsigned_byte;

  texture_data_buffer buffer(p.width * p.height * p.num_channels, 0);
  p.data.data = buffer;

  return p;
}

module_textures::params module_textures::depthtexture_params(uint32_t width, uint32_t height) {
  params p {};

  p.width = width;
  p.height = height;
  p.num_channels = 4;
  p.type = gapi::texture_target::texture_2d;
  p.min_filter = gapi::texture_min_filter::nearest;
  p.mag_filter = gapi::texture_mag_filter::nearest;
  p.format = gapi::texture_fmt::depth_component;
  p.internal_format = gapi::texture_int_fmt::depth_component;
  p.texel_type = gapi::primitive_type::floating_point;

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
  gapi::texture_object_handle handle = g_m.gpu->texture_new();
  g_m.gpu->texture_bind(p.type, handle);

  auto iterable = p.post();

  for (gapi::texture_param_ref param: iterable) {
    g_m.gpu->texture_set_param(p.type, param);
  }

  g_m.gpu->texture_bind(p.type, gapi::k_texture_object_none);

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
  case gapi::texture_target::texture_cube_map: {
    const auto& cubemap_d = std::get<cubemap_data>(p.data.data);

    fill_texture2d(gapi::texture_target::texture_cube_map_px, index, cubemap_d.px.data());
    fill_texture2d(gapi::texture_target::texture_cube_map_nx, index, cubemap_d.nx.data());

    fill_texture2d(gapi::texture_target::texture_cube_map_py, index, cubemap_d.py.data());
    fill_texture2d(gapi::texture_target::texture_cube_map_ny, index, cubemap_d.ny.data());

    fill_texture2d(gapi::texture_target::texture_cube_map_pz, index, cubemap_d.pz.data());
    fill_texture2d(gapi::texture_target::texture_cube_map_nz, index, cubemap_d.nz.data());
  } break;

  case gapi::texture_target::texture_2d: {
    const auto& d = std::get<texture_data_buffer>(p.data.data);

    fill_texture2d(gapi::texture_target::texture_2d, index, d.data());
  } break;

  default:
    __FATAL__("Invalid texture target type.");
    break;
  }
  unbind(index);

  return index;
}

void module_textures::fill_texture2d(gapi::texture_target paramtype, index_type tid, const uint8_t* data) {
  g_m.gpu->texture_image_2d(paramtype,
                            0, // mip level
                            internal_formats[tid],
                            widths[tid],
                            heights[tid],
                            formats[tid],
                            texel_types[tid],
                            reinterpret_cast<const void*>(data));
#if 0
  GL_FN(glTexImage2D(paramtype,
                     0,
                     internal_formats[tid],
                     widths[tid],
                     heights[tid],
                     0,
                     formats[tid],
                     texel_types[tid],
                     data));
#endif                    
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

gapi::texture_object_ref module_textures::handle(module_textures::index_type i) const {
  return tex_handles.at(i);
}

uint32_t module_textures::width(index_type i) const {
  return widths.at(i);
}

uint32_t module_textures::height(index_type i) const {
  return heights.at(i);
}

gapi::texture_fmt module_textures::format(index_type i) const {
  return formats.at(i);
}

gapi::texture_target module_textures::type(index_type i) const {
  return types.at(i);
}

gapi::primitive_type module_textures::texel_type(index_type i) const {
  return texel_types.at(i);
}

uint32_t module_textures::bytes_per_pixel(index_type i) const {

  ASSERT_CODE(
    switch (gl_int_fmt_to_int(internal_formats[i])) {
    case GL_RGBA:
    case GL_RGBA8:
    case GL_SRGB8_ALPHA8:
    case GL_DEPTH_COMPONENT:
      break;
      
    default:
      __FATAL__("unexpected format found: 0x% " PRIx64,
                internal_formats[i]);
      break;
    }
  );

  return num_channels.at(i);
}


