#pragma once

#include "common.hpp"
#include "util.hpp"
#include "gapi.hpp"

#include <stdint.h>
#include <vector>
#include <array>
#include <string>
#include <variant>

#define ASSERT_FMT(fmt) ASSERT(fmt == GL_RGBA || fmt == GL_RGB || fmt == GL_DEPTH_COMPONENT)

using texparams_t = darray<GLenum>;

struct module_textures: public type_module {
  darray<gapi::texture_object_handle> tex_handles;

  darray<uint32_t> widths;
  darray<uint32_t> heights;
  darray<uint32_t> num_channels;
  
  darray<gapi::texture_int_fmt> internal_formats;
  darray<gapi::texture_fmt> formats;

  darray<uint8_t> num_levels; // only zero supported for now
  
  darray<gapi::texture_min_filter> min_filters;
  darray<gapi::texture_mag_filter> mag_filters;
  darray<gapi::primitive_type> texel_types;

  mutable darray<gapi::int_t> slots;
  darray<gapi::texture_object_target> types;

  using index_type = int16_t;
  using cubemap_paths_type = std::array<fs::path, 6>; // UNUSED

  static const inline index_type k_uninit = -1;
  static const inline fs::path k_root_path = fs::path("resources") / fs::path("textures"); // UNUSED

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

    gapi::texture_object_target type {gapi::texture_object_target::texture_2d};

    gapi::texture_fmt format {gapi::texture_fmt::rgba};
    gapi::texture_int_fmt internal_format {gapi::texture_int_fmt::rgba8};

    gapi::texture_min_filter min_filter {gapi::texture_min_filter::linear};
    gapi::texture_mag_filter mag_filter {gapi::texture_mag_filter::linear};

    gapi::texture_wrap_mode wrap_mode_s {gapi::texture_wrap_mode::clamp_to_edge};
    gapi::texture_wrap_mode wrap_mode_t {gapi::texture_wrap_mode::clamp_to_edge};
    gapi::texture_wrap_mode wrap_mode_r {gapi::texture_wrap_mode::clamp_to_edge};

    uint8_t mip_base_level {0};
    uint8_t mip_max_level {0};

    gapi::primitive_type texel_type {gapi::primitive_type::unsigned_byte};

    uint32_t width {256};
    uint32_t height {256};
    uint32_t num_channels {4};

    uint8_t num_levels {1};

    auto post() const {
      ASSERT(num_levels == 1);
      ASSERT(mip_base_level == 0);
      ASSERT(mip_max_level == 0);

      darray<gapi::texture_param> v;

      v.insert(v.end(), {
        gapi::texture_param{}.min_filter(min_filter),
        gapi::texture_param{}.mag_filter(mag_filter),
        gapi::texture_param{}.wrap_mode_s(wrap_mode_s),
        gapi::texture_param{}.wrap_mode_t(wrap_mode_t),
        gapi::texture_param{}.mip_base_level(mip_base_level),
        gapi::texture_param{}.mip_max_level(mip_max_level)
      });

      if (type == gapi::texture_object_target::texture_cube_map) {
        v.insert(v.end(), {
          gapi::texture_param{}.wrap_mode_r(wrap_mode_r)
        });
      }

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

  void fill_texture2d(gapi::texture_object_target paramtype, index_type tid, const uint8_t* data);

  gapi::texture_object_ref handle(index_type i) const;

  uint32_t width(index_type i) const;
  uint32_t height(index_type i) const;

  gapi::texture_fmt format(index_type i) const;
  gapi::texture_object_target type(index_type i) const;
  gapi::primitive_type texel_type(index_type i) const;

  uint32_t bytes_per_pixel(index_type i) const;
};

