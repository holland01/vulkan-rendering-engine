#pragma once

#include "common.hpp"
#include "util.hpp"

#include <stdint.h>
#include <vector>
#include <array>
#include <string>
#include <variant>

#define ASSERT_FMT(fmt) ASSERT(fmt == GL_RGBA || fmt == GL_RGB || fmt == GL_DEPTH_COMPONENT)

using texparams_t = darray<GLenum>;

struct module_textures: public type_module {
  darray<GLuint> tex_handles;

  darray<uint32_t> widths;
  darray<uint32_t> heights;
  darray<uint32_t> num_channels;
  darray<GLenum> internal_formats;
  darray<GLenum> formats;
  darray<uint8_t> num_levels; // only zero supported for now
  darray<GLenum> min_filters;
  darray<GLenum> mag_filters;
  darray<GLenum> texel_types;
  mutable darray<GLenum> slots;
  darray<GLenum> types;

  using index_type = int16_t;
  using cubemap_paths_type = std::array<fs::path, 6>;

  static const inline index_type k_uninit = -1;
  static const inline fs::path k_root_path = fs::path("resources") / fs::path("textures");

  ~module_textures();

  void bind(index_type id, int slot = 0) const;

  void unbind(index_type id) const;

  using texture_data_buffer = darray<uint8_t>;

  struct cubemap_data {
    texture_data_buffer px, nx;
    texture_data_buffer py, ny;
    texture_data_buffer pz, nz;

    static cubemap_data all(size_t size,
                            uint8_t init_pixel) {

      texture_data_buffer set(size, init_pixel);

      cubemap_data R;
      R.px = darray_clone(set);
      R.nx = darray_clone(set);

      R.py = darray_clone(set);
      R.ny = darray_clone(set);

      R.pz = darray_clone(set);
      R.nz = darray_clone(set);

      return R;
    }
  };

  struct texture_data {
    using data_type = std::variant<cubemap_data, texture_data_buffer>;

    data_type data;

    texture_data(const cubemap_data& cd): data(cd) {}
    texture_data(const texture_data_buffer& db): data(db) {}
    texture_data(): data(texture_data_buffer()) {}
  };

  struct params {
    texture_data data {};

    GLenum type {GL_TEXTURE_2D};

    GLenum format {GL_RGBA};
    GLenum internal_format {GL_RGBA};

    GLenum min_filter {GL_LINEAR};
    GLenum mag_filter {GL_LINEAR};

    GLenum wrap_mode_s {GL_CLAMP_TO_EDGE};
    GLenum wrap_mode_t {GL_CLAMP_TO_EDGE};
    GLenum wrap_mode_r {GL_CLAMP_TO_EDGE};

    GLenum mip_base_level {0};
    GLenum mip_max_level {0};

    GLenum texel_type {GL_UNSIGNED_BYTE};

    uint32_t width {256};
    uint32_t height {256};
    uint32_t num_channels {4};

    uint8_t num_levels {1};

    auto post() const {
      ASSERT(num_levels == 1);
      ASSERT(mip_base_level == 0);
      ASSERT(mip_max_level == 0);

      darray<GLenum> v;

      v.insert(v.end(), {
        GL_TEXTURE_MIN_FILTER, min_filter,
        GL_TEXTURE_MAG_FILTER, mag_filter,
        GL_TEXTURE_WRAP_T, wrap_mode_s,
        GL_TEXTURE_WRAP_S, wrap_mode_t,
        GL_TEXTURE_BASE_LEVEL, mip_base_level,
        GL_TEXTURE_MAX_LEVEL, mip_max_level
      });

      if (type == GL_TEXTURE_CUBE_MAP) {
        v.insert(v.end(), {
          GL_TEXTURE_WRAP_R, wrap_mode_r
        });
      }

      ASSERT((v.size() & 1) == 0);

      return v;
    }
  };

  enum cubemap_preset {
    cubemap_preset_test_room_0
  };

  module_textures::params cubemap_params(uint32_t width, uint32_t height);

  module_textures::params cubemap_params(uint32_t width, uint32_t height, cubemap_preset preset);

  module_textures::params cubemap_params(uint32_t width,
                                         uint32_t height,
                                         uint32_t num_channels,
                                         cubemap_data data);

  // designed for operations that don't involve gamma correct colors
  module_textures::params texture2d_rgba_params(uint32_t width,
                                                 uint32_t height);

  module_textures::params depthtexture_params(uint32_t width, uint32_t height);

  index_type new_texture(const module_textures::params& p);

  void fill_texture2d(GLenum paramtype, index_type tid, const uint8_t* data);

  GLenum format_from_channels(int channels) const;

  index_type handle(index_type i) const;

  uint32_t width(index_type i) const;
  uint32_t height(index_type i) const;
  GLenum format(index_type i) const;
  GLenum type(index_type i) const;
  GLenum texel_type(index_type i) const;

  uint32_t bytes_per_pixel(index_type i) const;


};

