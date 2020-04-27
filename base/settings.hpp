#pragma once

#include "common.hpp"
#include <variant>

class settings {
public:    
  enum class shape_type
    {
     triangle,
     sphere,
     cube
    };
  
  struct shape_info {
    std::vector<vec4_t> colors;
    std::vector<vec3_t> centers;
    std::vector<vec3_t> sizes;
    std::vector<shape_type> types;
  } m_shape_info;

  struct add_shape {
    vec4_t color{R(1.0)};
    vec3_t center{R(0)};
    vec3_t size{R(1.0)};
    shape_type type{shape_type::sphere};

    settings& self;
   
    add_shape(settings& self)
      : self{self} {
    }

    add_shape& with_color(const vec4_t& color) { this->color = color; return *this; }
    add_shape& with_center(const vec3_t& center) { this->center = center; return *this; }
    add_shape& with_size(const vec3_t& size) { this->size = size; return *this; }
    add_shape& as_type(shape_type type) { this->type = type; return *this; }
    void insert() {
      self.m_shape_info.colors.push_back(color);
      self.m_shape_info.centers.push_back(center);
      self.m_shape_info.sizes.push_back(size);
      self.m_shape_info.types.push_back(type);
    }
  }; 
  
  // vulkan API related settings
  struct vk {
      enum class present_mode_select
      {
       fifo,
       fifo_relaxed,
       best_fit     // not implemented yet
      };

    enum class swapchain_option
      {
       max_image_count,
       min_image_count
      };

    using swapchain_image_count_t =
      std::variant<uint8_t, swapchain_option>;
    
    struct class_renderer {
      struct method_render {
	bool use_frustum_culling{false};
	bool allow_more_frames_than_fences{false};
      };
      struct method_setup_vertex_buffer {
	bool use_staging{false};
      };
      struct method_setup {
	bool use_single_pass{false};
      };
      struct method_select_present_mode {
	present_mode_select select_method{present_mode_select::fifo};
      };

      method_render render{};
      method_setup_vertex_buffer setup_vertex_buffer{};
      method_setup setup{};
      method_select_present_mode select_present_mode{};


      // the amount of frames that can simultaneously not be in the ready state
      swapchain_image_count_t max_frames_in_flight{swapchain_option::max_image_count};
      swapchain_image_count_t swapchain_image_count{swapchain_option::max_image_count};

      bool enable_validation_layers{true};
      
    } renderer{};

    struct class_image_pool {
      struct method_make_image {
	// this takes an image_gen_params with settings
	// that are designed to produce an image with preinitialized
	// data and internally, within the pool itself,
	// produces an image with an optimal data layout.
	// The tradeoff is that it will take longer to create the image,
	// but if this is during init then it's really not a problem.
	bool always_produce_optimal_images{true};
      };

      method_make_image make_image{};
    } image_pool{};
    
    bool ok() const;
  };

private:  
  vk m_vk{};
  
public:
  const vk& vk_settings() const { return m_vk; }
  
  bool read();
};
