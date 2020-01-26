// This is a one-stop shop Vulkan implementation. All Vulkan code should reside in the base/backend folder.
// All information on the API that was used to produce this implementation has been garnered from the following 
// primary resources:
// 
// - http://vulkan-tutorial.com
// - https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html

#pragma once

#include "common.hpp"
#include "device_context.hpp"

#include "vk_common.hpp"
#include "vk_image.hpp"
#include "vk_uniform_buffer.hpp"
#include "vk_pipeline.hpp"

#include <optional>
#include <set>
#include <initializer_list>
#include <unordered_map>
#include <iomanip>
#include <iostream>
#include <functional>

#include <glm/gtc/matrix_transform.hpp>

namespace vulkan {
  struct image_requirements;
  
  VkPipelineLayoutCreateInfo default_pipeline_layout_settings();

  VkAttachmentDescription default_colorbuffer_settings(VkFormat swapchain_format);
  VkAttachmentReference default_colorbuffer_ref_settings();

  VkAttachmentDescription default_depthbuffer_settings();
  VkAttachmentReference default_depthbuffer_ref_settings();
  
  VkShaderModuleCreateInfo make_shader_module_settings(darray<uint8_t>& spv_code);

  VkPipelineShaderStageCreateInfo make_shader_stage_settings(VkShaderModule module, VkShaderStageFlagBits type);  
    
  depthbuffer_data make_depthbuffer(const device_resource_properties& properties,
				    uint32_t width,
				    uint32_t height);

  static inline constexpr bool k_test_multipass = false;
  
  struct queue_family_indices {
    std::optional<uint32_t> graphics_family{};
    std::optional<uint32_t> present_family{};

    bool ok() const {
      bool r =
	graphics_family.has_value() &&
	present_family.has_value();
      ASSERT(r);
      return r;
    }
  };

  struct swapchain_support_details {
    VkSurfaceCapabilitiesKHR capabilities;
    darray<VkSurfaceFormatKHR> formats;
    darray<VkPresentModeKHR> present_modes;
  };

  struct transform_data {
    mat4_t view_to_clip{R(1.0)};
    mat4_t world_to_view{R(1.0)};    
  };

  struct sampler_data {
    int sampler_index;
  };

  struct rot_cmd {
    vec3_t axes;
    real_t rad;
  };
  
  using vertex_list_t = darray<vertex_data>;

  
  
  struct pass_create_params {
    image_pool* pool{nullptr};
    render_pass_pool* rpass_pool{nullptr};

    render_pass_pool::index_type render_pass{render_pass_pool::k_unset};
    
    uint32_t width{UINT32_MAX};
    uint32_t height{UINT32_MAX};
    
    const darray<VkImageView>& next_pass_image_views;
    
    VkImageView depth_image_view{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};

    bool ok() const {
      bool r =
	(pool != nullptr) &&
	(rpass_pool != nullptr) &&
	(width != UINT32_MAX) &&
	(height != UINT32_MAX) &&
	(!next_pass_image_views.empty()) &&
	(depth_image_view != VK_NULL_HANDLE) &&
	(rpass_pool->ok_render_pass(render_pass)) &&
	(format != VK_FORMAT_UNDEFINED);
      
      ASSERT(r);
      return r;
    }
  };

  struct render_pass_image_create_params {
    image_pool* p_image_pool{nullptr};
    size_t num_images{num_max<size_t>()};
    uint32_t width{UINT32_MAX};
    uint32_t height{UINT32_MAX};
    VkFormat format{VK_FORMAT_UNDEFINED};
    
    bool ok() const {
      bool r =
	(p_image_pool != nullptr) &&
	(num_images != num_max<size_t>()) &&
	(width != UINT32_MAX) &&
	(height != UINT32_MAX) &&
	(format != VK_FORMAT_UNDEFINED);      
      ASSERT(r);      
      return r;
    }
  };

  using attachment_list_t =
    std::array<VkImageView, 3>;

  struct render_pass_framebuffer_create_params;
  
  using attachment_setup_fn_type =
    std::function<void(size_t framebuffer_index,
		       attachment_list_t& attachments,
		       const darray<image_pool::index_type>& images,
		       const render_pass_framebuffer_create_params& self
		       )>;
  
  struct render_pass_framebuffer_create_params {
    const render_pass_image_create_params& image_params;
    const darray<VkImageView>& next_pass_image_views;
    
    VkRenderPass render_pass{VK_NULL_HANDLE};
    VkImageView  depth_image_view{VK_NULL_HANDLE};

    attachment_setup_fn_type fn_attachment_setup;    

    render_pass_framebuffer_create_params(const render_pass_image_create_params& image_params,
					  const darray<VkImageView>& next_pass_image_views,    
					  VkRenderPass render_pass,
					  VkImageView  depth_image_view,
					  attachment_setup_fn_type fn_attachment_setup)
      
      : image_params{image_params},
	next_pass_image_views{next_pass_image_views},
	render_pass{render_pass},
	depth_image_view{depth_image_view},
	fn_attachment_setup{fn_attachment_setup}
    {}

    bool ok() const {
      bool r =
        image_params.ok() &&
	(render_pass != VK_NULL_HANDLE) &&
	(depth_image_view != VK_NULL_HANDLE) &&
	(fn_attachment_setup != nullptr) &&
	(!next_pass_image_views.empty());
      ASSERT(r);
      return r;
    }
  };
  
  class render_pass_external_data {
  private:  
    darray<image_pool::index_type> m_images;
    darray<VkFramebuffer> m_framebuffers;
    
    size_t m_expected_count{num_max<size_t>()};
    
  public:
    bool make_images(const device_resource_properties& properties,
		     const render_pass_image_create_params& params) {
      bool good =
	properties.ok() &&
	params.ok();
      
      if (good) {
	m_expected_count = params.num_images;
	
	image_gen_params i_params{};

	i_params.memory_property_flags =
	  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	i_params.format = params.format;
      
	i_params.attachment_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	i_params.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	i_params.final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      
	i_params.tiling = VK_IMAGE_TILING_OPTIMAL;
	i_params.usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      
	i_params.type = VK_IMAGE_TYPE_2D;
	i_params.view_type = VK_IMAGE_VIEW_TYPE_2D;
	i_params.aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
      
	i_params.source_pipeline_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	i_params.dest_pipeline_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      
	i_params.source_access_flags = 0;

	i_params.dest_access_flags =
	  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	  VK_ACCESS_TRANSFER_READ_BIT;

	i_params.width = params.width;
	i_params.height = params.height;
	i_params.depth = 1;
            
	m_images.resize(m_expected_count);
      
	for (size_t i = 0; i < m_images.size() && good; ++i) {
	  m_images[i] = params.p_image_pool->make_image(properties,
							i_params);

	  good = params.p_image_pool->ok_image(m_images[i]);
	}
      }

      return good;
    }

    bool make_framebuffers(const device_resource_properties& properties,
			   const render_pass_framebuffer_create_params& params) {

      bool good =
	properties.ok() &&
	params.ok() &&
	(m_expected_count != num_max<size_t>() && m_expected_count != 0) &&
	(!m_images.empty()) &&
	(m_images.size() == m_expected_count);

      if (good) {      
	std::array<VkImageView, 3> attachments{};
	attachments.fill(VK_NULL_HANDLE);
      
	VkFramebufferCreateInfo framebuffer_info = {};
      
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.pNext = nullptr;
	
	framebuffer_info.flags = 0;

	framebuffer_info.renderPass = params.render_pass;

	framebuffer_info.attachmentCount = attachments.size();

	framebuffer_info.width = params.image_params.width;
	framebuffer_info.height = params.image_params.height;
	framebuffer_info.layers = 1;

	m_framebuffers.resize(m_expected_count, VK_NULL_HANDLE);
       
	
	for (size_t i = 0; i < m_framebuffers.size() && good; ++i) {
	  params.fn_attachment_setup(i,
				     attachments,
				     m_images,
				     params);
	
	  framebuffer_info.pAttachments = attachments.data();
	  
	  VK_FN(vkCreateFramebuffer(properties.device,
				    &framebuffer_info,
				    nullptr,
				    &m_framebuffers[i]));

	  good = H_OK(m_framebuffers[i]);
	}

	if (!good) {
	  free_mem(properties.device);	
	}
      }

      return good;
    }    

    void free_mem(VkDevice device) {
      for (VkFramebuffer& fb: m_framebuffers) {
	free_device_handle<VkFramebuffer, &vkDestroyFramebuffer>(device,
								 fb);
      }

      m_framebuffers.clear();
      m_images.clear();
    }
    
    bool ok() const {
      bool r =
	(m_expected_count != num_max<size_t>()) &&
	(m_images.size() == m_expected_count) &&
	(m_images.size() == m_framebuffers.size());
      
      ASSERT(r);
      return r;
    }
  };
  
  class renderer {    
    uint32_t m_instance_count{0};   

    render_pass_external_data m_first_pass{};

    render_pass_external_data m_second_pass{};
    
    darray<VkPhysicalDevice> m_vk_physical_devs;

    darray<VkImage> m_vk_swapchain_images;
    
    darray<VkImageView> m_vk_swapchain_image_views;

    darray<VkFramebuffer> m_vk_swapchain_framebuffers;

    darray<VkImage> m_vk_firstpass_images;

    darray<VkCommandBuffer> m_vk_command_buffers;   

    descriptor_set_pool m_descriptor_set_pool{};
    
    image_pool m_image_pool{};

    texture_pool m_texture_pool{};

    uniform_block_pool m_uniform_block_pool{};

    render_pass_pool m_render_pass_pool{};

    pipeline_layout_pool m_pipeline_layout_pool{};

    pipeline_pool m_pipeline_pool{};
    
    depthbuffer_data m_depthbuffer{};
    
    uniform_block_data<transform_data> m_transform_uniform_block{};

    static inline constexpr vec3_t k_room_cube_center = R3v(0, 0, 0);
    static inline constexpr vec3_t k_room_cube_size = R3(20);
    static inline constexpr vec3_t k_mirror_cube_center = R3v(0, 0, 0);
    static inline constexpr vec3_t k_mirror_cube_size = R3(1);
    
    static inline constexpr vec3_t k_color_green = R3v(0, 1, 0);
    static inline constexpr vec3_t k_color_red = R3v(1, 0, 0);
    static inline constexpr vec3_t k_color_blue = R3v(0, 0, 1);

    static inline constexpr int32_t k_sampler_checkerboard = 0;
    static inline constexpr int32_t k_sampler_aqua = 1;
    
    // NOTE:
    // right handed system,
    // so positive rotation about a given axis
    // is counter-clockwise
    vertex_list_t m_vertex_buffer_vertices =
      model_triangle(R3v(-2.25, 0, 0)) + // offset: 0, length: 3
      model_triangle(R3v(2.25, 0, 1), R3v(0, 0.5, 0.8)) + // offset: 3, length: 3
      model_cube(k_mirror_cube_center, R3(1), k_mirror_cube_size) + // offset: 6, length: 36
      model_cube(k_room_cube_center, R3(1), k_room_cube_size); // offset: 42, length: 36
      
    VkCommandPool m_vk_command_pool{VK_NULL_HANDLE};

    VkDescriptorPool m_vk_descriptor_pool{VK_NULL_HANDLE};   

    VkSurfaceFormatKHR m_vk_khr_swapchain_format;

    VkExtent2D m_vk_swapchain_extent;

    VkInstance m_vk_instance {VK_NULL_HANDLE};

    VkPhysicalDevice m_vk_curr_pdevice{VK_NULL_HANDLE};

    VkDevice m_vk_curr_ldevice{VK_NULL_HANDLE};

    VkQueue m_vk_graphics_queue{VK_NULL_HANDLE};
    VkQueue m_vk_present_queue{VK_NULL_HANDLE};

    VkSurfaceKHR m_vk_khr_surface{VK_NULL_HANDLE};
    VkSwapchainKHR m_vk_khr_swapchain{VK_NULL_HANDLE};

    VkSemaphore m_vk_sem_image_available;
    VkSemaphore m_vk_sem_render_finished;

    VkBuffer m_vk_vertex_buffer{VK_NULL_HANDLE};
    VkDeviceMemory m_vk_vertex_buffer_mem{VK_NULL_HANDLE};

    std::array<image_pool::index_type, 2> m_test_image_indices =
      {
       image_pool::k_unset,
       image_pool::k_unset
      };
    
    std::array<texture_pool::index_type, 2> m_test_texture_indices =
      {
       texture_pool::k_unset,
       texture_pool::k_unset
      };

    //
    // Descriptor set indices
    //
    // NOTE:
    //    m_vk_descriptor_pool is used to allocate memory
    //    needed for these descriptor sets. When the pool is allocated,
    //    the amount of descriptor sets that are to be created is specified
    //    _upfront_. So, it's important that the number of indices here
    //    match the total count that's passed to vkCreateDescriptorPool's
    //    create_info (see setup_descriptor_pool())
    //
    static constexpr inline int k_descriptor_set_samplers = 0;
    static constexpr inline int k_descriptor_set_uniform_blocks = 1; 
    static constexpr inline int k_descriptor_set_sampler_cubemap = 2;
    
    std::array<descriptor_set_pool::index_type, 3> m_test_descriptor_set_indices =
      {
       descriptor_set_pool::k_unset,  // pipeline 0
       descriptor_set_pool::k_unset,  // pipeline 0, pipeline 1
       descriptor_set_pool::k_unset   // pipeline 1
      };   

    static constexpr inline int k_render_phase_texture2d = 0;
    static constexpr inline int k_render_phase_cubemap = 1;
    
    darray<render_pass_pool::index_type> m_render_pass_indices =
      {
       render_pass_pool::k_unset,
       render_pass_pool::k_unset
      };

    darray<pipeline_layout_pool::index_type> m_pipeline_layout_indices =
      {
       pipeline_layout_pool::k_unset,
       pipeline_layout_pool::k_unset,
      };

    darray<pipeline_pool::index_type> m_pipeline_indices =
      {
       pipeline_pool::k_unset,
       pipeline_pool::k_unset
      };
    
    bool m_ok_present{false};
    bool m_ok_descriptor_pool{false};
    bool m_ok_render_pass{false};
    bool m_ok_uniform_block_data{false};
    bool m_ok_texture_data{false};
    bool m_ok_depthbuffer_data{false};
    bool m_ok_graphics_pipeline{false};
    bool m_ok_vertex_buffer{false};
    bool m_ok_framebuffers{false};
    bool m_ok_command_pool{false};
    bool m_ok_command_buffers{false};
    bool m_ok_semaphores{false};
    bool m_ok_scene{false};
    
    struct vk_layer_info {
      const char* name{nullptr};
      bool enable{false};
    };

    static inline darray<vk_layer_info> s_layers = {
      { "VK_LAYER_LUNARG_standard_validation", true },
      { "VK_LAYER_LUNARG_core_validation", true },
      { "VK_LAYER_LUNARG_parameter_validation", false }
    };

    static inline darray<const char*> s_device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    static constexpr inline real_t k_tri_ps = R(1);
    
    vertex_list_t model_triangle(vec3_t offset = R3(0), vec3_t color = R3(1)) {
      m_instance_count++;
      return
	{
	 { R3v(-k_tri_ps, k_tri_ps, 0.0) + offset, R2v(0.0, 0.0), color }, // top left position, top left texture
	 { R3v(k_tri_ps, -k_tri_ps, 0.0) + offset, R2v(1.0, 1.0), color }, // bottom right position, bottom right texture
	 { R3v(-k_tri_ps, -k_tri_ps, 0.0) + offset, R2v(0.0, 1.0), color }
	};
    }
    
    vertex_list_t model_quad(vec3_t translate = R3(0),
			     vec3_t color = R3(1),
			     vec3_t scale = R3(1),
			     darray<rot_cmd> rot = darray<rot_cmd>()) {
      
      auto a = model_triangle(R3(0), color);
      auto b = model_triangle(R3(0), color);

      a[1].position.y = R(k_tri_ps); // flip to top
      a[1].st.y = R(0);
      
      a[2].position.x = R(k_tri_ps); 
      a[2].st.x = R(1);

      auto combined = a + b;
      
      for (vertex_data& vertex: combined) {
	vertex.position *= scale;
	for (const auto& cmd: rot) {
	  mat4_t R = glm::rotate(mat4_t(R(1)), cmd.rad, cmd.axes);
	  vertex.position = MAT4V3(R, vertex.position);
	}
	vertex.position += translate * scale;
      }

      return combined;
    }

    vertex_list_t model_cube(vec3_t translate = R3(0),
			     vec3_t color = R3(1),
			     vec3_t scale = R3(1)) {
      return
	// left face
	model_quad(translate + R3v(-1, 0, 0),
		   color,
		   scale,
		   {
		    {
		     R3v(0, 1, 0),
		     glm::half_pi<real_t>()
		    }
		   }) +

	// right face
	model_quad(translate + R3v(1, 0, 0),
		   color,
		   scale,
		   {
		    {
		     R3v(0, 1, 0),
		     glm::half_pi<real_t>()
		    }
		   }) +

	// up face
	model_quad(translate + R3v(0, 1, 0),
		   color,
		   scale,
		   {
		    {
		     R3v(1, 0, 0),
		     -glm::half_pi<real_t>()
		    }
		   }) +

	// down face
	model_quad(translate + R3v(0, -1, 0),
		   color,
		   scale,
		   {
		    {
		     R3v(1, 0, 0),
		     glm::half_pi<real_t>()
		    }
		   }) +
      
	// front face
	model_quad(translate + R3v(0, 0, 1),
		   color,
		   scale,
		   {}) +

	// back face
	model_quad(translate + R3v(0, 0, -1),
		   color,
		   scale,
		   {});
    }

    VkRenderPass render_pass(int index) const {
      return
	m_render_pass_pool.render_pass(m_render_pass_indices.at(index));
    }
    
    VkPipelineLayout pipeline_layout(int index) const {
      return
	m_pipeline_layout_pool.pipeline_layout(m_pipeline_layout_indices.at(index));
    }

    VkPipeline pipeline(int index) const {
      return
	m_pipeline_pool.pipeline(m_pipeline_indices.at(index));
    }

    VkDescriptorSetLayout descriptor_set_layout(int index) const {
      return
	m_descriptor_set_pool.
	descriptor_set_layout(m_test_descriptor_set_indices.at(index));
    }

    void print_physical_device_memory_types() {
      darray<VkMemoryType> mem_types = get_physical_device_memory_types(m_vk_curr_pdevice);

      ASSERT(!mem_types.empty());
	
      std::stringstream ss;
      ss << "Memory types (" << mem_types.size() << ")\n";
      for (VkMemoryType mt: mem_types) {
	ss << "..\n"
	   << "....propertyFlags = " << SS_HEX(mt.propertyFlags) << "\n"
	   << "....heapIndex = " << SS_HEX(mt.heapIndex) << "\n"; 
      }
      std::cout << ss.str() << std::endl;
    }
    
    darray<VkMemoryType> get_physical_device_memory_types(VkPhysicalDevice device) {
      darray<VkMemoryType> ret{};
      if (ok()) {
        VkPhysicalDeviceMemoryProperties out_properties = {};
	vkGetPhysicalDeviceMemoryProperties(device, &out_properties);
	ret.resize(out_properties.memoryTypeCount);

	for (size_t i = 0; i < ret.size(); ++i) {
	  ret[i] = out_properties.memoryTypes[i];
	}
      }
      return ret;
    }

    device_resource_properties make_device_resource_properties() const {
      uint32_t num_indices = 0;
      uint32_t* indices = query_graphics_buffer_indices(&num_indices);

      darray<uint32_t> indices_copy(num_indices, 0);

      for (uint32_t i = 0; i < num_indices; ++i) {
	indices_copy[i] = indices[i];
      }
      
      return {
        std::move(indices_copy),
        m_vk_curr_pdevice,
        m_vk_curr_ldevice,
        query_queue_sharing_mode(),
	m_vk_descriptor_pool
      };
    }
    
    queue_family_indices query_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) const {
      queue_family_indices indices {};

      if (ok()) {
        uint32_t queue_fam_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device,
						 &queue_fam_count, 
						 nullptr);
        ASSERT(queue_fam_count > 0);
        darray<VkQueueFamilyProperties> queue_props(queue_fam_count);

        vkGetPhysicalDeviceQueueFamilyProperties(device,
						 &queue_fam_count,
						 queue_props.data());

        uint32_t i = 0;
        while (i < queue_fam_count) {
          if ((queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            indices.graphics_family = i;
          }

          if (surface != VK_NULL_HANDLE) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (present_support) {
              indices.present_family = i;
            }
          }
          i++;
        }
      }
      
      return indices;
    }

    VkSharingMode query_queue_sharing_mode() const {
      queue_family_indices details =
	query_queue_families(m_vk_curr_pdevice, m_vk_khr_surface);

      return details.graphics_family.value() == details.present_family.value()
	? VK_SHARING_MODE_EXCLUSIVE
	: VK_SHARING_MODE_CONCURRENT;
    }

    uint32_t* query_graphics_buffer_indices(uint32_t* out_num_indices) const {
      static uint32_t queue_families[1] = {0};

      queue_family_indices details =
	query_queue_families(m_vk_curr_pdevice, m_vk_khr_surface);

      queue_families[0] = details.graphics_family.value();
      *out_num_indices = 1;
      
      return &queue_families[0];
    }

    VkBufferCreateInfo make_vertex_buffer_create_info(VkDeviceSize size) const {
      VkBufferCreateInfo buffer_info = {};
      
      buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      buffer_info.pNext = nullptr;
      buffer_info.flags = 0;

      buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
	  VK_BUFFER_USAGE_TRANSFER_DST_BIT;

      buffer_info.size = size;
      
      buffer_info.sharingMode = query_queue_sharing_mode();

      uint32_t num_indices = 0;
      
      buffer_info.pQueueFamilyIndices = query_graphics_buffer_indices(&num_indices);
      buffer_info.queueFamilyIndexCount = num_indices;

      return buffer_info;
    }

    std::optional<VkMemoryRequirements> query_vertex_buffer_memory_requirements(VkDeviceSize size) const {
      std::optional<VkMemoryRequirements> ret{};
      if (ok_graphics_pipeline()) {
	VkBufferCreateInfo buffer_info = make_vertex_buffer_create_info(size);
	
	VkBuffer dummy_buffer = 0;
	VK_FN(vkCreateBuffer(m_vk_curr_ldevice, &buffer_info, nullptr, &dummy_buffer));

	if (ok() && dummy_buffer != VK_NULL_HANDLE) {
	  VkMemoryRequirements r = {};
	  vkGetBufferMemoryRequirements(m_vk_curr_ldevice,
					dummy_buffer,
					&r);

	  ret = r;
	}
	
	free_vk_ldevice_handle<VkBuffer, &vkDestroyBuffer>(dummy_buffer);
      }
      return ret;
    }

    swapchain_support_details query_swapchain_support(VkPhysicalDevice device) {
      swapchain_support_details details;
      ASSERT(m_vk_khr_surface != VK_NULL_HANDLE);
      if (m_vk_khr_surface != VK_NULL_HANDLE) {
        if (ok()) {
          VK_FN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_vk_khr_surface, &details.capabilities));
        }
        if (ok()) {
          uint32_t format_count = 0;
          VK_FN(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_vk_khr_surface, &format_count, nullptr));
          if (ok()) {
            ASSERT(format_count != 0);
            details.formats.resize(format_count);
            VK_FN(vkGetPhysicalDeviceSurfaceFormatsKHR(device, 
                                                        m_vk_khr_surface, 
                                                        &format_count, 
                                                        details.formats.data()));
          }
        }
        if (ok()) {
          uint32_t present_mode_count = 0;
          VK_FN(vkGetPhysicalDeviceSurfacePresentModesKHR(device, 
                                                            m_vk_khr_surface, 
                                                            &present_mode_count,
                                                            nullptr));
          if (ok()) {
            ASSERT(present_mode_count != 0);
            details.present_modes.resize(present_mode_count);
            VK_FN(vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                                              m_vk_khr_surface,
                                                              &present_mode_count,
                                                              details.present_modes.data()));
          }
        }
      }
      return details;
    }

    bool swapchain_ok(VkPhysicalDevice device) {
      swapchain_support_details details = 
        query_swapchain_support(device);
      
      return !details.formats.empty() && 
             !details.present_modes.empty();
    }

    bool check_device_extensions(VkPhysicalDevice device) {
      bool r = false;
      if (ok()) {
        uint32_t extension_count;
        VK_FN(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr));

        if (ok()) {
          std::vector<VkExtensionProperties> avail_ext(extension_count);

          VK_FN(vkEnumerateDeviceExtensionProperties(device, 
                                                      nullptr, 
                                                      &extension_count, 
                                                      avail_ext.data()));
          
          if (ok()) {
            std::set<std::string> required_ext(s_device_extensions.begin(), 
                                               s_device_extensions.end());

            for (uint32_t i = 0; i < extension_count; ++i) {
              required_ext.erase(avail_ext[i].extensionName);
            }

            r = required_ext.empty();
          }
        }
      }
      return r;
    }

    void setup_device_and_queues() {
      if (ok_pdev()) {
        queue_family_indices indices = 
          query_queue_families(m_vk_curr_pdevice, m_vk_khr_surface);
        
        ASSERT(indices.ok());

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

        std::set<uint32_t> unique_queue_indices = {
          indices.present_family.value(),
          indices.graphics_family.value()
        };

        write_logf("present family queue: %" PRIu32 "; graphics family queue: %" PRIu32 "\n",
                    indices.present_family.value(),
                    indices.graphics_family.value());

        float priority = 1.0f;
	
        for (uint32_t queue: unique_queue_indices) {
          VkDeviceQueueCreateInfo queue_create_info = {};
          queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
          queue_create_info.queueFamilyIndex = queue;
          queue_create_info.queueCount = 1;
          queue_create_info.pQueuePriorities = &priority;
          queue_create_infos.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures dev_features = {};
        
        VkDeviceCreateInfo dev_create_info = {};
        
        dev_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        dev_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        dev_create_info.pQueueCreateInfos = queue_create_infos.data();
        
        dev_create_info.pEnabledFeatures = &dev_features;
        
        auto avail_layers = query_layers();

        dev_create_info.enabledLayerCount = static_cast<uint32_t>(avail_layers.size());
        dev_create_info.ppEnabledLayerNames = ( !avail_layers.empty() )
                                        ? avail_layers.data()
                                        : nullptr;

        dev_create_info.enabledExtensionCount = static_cast<uint32_t>(s_device_extensions.size());
        dev_create_info.ppEnabledExtensionNames = s_device_extensions.data();

        VK_FN(vkCreateDevice(m_vk_curr_pdevice, 
                              &dev_create_info, 
                              nullptr, 
                              &m_vk_curr_ldevice));

        if (ok_ldev()) {
          vkGetDeviceQueue(m_vk_curr_ldevice, 
                           indices.graphics_family.value(), 
                           0, 
                           &m_vk_graphics_queue);

          vkGetDeviceQueue(m_vk_curr_ldevice,
                          indices.present_family.value(),
                          0,
                          &m_vk_present_queue);
        }

        ASSERT(m_vk_graphics_queue != VK_NULL_HANDLE);
      }
    }

    bool setup_surface() {
      if (ok()) {
        VK_FN(glfwCreateWindowSurface(m_vk_instance, 
                                       g_m.device_ctx->window(),
                                       nullptr,
                                       &m_vk_khr_surface));
      }
      return m_vk_khr_surface != VK_NULL_HANDLE;
    }

    void setup_swapchain() {
      if (ok_ldev()) {
        if (swapchain_ok(m_vk_curr_pdevice)) {
          swapchain_support_details details = query_swapchain_support(m_vk_curr_pdevice);
          // Make sure that we have the standard SRGB colorspace available
          VkSurfaceFormatKHR surface_format;
          {
            bool chosen = false;
            for (const auto& format: details.formats) {
              if (format.format == VK_FORMAT_B8G8R8A8_UNORM
                    && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surface_format = format;
                chosen = true;
                break;
              }
            }
            ASSERT(chosen);
          }

          // FIFO_KHR implies vertical sync. There may be a slight latency,
          // but for our purposes this shouldn't a problem.
          // We can deal with potential issues that can occur
          // with MAILBOX_KHR (provides triple buffering/lower latency)
          // if necessary.
          VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
          
          // Here we select the swapchain dimensions (the dimensions of the images that have been
          // rendered into). We want to make them as close as possible to the actual
          // window dimensions; sometimes this will be exact, other times it won't, depending
          // on the system.
          //
          // When the swapchain capabilities are queried, the driver will set the currentExtent
          // width and height to the window dimensions if it asks us to match the dimensions with the window.
          // Otherwise, we have a bit more wiggle room. The driver tells us this by setting
          // currentExtent.width to UINT32_MAX. 
          //
          // For now, it's best to keep things simple and be conservative, 
          // so we'll operate under the (likely) assumption that UINT32_MAX _is not_ set, 
          // and add support for more varied dimension requirements
          // if they pop up in the future.
          VkExtent2D swap_extent;
          {
            ASSERT(details.capabilities.currentExtent.width != UINT32_MAX);
            swap_extent = details.capabilities.currentExtent;
          }

          // Image count represents the amount of images on the swapchain.
          // We'll use a small image count for now. Simple semantics first;
          // we can optimize later as necessary.
          uint32_t image_count = details.capabilities.minImageCount;
          ASSERT(image_count != 0);
          if (image_count == 1) {
            image_count++;
          }
          ASSERT(image_count <= details.capabilities.maxImageCount);

          //
          // Here we actually create the swapchain.
          //
          // --
          // On imageArrayLayers: always set to 1 unless we want to use 
          // stereoscopic 3D rendering.
          //
          // --
          // On imageUsage: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT implies that render commands
          // will write directly to the images on the swapchain. If we want to perform
          // offscreen rendering, something like VK_IMAGE_USAGE_TRANSFER_DST_BIT
          // is considered an option.
          //
          // --
          // On imageSharingMode: something to keep in mind for queue family indices is that
          // both the graphics family queue and present family queue
          // can be shared (use the same family index). 
          //
          // Assuming that they are _not_ shared, then images will be drawn _on_
          // the swapchain using the graphics queue. They will then be submitted 
          // through the presentation queue.
          //
          // VK_SHARING_MODE_EXCLUSIVE implies that an image is owned by one
          // queue family at a time and ownership must be explicitly transferred before
          // using it in another queue family; this is what we use if the indices
          // are the same.
          //
          // VK_SHARING_MODE_CONCURRENT implies that images can be used across
          // multiple queue families without explicit ownership transfers. In advance,
          // we specify which queue family indices the transfers will occur between.
          //
          // --
          // On preTransform: represents a transform that is applied to images on the swapchain.
          // Can range from anything to a 90 degreee clockwise rotation or a flip. For now,
          // there is no need, so we simply use the current transform that's provided by 
          // the swapchain capabilities.
          //
          // --
          // On compositeAlpha: use VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR to ensure that we don't
          // use the alpha channel to perform a blend operation with other windows in the 
          // window system.
          //
          // --
          // On clipped: when other windows in the window system obscure portions of our 
          // window, we can use to tell the driver to not draw those portions. 
          // While the tutorial this references suggests using VK_TRUE here,
          // we instead use VK_FALSE. Technically speaking there isn't a clear indication
          // that using VK_TRUE will cause issues, but considering that framebuffer reads
          // are crucial to this application, it makes sense to avoid any potential
          // sources of error, as using VK_TRUE will definitely prevent writes to portions
          // of the screen that could be important for post processing (example: post processing
          // that relies on interpolation of already written data that's adjacent to something
          // which is visible, but itself is currently _not_ visible).
          //
          // --
          // On oldSwapChain: at the time of this writing, we keep this set to VK_NULL_HANDLE. This will need to be
          // none-null once we support things like window resizing, or anything that requires
          // re-initialization of the swapchain.
          // 
          {
            VkSwapchainCreateInfoKHR create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

            create_info.surface = m_vk_khr_surface;
            
            create_info.minImageCount = image_count;
            
            create_info.imageExtent = swap_extent;

            create_info.imageFormat = surface_format.format;
            create_info.imageColorSpace = surface_format.colorSpace;
            create_info.imageArrayLayers = 1;
            create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            queue_family_indices queue_indices = query_queue_families(m_vk_curr_pdevice, 
                                                                      m_vk_khr_surface);
            
            std::array<uint32_t, 2> array_indices = {
              queue_indices.graphics_family.value(),
              queue_indices.present_family.value()
            };

            if (queue_indices.graphics_family != queue_indices.present_family) {
              create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
              create_info.queueFamilyIndexCount = 2;
              create_info.pQueueFamilyIndices = array_indices.data();
            }
            else {
              create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
              create_info.queueFamilyIndexCount = 0;
              create_info.pQueueFamilyIndices = nullptr;
            }

            create_info.preTransform = details.capabilities.currentTransform;

            create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

            create_info.presentMode = present_mode;
            create_info.clipped = VK_FALSE;

            create_info.oldSwapchain = VK_NULL_HANDLE;

            VK_FN(vkCreateSwapchainKHR(m_vk_curr_ldevice, &create_info, nullptr, &m_vk_khr_swapchain));

            if (ok_swapchain()) {
              uint32_t count = 0;
              VK_FN(vkGetSwapchainImagesKHR(m_vk_curr_ldevice, 
                                             m_vk_khr_swapchain, 
                                             &count,
                                             nullptr));
              if (ok_swapchain()) {
                m_vk_swapchain_images.resize(count);
                VK_FN(vkGetSwapchainImagesKHR(m_vk_curr_ldevice, 
                                              m_vk_khr_swapchain, 
                                              &count,
                                              m_vk_swapchain_images.data()));

                m_vk_khr_swapchain_format = surface_format;
                m_vk_swapchain_extent = swap_extent;
              }
            }
          }
        }
      }
    }
    
    // make_image_views
    //
    // The idea behind this function is to generate image views from
    // a series of source images.
    //
    // One use case we have for this is the images in the swapchain:
    // we need to be able to present them to the screen after they've been 
    // rendered to. Another is for the processing of textures.
    //
    // Each image view requires a create-info struct, and this create-info struct
    // defines how the image itself is to be interpreted by the image view.
    // 
    // --
    // On image: represents the source image for the corresponding image view.
    //
    // --
    // On viewType: we'll stick with VK_IMAGE_VIEW_TYPE_2D for now. That said,
    // for textures we could, for example, specify VK_IMAGE_VIEW_TYPE_CUBE 
    // for a cube map.
    //
    // --
    // On format: the actual format of the image itself. 
    //
    // --
    // On components: these are designed to specify how different color channels should be used.
    // It's possible for each component to take on the value of, for example, the red channel.
    // It's also possible to set each component to a constant value (e.g., alpha always 1).
    //
    // --
    // On subresourceRange: this describes a) the image's purpose, and b) how it is to be accessed.
    // 
    darray<VkImageView> make_image_views(const darray<VkImage>& source_images, VkFormat format) {
      darray<VkImageView> ret{};
      ret.resize(source_images.size());

      for (size_t i = 0; i < ret.size(); ++i) {
        if (ok_swapchain()) {
          VkImageViewCreateInfo create_info = {};

          create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
          create_info.image = source_images[i];
          create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
          
          create_info.format = format;
          
          create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
          create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
          create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
          create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

          create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          create_info.subresourceRange.baseMipLevel = 0;
          create_info.subresourceRange.levelCount = 1;
          create_info.subresourceRange.baseArrayLayer = 0;
          create_info.subresourceRange.layerCount = 1;

          VK_FN(vkCreateImageView(m_vk_curr_ldevice, 
                                   &create_info,
                                   nullptr,
                                   &ret[i]));
        }
      }

      return ret;
    }

    void setup_swapchain_image_views() {
      if (ok_swapchain()) {
        m_vk_swapchain_image_views = make_image_views(m_vk_swapchain_images, 
                                                      m_vk_khr_swapchain_format.format);
      }
    }
    
    void setup_vertex_buffer() {
      if (ok_graphics_pipeline()) {
	const VkDeviceSize k_buf_verts_size =
	  sizeof(m_vertex_buffer_vertices[0]) * m_vertex_buffer_vertices.size();
	
	std::optional<VkMemoryRequirements> mem_reqs =
	  query_vertex_buffer_memory_requirements(k_buf_verts_size);

	ASSERT(mem_reqs.has_value());

	// This is a _required_ allocation size
	// for the vertex buffer - not using this size will create
	// an error in the API's validation layers.
	const VkDeviceSize k_buf_size = mem_reqs.value().size;
	
	VkBufferCreateInfo buffer_info = make_vertex_buffer_create_info(k_buf_size);
       
	VK_FN(vkCreateBuffer(m_vk_curr_ldevice, &buffer_info, nullptr, &m_vk_vertex_buffer));

	// find which memory type index is available
	// for this vertex buffer; the i'th bit
	// represents a memory type index,
	// and its bit is set if and only if that
	// memory type is supported.
	uint32_t memory_type_bits = mem_reqs.value().memoryTypeBits; 
	uint32_t memory_type_index = 0;
	while ((memory_type_bits & 1) == 0 && memory_type_index < 32) {
	  memory_type_index++;
	  memory_type_bits >>= 1;
	}
	// if this fails, we need to investigate
	// the host's hardware/level of vulkan support.
	ASSERT(memory_type_index < 32);

	if (ok() && m_vk_vertex_buffer != VK_NULL_HANDLE) {	  		
	  VkMemoryAllocateInfo balloc_info = {};
	  balloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	  balloc_info.pNext = nullptr;
	  balloc_info.allocationSize = k_buf_size;
	  balloc_info.memoryTypeIndex = memory_type_index;
	  
	  VK_FN(vkAllocateMemory(m_vk_curr_ldevice,
				 &balloc_info,
				 nullptr,
				 &m_vk_vertex_buffer_mem));

	  VK_FN(vkBindBufferMemory(m_vk_curr_ldevice,
				   m_vk_vertex_buffer,
				   m_vk_vertex_buffer_mem,
				   0));

	  if (ok()) {	    
	    m_ok_vertex_buffer = true;
	  }
	}
      }
    }

    void run_cmds(std::function<void(VkCommandBuffer cmd_buffer)> f,
		  std::function<void()> err_fn) {
      
      VkCommandBufferAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandPool = m_vk_command_pool;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      alloc_info.commandBufferCount = 1;

      VkCommandBuffer cmd_buffer{VK_NULL_HANDLE};	
      VK_FN(vkAllocateCommandBuffers(m_vk_curr_ldevice,
				     &alloc_info,
				     &cmd_buffer));

      

      if (cmd_buffer != VK_NULL_HANDLE) {
	if (ok()) {
	  VkCommandBufferBeginInfo begin_info = {};
	  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	  vkBeginCommandBuffer(cmd_buffer, &begin_info);

	  f(cmd_buffer);
	  
	  vkEndCommandBuffer(cmd_buffer);

	  VkSubmitInfo submit_info = {};
	  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	  submit_info.commandBufferCount = 1;
	  submit_info.pCommandBuffers = &cmd_buffer;

	  VK_FN(vkQueueSubmit(m_vk_graphics_queue, 1, &submit_info, VK_NULL_HANDLE));
	  VK_FN(vkQueueWaitIdle(m_vk_graphics_queue));
	}
	else if (err_fn) {
	  err_fn();
	}

        vkFreeCommandBuffers(m_vk_curr_ldevice,
			     m_vk_command_pool,
			     1,
			     &cmd_buffer);
      }
      else if (err_fn) {
	err_fn();
      }
    }

  public:
    ~renderer() {
      free_mem();
    }

    bool ok() const {
      bool r = api_ok();
      ASSERT(r);
      return r; 
    }
    
    bool ok_surface() const {
      bool r = ok() && m_vk_khr_surface != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    bool ok_pdev() const { 
      bool r = ok_surface() && m_vk_curr_pdevice != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    bool ok_ldev() const {
      bool r = ok_pdev() && m_vk_curr_ldevice != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    bool ok_swapchain() const {
      bool r = ok_ldev() && m_vk_khr_swapchain != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    bool ok_present() const {
      bool r = ok() && m_ok_present;
      ASSERT(r);
      return r;
    }

    bool ok_render_pass() const {
      bool r = ok() && m_ok_render_pass;
      ASSERT(r);
      return r;
    }

    bool ok_descriptor_pool() const {
      bool r = ok() && m_ok_descriptor_pool;
      ASSERT(r);
      return r;
    }

    bool ok_uniform_block_data() const {
      bool r = ok() && m_ok_uniform_block_data;
      ASSERT(r);
      return r;
    }
    
    bool ok_texture_data() const {
      bool r = ok() && m_ok_texture_data;
      ASSERT(r);
      return r;
    }

    bool ok_depthbuffer_data() const {
      bool r = ok() && m_ok_depthbuffer_data;
      ASSERT(r);
      return r;
    }

    bool ok_graphics_pipeline() const {
      bool r = ok() && m_ok_graphics_pipeline;
      ASSERT(r);
      return r;
    }

    bool ok_vertex_buffer() const {
      bool r = ok() && m_ok_vertex_buffer;
      ASSERT(r);
      return r;
    }

    bool ok_framebuffers() const {
      bool r = ok() && m_ok_framebuffers;
      ASSERT(r);
      return r;
    }

    bool ok_command_pool() const {
      bool r = ok() && m_ok_command_pool;
      ASSERT(r);
      return r;
    }

    bool ok_command_buffers() const {
      bool r = ok() && m_ok_command_buffers;
      ASSERT(r);
      return r;
    }

    bool ok_semaphores() const {
      bool r = ok() && m_ok_semaphores;
      ASSERT(r);
      return r;
    }

    bool ok_scene() const {
      bool r = ok() && m_ok_scene;
      ASSERT(r);
      return r;
    }

    uint32_t num_devices() const { return m_vk_physical_devs.size(); }

    bool is_device_suitable(VkPhysicalDevice device) {
      VkPhysicalDeviceProperties properties;
      VkPhysicalDeviceFeatures features;
      vkGetPhysicalDeviceProperties(device, &properties);
      vkGetPhysicalDeviceFeatures(device, &features);

      bool type_ok = 
        properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
        properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

      queue_family_indices indices = query_queue_families(device, m_vk_khr_surface);

      bool extensions_supported = check_device_extensions(device);

      return type_ok && 
             indices.ok() && 
             extensions_supported && 
             swapchain_ok(device);
    }

    void set_physical_device(uint32_t device) {
      ASSERT(device < num_devices());
      if (ok()) {
        bool can_use = is_device_suitable(m_vk_physical_devs[device]);
        ASSERT(can_use);
        if (can_use) {
          m_vk_curr_pdevice = m_vk_physical_devs[device];
        }
      }
    }

    void print_device_info(uint32_t device) {
      if (ok()) {
        if (device < m_vk_physical_devs.size()) {
          VkPhysicalDeviceProperties props = {};
	  
          vkGetPhysicalDeviceProperties(m_vk_physical_devs[device],
                                       &props);

          std::stringstream ss; 

          ss << "device " << device << "\n"
             << "--- apiVersion: " << props.apiVersion << "\n"
             << "--- driverVersion: " << props.driverVersion << "\n"
             << "--- vendorID: " << SS_HEX(props.vendorID) << "\n"
             << "--- deviceID: " << SS_HEX(props.deviceID) << "\n"
             << "--- deviceType: " << SS_HEX(props.deviceType) << "\n"
             << "--- deviceName: " << props.deviceName << "\n"
             << "--- piplineCacheUUID: <OMITTED> \n" 
             << "--- limits: <OMITTED> \n"
             << "--- sparseProperties: <OMITTED> \n";

          write_logf("%s", ss.str().c_str());
        }
      }
    }

    void query_physical_devices() {
      if (ok()) {
        uint32_t device_count = 0;
        VK_FN(vkEnumeratePhysicalDevices(m_vk_instance, 
                                          &device_count, 
                                          nullptr));

        if (ok()) {
          m_vk_physical_devs.resize(device_count);

          VK_FN(vkEnumeratePhysicalDevices(m_vk_instance,
                                            &device_count,
                                            &m_vk_physical_devs[0]));
        }
      }
    }

    darray<const char*> query_layers() {
      darray<const char*> ret;
      if (ok()) {
        uint32_t layer_count = 0;
        VK_FN(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));
        if (ok()) {
          darray<VkLayerProperties> avail_layers(layer_count);
          VK_FN(vkEnumerateInstanceLayerProperties(&layer_count, &avail_layers[0]));

          std::stringstream ss;
          ss << "Vulkan Layers found:\n";
          for (const auto& properties: avail_layers) {
            ss << "--- " << properties.layerName << "\n";
          }
          write_logf("%s", ss.str().c_str());

          for (const auto& info: s_layers) {
            if (info.enable) {
              for (const auto& properties: avail_layers) {
                if (strcmp(properties.layerName, info.name) == 0) {
                  ret.push_back(info.name);
                  break;
                }
              }
            }
          }
        }
      }
      return ret;
    }

    bool init_context() {
      auto avail_layers = query_layers();

      VkApplicationInfo app_info = {};
      VkInstanceCreateInfo create_info = {};

      app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
      app_info.pApplicationName = "Renderer";
      app_info.apiVersion = VK_API_VERSION_1_1;
      app_info.applicationVersion = 1;

      create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      create_info.pApplicationInfo = &app_info;

      uint32_t ext_count = 0;
      const char** extensions = glfwGetRequiredInstanceExtensions(&ext_count);

      create_info.enabledExtensionCount = ext_count;
      create_info.ppEnabledExtensionNames = extensions;

      create_info.enabledLayerCount = static_cast<uint32_t>(avail_layers.size());
      create_info.ppEnabledLayerNames = ( !avail_layers.empty() )
                                        ? avail_layers.data()
                                        : nullptr;

      std::stringstream ss ;
      ss << "Creating Vulkan instance with the following GLFW extensions:\n";
      for (uint32_t i = 0; i < ext_count; ++i) {
        ss << "--- " << extensions[i] << "\n";
      }
      write_logf("%s\n", ss.str().c_str());

      VK_FN(vkCreateInstance(&create_info, nullptr, &m_vk_instance));

      if (ok()) {
        if (setup_surface()) {
          query_physical_devices();
        }
      }

      return api_ok();
    }

    void setup_presentation() {
      setup_device_and_queues();
      setup_swapchain();
      setup_swapchain_image_views();

      if (ok_swapchain()) {
	m_ok_present = true;
      }
    }
    
    void setup_descriptor_pool() {
      if (ok_present()) {       
	std::vector<VkDescriptorPoolSize> pool_sizes =
	  {
	   // type, descriptorCount
	   { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
	   { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
	  };

	uint32_t max_sets = sum<uint32_t,
				VkDescriptorPoolSize>(pool_sizes,
						      [](const VkDescriptorPoolSize& x) -> uint32_t
						      { return x.descriptorCount; });
	
	VkDescriptorPoolCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.maxSets = max_sets;
	create_info.poolSizeCount = pool_sizes.size();
	create_info.pPoolSizes = pool_sizes.data();

	VK_FN(vkCreateDescriptorPool(m_vk_curr_ldevice,
				     &create_info,
				     nullptr,
				     &m_vk_descriptor_pool));
	
	m_ok_descriptor_pool = api_ok() && m_vk_descriptor_pool != VK_NULL_HANDLE;
      }
    }

    bool setup_render_pass(int index, render_pass_gen_params params) {
      m_render_pass_indices[index] =
	  m_render_pass_pool.make_render_pass(make_device_resource_properties(),
					      params);       

      return m_render_pass_pool.ok_render_pass(m_render_pass_indices[index]);
    }

    bool setup_render_pass_texture2d() {
      return
	setup_render_pass(k_render_phase_texture2d,
			  {
			   // attachment params
			   {
			    // color buffer info
			    {
			     m_vk_khr_swapchain_format.format,	       				
			     // load op
			     VK_ATTACHMENT_LOAD_OP_LOAD,
			     // store op
			     VK_ATTACHMENT_STORE_OP_STORE,
			     // layout info
			     {
			      // initial
			      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			      // attachment layout
			      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			      // final layout
			      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			     }
			    },
			    // depth buffer info
			    {
			     depthbuffer_data::k_format,				
			     // load op
			     VK_ATTACHMENT_LOAD_OP_LOAD,
			     // store op
			     VK_ATTACHMENT_STORE_OP_STORE,
			     // layout info
			     {
			      // initial
			      depthbuffer_layouts::primary(),
			      // attachment layout
			      depthbuffer_layouts::primary(),
			      // final layout
			      depthbuffer_layouts::primary()
			     }			       
			    }
			   },
			   // additional dependencies
			   {}
			  });
    }

    bool setup_render_pass_texture2d_standalone() {
      return
	setup_render_pass(k_render_phase_texture2d,
			  {
			   // attachment params
			   {
			    // color buffer info
			    {
			     m_vk_khr_swapchain_format.format,	       				
			     // load op
			     VK_ATTACHMENT_LOAD_OP_CLEAR,
			     // store op
			     VK_ATTACHMENT_STORE_OP_STORE,
			     // layout info
			     {
			      // initial
			      VK_IMAGE_LAYOUT_UNDEFINED,
			      // attachment layout
			      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			      // final layout
			      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			     }
			    },
			    // depth buffer info
			    {
			     depthbuffer_data::k_format,				
			     // load op
			     VK_ATTACHMENT_LOAD_OP_CLEAR,
			     // store op
			     VK_ATTACHMENT_STORE_OP_STORE,
			     // layout info
			     {
			      // initial
			      depthbuffer_data::k_initial_layout,
			      // attachment layout
			      depthbuffer_layouts::primary(),
			      // final layout
			      depthbuffer_layouts::primary()
			     }			       
			    }
			   },
			   // additional dependencies
			   {}
			  });
    } 

    bool setup_render_pass_cubemap() {
      return
	setup_render_pass(k_render_phase_cubemap,
			  {
			   // attachment params
			   {
			    // swapchain color buffer info
			    {
			     m_vk_khr_swapchain_format.format,	       				
			     // load op
			     VK_ATTACHMENT_LOAD_OP_CLEAR,
			     // store op
			     VK_ATTACHMENT_STORE_OP_STORE,
			     // layout info
			     {
			      // initial
			      VK_IMAGE_LAYOUT_UNDEFINED,
			      // attachment layout
			      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			      // final layout
			      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			     
			     }
			    },
			    // main pass color buffer info
			    { 
			     m_vk_khr_swapchain_format.format,	       				
			     // load op
			     VK_ATTACHMENT_LOAD_OP_CLEAR,
			     // store op
			     VK_ATTACHMENT_STORE_OP_STORE,
			     // layout info
			     {
			      // initial
			      VK_IMAGE_LAYOUT_UNDEFINED,
			      // attachment layout
			      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			      // final layout
			      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			      
			     }
			    },
			    // depth buffer info
			    {
			     depthbuffer_data::k_format,				
			     // load op
			     VK_ATTACHMENT_LOAD_OP_CLEAR,
			     // store op
			     VK_ATTACHMENT_STORE_OP_STORE,
			     // layout info
			     {
			      // initial
			      depthbuffer_data::k_initial_layout,
			      // attachment layout
			      depthbuffer_layouts::primary(),
			      // final layout
			      depthbuffer_layouts::primary()
			     }			       
			    }
			   },
			   // additional dependencies
			   {// for swapchain image
			    {
			     // src subpass index
			     0,
			     // dst subpass index
			     0,
			     // src stage mask
			     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			     // dst stage mask
			     VK_PIPELINE_STAGE_TRANSFER_BIT,
			     // src access mask
			     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			     // dst access mask
			     VK_ACCESS_TRANSFER_WRITE_BIT
			    }
			   }
			  });
    }

    render_pass_image_create_params make_render_pass_image_params() {
      return render_pass_image_create_params
	{									
	 &m_image_pool,								
	 m_vk_swapchain_image_views.size(),		
	 m_vk_swapchain_extent.width,			
	 m_vk_swapchain_extent.height,			
	 m_vk_khr_swapchain_format.format				    
	};
    }
    
    void setup_render_pass() {
      if (ok_descriptor_pool()) {
	STATIC_IF (k_test_multipass) {
	  m_ok_render_pass =
	    setup_render_pass_texture2d() &&
	    setup_render_pass_cubemap() &&
	    m_first_pass.make_images(make_device_resource_properties(),
				     make_render_pass_image_params());

	}
	else {
	  m_ok_render_pass = setup_render_pass_texture2d_standalone();
	}
      }
    }

    void setup_uniform_block_data() {
      if (ok_render_pass()) {
	{
	  descriptor_set_gen_params uniform_block_desc_set_params =
	    {
	     {
	      VK_SHADER_STAGE_VERTEX_BIT // binding 0: transform block
	     },
	     // separate descriptors
	     {
	      1
	     },
	     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	    };

	  m_test_descriptor_set_indices[k_descriptor_set_uniform_blocks] =
	    m_descriptor_set_pool.make_descriptor_set(make_device_resource_properties(),
						      uniform_block_desc_set_params);
	}

	m_uniform_block_pool.set_descriptor_set_pool(&m_descriptor_set_pool);
	
	{
	  uniform_block_gen_params transform_block_params =
	    {
	     m_test_descriptor_set_indices[k_descriptor_set_uniform_blocks],	   

	     static_cast<void*>(&m_transform_uniform_block.data),
	     sizeof(m_transform_uniform_block.data),
	     // array element index
	     0,
	     // binding index
	     0
	    };
	
	  m_transform_uniform_block.index =
	    m_uniform_block_pool.make_uniform_block(make_device_resource_properties(),
						    transform_block_params);

	  m_transform_uniform_block.pool = &m_uniform_block_pool;
	}


	m_ok_uniform_block_data = m_transform_uniform_block.ok();
      }
    }
    
    void setup_texture_data() {
      if (ok_uniform_block_data()) {
	m_texture_pool.set_image_pool(&m_image_pool);
	m_texture_pool.set_descriptor_set_pool(&m_descriptor_set_pool);

	//
	// create our descriptor set that's used
	// for the 2d samplers
	//
	{
	  descriptor_set_gen_params descriptor_set_params =
	    {
	     // darray<VkShaderStageFlags> stages
	     {
	      VK_SHADER_STAGE_FRAGMENT_BIT
	     },
	     {
	      2
	     },
	     // VkDescriptorType type
	     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	    };

	  m_test_descriptor_set_indices[k_descriptor_set_samplers] =
	    m_descriptor_set_pool.make_descriptor_set(make_device_resource_properties(),
						      descriptor_set_params);
	}

	//
	// create our descriptor set that's used for
	// the cubemap
	// 
	{
	  descriptor_set_gen_params descriptor_set_params =
	    {
	     // darray<VkShaderStageFlags> stages
	     {
	      VK_SHADER_STAGE_FRAGMENT_BIT
	     },
	     {
	      1
	     },
	     // VkDescriptorType type
	     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	    };

	  m_test_descriptor_set_indices[k_descriptor_set_sampler_cubemap] =
	    m_descriptor_set_pool.make_descriptor_set(make_device_resource_properties(),
						      descriptor_set_params);
	}

	//
	// make first image/texture
	//

	// reuse these 4 variables for both images
	uint32_t image_w = 256;
	uint32_t image_h = 256;

	uint32_t bpp = 4;

	darray<uint8_t> buffer(image_w * image_h * bpp, 0);

	// this creates a checkerboard pattern
	{
	  uint8_t rgb = 0;

	  uint32_t mask = 7;

	  uint32_t y = 0;
	  while (y < image_h) {
	    uint32_t x = 0;
	    while (x < image_w) {
	      uint32_t offset = (y * image_w + x) * bpp;
	    
	      buffer[offset + 0] = 255 & rgb;
	      buffer[offset + 1] = 255 & rgb;
	      buffer[offset + 2] = 255 & rgb;
	      buffer[offset + 3] = 255;
	    
	      x++;

	      if (((x + 1) & mask) == 0) {
		rgb = ~rgb;
	      }
	    }

	    y++;

	    if (((y + 1) & mask) == 0) {
	      rgb = ~rgb;
	    }
	  }
	}

	
	image_gen_params image_params =
	  {
	   // data
	   static_cast<void*>(buffer.data()), 
	   // memory 
	   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	   // format
	   VK_FORMAT_R8G8B8A8_UNORM,
	   // attachment layout
	   VK_IMAGE_LAYOUT_UNDEFINED,
	   // initial layout
	   VK_IMAGE_LAYOUT_PREINITIALIZED,
	   // final layout
	   VK_IMAGE_LAYOUT_GENERAL,
	   // tiling
	   VK_IMAGE_TILING_LINEAR,
	   // usage flags
	   VK_IMAGE_USAGE_SAMPLED_BIT,
	   // type
	   VK_IMAGE_TYPE_2D,
	   // view type
	   VK_IMAGE_VIEW_TYPE_2D,
	    
	   // aspect flags
	   VK_IMAGE_ASPECT_COLOR_BIT,
	   // source pipeline stage
	   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	   // dest pipeline stage
	   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	   // source access flags
	   0,
	   // dest access flags
	   VK_ACCESS_SHADER_READ_BIT,
	    
	   // width
	   image_w,
	   // height
	   image_h,
	   // depth
	   1
	  };
	
	m_test_image_indices[0] = m_image_pool.make_image(make_device_resource_properties(),
							  image_params);
	
	texture_gen_params texture_params =
	  {
	   m_test_image_indices[0],
	   // descriptor set index
	   m_test_descriptor_set_indices[k_descriptor_set_samplers],
	   // descriptor array element
	   0,
	   // binding index
	   0,
	  };
	
	m_test_texture_indices[0] = m_texture_pool.make_texture(make_device_resource_properties(),
								texture_params);

	//
	// make second image/texture
	//

	darray<uint8_t> buffer2(image_w * image_h * bpp, 255);
	
	// this creates a simple aqua image
	{
	  uint32_t y = 0;
	  while (y < image_h) {
	    uint32_t x = 0;
	    while (x < image_w) {
	      uint32_t offset = (y * image_w + x) * bpp;
	    
	      buffer2[offset + 0] = 0;
	      buffer2[offset + 1] = 200;
	      buffer2[offset + 2] = 255;
	      buffer2[offset + 3] = 255;
	    
	      x++;
	    }

	    y++;
	  }
	}

	image_params.data = static_cast<void*>(buffer2.data());

	m_test_image_indices[1] =
	  m_image_pool.make_image(make_device_resource_properties(),
				  image_params);

	texture_params.image_index = m_test_image_indices[1];
	texture_params.descriptor_array_element = 1;

	m_test_texture_indices[1] =
	  m_texture_pool.make_texture(make_device_resource_properties(),
				      texture_params);

	//
	// validate
	//
	
	m_ok_texture_data =
	  m_texture_pool.ok_texture(m_test_texture_indices[0]) &&
	  m_texture_pool.ok_texture(m_test_texture_indices[1]);
      }
    }

    void setup_depthbuffer_data() {
      if (ok_texture_data()) {
	m_depthbuffer = make_depthbuffer(make_device_resource_properties(),
					 g_m.device_ctx->width(),
					 g_m.device_ctx->height());
	
	m_ok_depthbuffer_data = m_depthbuffer.ok();
      }
    }


    bool setup_pipeline(int render_phase_index,
			pipeline_layout_gen_params layout_params,
			pipeline_gen_params params) {
      bool success = false;

      params.render_pass_index = m_render_pass_indices.at(render_phase_index);
      
      m_pipeline_layout_indices[render_phase_index] =
	m_pipeline_layout_pool.make_pipeline_layout(make_device_resource_properties(),
						    layout_params);
      
      params.pipeline_layout_index = m_pipeline_layout_indices.at(render_phase_index);
      
      if (m_pipeline_layout_pool.ok_pipeline_layout(m_pipeline_layout_indices[0])) {
		  
	m_pipeline_indices[render_phase_index] =
	  m_pipeline_pool.make_pipeline(make_device_resource_properties(),
				        params);

	success = m_pipeline_pool.ok_pipeline(m_pipeline_indices[render_phase_index]);
      }

      return success;
    }
    
    bool setup_pipeline_texture2d() {      	
      return setup_pipeline(k_render_phase_texture2d,
			    // pipeline layout
			    {
			     // descriptor set layouts
			     {
			      descriptor_set_layout(k_descriptor_set_samplers),
			      
			      descriptor_set_layout(k_descriptor_set_uniform_blocks)
			     },
			     // push constant ranges
			     {
			      {
			       VK_SHADER_STAGE_FRAGMENT_BIT,
			       0,
			       sizeof(int)
			      }
			     }
			    },
			    // pipeline
			    {
			     // viewport extent
			     m_vk_swapchain_extent,	   
			     // vert spv path
			     realpath_spv("tri_ubo.vert.spv"),
			     // frag spv path
			     realpath_spv("tri_ubo.frag.spv")

			     // remaining index parameters handled in setup_pipeline()
			    });
    }

    bool setup_pipeline_cubemap() {
      return setup_pipeline(k_render_phase_cubemap,
#if 0
			    // pipeline layout
			    {
			     // descriptor set layouts
			     {
			      descriptor_set_layout(k_descriptor_set_sampler_cubemap),
			      
			      descriptor_set_layout(k_descriptor_set_uniform_blocks)
			     },
			     // push constant ranges
			     {}
			    },
			    // pipeline
			    {
			     // viewport extent
			     m_vk_swapchain_extent,	   
			     // vert spv path
			     realpath_spv("cubemap.vert.spv"),
			     // frag spv path
			     realpath_spv("cubemap.frag.spv")

			     // remaining index parameters handled in setup_pipeline()
			    }
#else
			    // pipeline layout
			    {
			     // descriptor set layouts
			     {
			      descriptor_set_layout(k_descriptor_set_samplers),
			      
			      descriptor_set_layout(k_descriptor_set_uniform_blocks)
			     },
			     // push constant ranges
			     {
			      {
			       VK_SHADER_STAGE_FRAGMENT_BIT,
			       0,
			       sizeof(int)
			      }
			     }
			    },
			    // pipeline
			    {
			     // viewport extent
			     m_vk_swapchain_extent,	   
			     // vert spv path
			     realpath_spv("tri_ubo.vert.spv"),
			     // frag spv path
			     realpath_spv("tri_ubo.frag.spv")

			     // remaining index parameters handled in setup_pipeline()
			    }
#endif
			    );
    }
    
    void setup_graphics_pipeline() {      
      if (ok_depthbuffer_data()) {
	m_pipeline_pool.set_pipeline_layout_pool(&m_pipeline_layout_pool);
	m_pipeline_pool.set_render_pass_pool(&m_render_pass_pool);
	
	m_ok_graphics_pipeline =
	  setup_pipeline_texture2d();

	STATIC_IF (k_test_multipass) {
	  m_ok_graphics_pipeline = m_ok_graphics_pipeline && setup_pipeline_cubemap();
	}
      }
    }

    darray<VkFramebuffer> make_framebuffer_list(VkRenderPass fb_render_pass,
						const darray<VkImageView>& color_image_views) {
      darray<VkFramebuffer> ret(color_image_views.size(), VK_NULL_HANDLE);

      size_t i = 0;
      bool good = true;
      
      while (i < ret.size() && good) {
	std::array<VkImageView, 2> attachments =
	  {
	   color_image_views[i],
	   m_depthbuffer.image_view
	  };

	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.pNext = nullptr;
	
	framebuffer_info.flags = 0;

	framebuffer_info.renderPass = fb_render_pass;

	framebuffer_info.attachmentCount = attachments.size();
	framebuffer_info.pAttachments = attachments.data();

	framebuffer_info.width = m_vk_swapchain_extent.width;
	framebuffer_info.height = m_vk_swapchain_extent.height;
	framebuffer_info.layers = 1;

	VK_FN(vkCreateFramebuffer(m_vk_curr_ldevice,
				  &framebuffer_info,
				  nullptr,
				  &ret[i]));
	good = api_ok();
	i++;
      }

      if (!c_assert(good)) {
	ret.clear();
      }

      return ret;
    }
    
    void setup_framebuffers() {
      if (ok_vertex_buffer()) {
	m_vk_swapchain_framebuffers =
	  make_framebuffer_list(render_pass(k_render_phase_texture2d),
				m_vk_swapchain_image_views);	

	m_ok_framebuffers = !m_vk_swapchain_framebuffers.empty();
	
	STATIC_IF (k_test_multipass) {
	  if (m_ok_framebuffers) {

	    m_ok_framebuffers =
	      m_first_pass.make_framebuffers(make_device_resource_properties(),
					     { // image params
					      make_render_pass_image_params(),
					      m_vk_swapchain_image_views, 
					      render_pass(k_render_phase_cubemap),
					      m_depthbuffer.image_view,
					      [](size_t framebuffer_index,
						 attachment_list_t& attachments,
						 const darray<image_pool::index_type>& images,
						 const render_pass_framebuffer_create_params& self) {
					      
						attachments[0] =
						  self.next_pass_image_views.at(framebuffer_index);
						attachments[1] =
						  self.image_params.p_image_pool->image_view(images.at(framebuffer_index));
						attachments[2] =
						  self.depth_image_view;
					      }
					     });	  
	  }
	}
      }
    }

    void setup_command_pool() {
      if (ok_framebuffers()) {
	queue_family_indices indices = query_queue_families(m_vk_curr_pdevice, m_vk_khr_surface);

	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = indices.graphics_family.value();
	pool_info.flags = 0;

	VK_FN(vkCreateCommandPool(m_vk_curr_ldevice, &pool_info, nullptr, &m_vk_command_pool));

	m_ok_command_pool = true;
      }      
    }

    void commands_begin_pipeline(VkCommandBuffer cmd_buffer,
				 VkPipeline pipeline,
				 VkPipelineLayout pipeline_layout,
				 const darray<VkDescriptorSet>& descriptor_sets) {

      VkDeviceSize vertex_buffer_ofs = 0;
      
      vkCmdBindPipeline(cmd_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline);
	    
      vkCmdBindVertexBuffers(cmd_buffer,
			     0,
			     1,
			     &m_vk_vertex_buffer,
			     &vertex_buffer_ofs);

      vkCmdUpdateBuffer(cmd_buffer,
			m_vk_vertex_buffer,
			0,
			sizeof(m_vertex_buffer_vertices[0]) * m_vertex_buffer_vertices.size(),
			m_vertex_buffer_vertices.data());

      vkCmdBindDescriptorSets(cmd_buffer,
			      VK_PIPELINE_BIND_POINT_GRAPHICS,
			      pipeline_layout,
			      0,
			      descriptor_sets.size(),
			      descriptor_sets.data(),
			      0,
			      nullptr);

      image_layout_transition().
	from_stage(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT).
	to_stage(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT).
	for_aspect(depthbuffer_data::k_image_aspect_flags).
	from_access(0).
	to_access(depthbuffer_data::k_access_flags).
	from(depthbuffer_data::k_initial_layout).
	to(depthbuffer_layouts::primary()).
	for_image(m_depthbuffer.image).
	via(cmd_buffer);
    }

    void commands_start_render_pass(VkCommandBuffer cmd_buffer,
				    VkFramebuffer framebuffer,
				    VkRenderPass render_pass) {
      VkRenderPassBeginInfo render_pass_info = {};
      
      render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      render_pass_info.renderPass = render_pass;
      render_pass_info.framebuffer = framebuffer;

      // shader loads and stores take place within this region
      render_pass_info.renderArea.offset = {0, 0};
      render_pass_info.renderArea.extent = m_vk_swapchain_extent;

      std::array<VkClearValue, 2> clear_values;
	    
      clear_values[0].color.float32[0] = 1.0f;
      clear_values[0].color.float32[1] = 0.0f;
      clear_values[0].color.float32[2] = 0.0f;
      clear_values[0].color.float32[3] = 1.0f;

      clear_values[1].depthStencil.depth = 1.0f;
      clear_values[1].depthStencil.stencil = 0;
	    
      // for the color attachment's VK_ATTACHMENT_LOAD_OP_CLEAR
      // operation
      render_pass_info.clearValueCount = clear_values.size();
      render_pass_info.pClearValues = clear_values.data();
	    	    
      vkCmdBeginRenderPass(cmd_buffer,
			   &render_pass_info,
			   VK_SUBPASS_CONTENTS_INLINE);      
    }
    
    void setup_render_commands(darray<VkCommandBuffer>& command_buffers,
			       const darray<VkDescriptorSet>& descriptor_sets,
			       VkPipeline pipeline,
			       VkPipelineLayout pipeline_layout) {
      ASSERT(!command_buffers.empty());
      
      VkCommandBufferAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandPool = m_vk_command_pool;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());

      VK_FN(vkAllocateCommandBuffers(m_vk_curr_ldevice, &alloc_info, command_buffers.data()));
	
      size_t i = 0;

      auto draw_inner_objects =
	[this](VkCommandBuffer cmd_buffer) {
	  // first 3 objects: two triangles and one small cube
	  vkCmdDraw(cmd_buffer,
		    (m_instance_count - 12) * 3, // num vertices
		    m_instance_count - 12, // num instances
		    0, // first vertex
		    0); // first instance

	};

      auto draw_room =
	[](VkCommandBuffer cmd_buffer) {
	  // big cube encompassing the scene
	  vkCmdDraw(cmd_buffer,
		    36,
		    12, // (6 faces, 12 triangles)
		    42, // 3 vertices + 3 vertices + 36 vertices
		    14); // 2 triangles + one cube (6 faces, 12 triangles)
	};
	
      while (i < command_buffers.size() && ok()) {
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = nullptr;

	VK_FN(vkBeginCommandBuffer(command_buffers[i], &begin_info));	  
	  
	if (ok()) {
	  commands_begin_pipeline(command_buffers[i],
				  pipeline,
				  pipeline_layout,
				  descriptor_sets);
	  
	  commands_start_render_pass(command_buffers[i],
				     m_vk_swapchain_framebuffers[i],
				     render_pass(k_render_phase_texture2d));
	    
	  // note that instances are per-triangle
	  {
	    uint32_t sampler0 = 0;
	    vkCmdPushConstants(command_buffers[i],
			       pipeline_layout,
			       VK_SHADER_STAGE_FRAGMENT_BIT,
			       0,
			       sizeof(sampler0),
			       &sampler0);
	    
	    draw_inner_objects(command_buffers[i]);
	  
	    uint32_t sampler1 = 1;
	    vkCmdPushConstants(command_buffers[i],
			       pipeline_layout,
			       VK_SHADER_STAGE_FRAGMENT_BIT,
			       0,
			       sizeof(sampler1),
			       &sampler1);
	    draw_room(command_buffers[i]);
	  	    
	    vkCmdEndRenderPass(command_buffers[i]);
	  }

	  VK_FN(vkEndCommandBuffer(command_buffers[i]));
	}

	i++;
      }
    }

    // ---------------------
    // WRT descriptor sets
    // ---------------------
    // The descriptor set must be associated with an image for us
    // to sample it in the fragment shader.
    //
    // The call to vkUpdateDescriptorSets() handles this portion,
    // and we have to make this call before any command buffers
    // that rely on the association itself are used.
    //
    // In the command buffer, it's necessary that we bind our
    // graphics pipeline to the command buffer _before_ any other
    // pipeline-specific commands are performed.
    //
    // The one pipeline specific command that matters here is
    // the call to vkCmdBindDescriptorSets(), which
    // associates the descriptor set with the descriptor
    // set layout that was associated with the
    // pipeline layout created earlier.
    void setup_command_buffers() {
      if (ok_command_pool()) {
	// bind sampler[i] to test_texture_indices[i] via
	// test_texture_index's array element index (should be i)
	darray<texture_pool::index_type> tex_indices(m_test_texture_indices.begin(), m_test_texture_indices.end());
	
	if (c_assert(m_texture_pool.update_descriptor_sets(m_vk_curr_ldevice,
							   std::move(tex_indices)))) {

	  vkDeviceWaitIdle(m_vk_curr_ldevice);	

	  // perform the image layout transition for
	  // test_image_indices[i]
	  run_cmds(
		   [this](VkCommandBuffer cmd_buf) {
		     puts("image_layout_transition");

		     size_t i{0};
		     bool good = true;

		     while (i < m_test_image_indices.size() && good) {
		       auto image_index = m_test_image_indices[i];
		     
		       auto layout_transition = m_image_pool.make_layout_transition(image_index);		    

		       good = layout_transition.ok();

		       if (good) {
			 layout_transition.via(cmd_buf);
		       }

		       i++;
		     }
		   
		     m_ok_scene = good;
		   },
		   [this]() {
		     puts("run_cmds ERROR");
		     ASSERT(false);
		     m_ok_scene = false;
		   });

	  if (ok_scene()) {

	    darray<VkDescriptorSet> descriptor_sets =
	      {
	       m_descriptor_set_pool.descriptor_set(m_test_descriptor_set_indices[k_descriptor_set_samplers]),
	       m_descriptor_set_pool.descriptor_set(m_test_descriptor_set_indices[k_descriptor_set_uniform_blocks])
	      };

	    m_vk_command_buffers.resize(m_vk_swapchain_framebuffers.size());
	    
	    setup_render_commands(m_vk_command_buffers,
				  descriptor_sets,
				  m_pipeline_pool.pipeline(m_pipeline_indices[0]),
				  pipeline_layout(k_render_phase_texture2d));	  
	
	    if (ok()) {
	      m_ok_command_buffers = true;
	    }
	  }
	}
      }
    }

    void setup_semaphores() {
      if (ok_command_buffers()) {
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VK_FN(vkCreateSemaphore(m_vk_curr_ldevice,
				&semaphore_info,
				nullptr,
				&m_vk_sem_image_available));

	VK_FN(vkCreateSemaphore(m_vk_curr_ldevice,
				&semaphore_info,
				nullptr,
				&m_vk_sem_render_finished));

	if (ok()) {
	  m_ok_semaphores = true;
	}
      }
    }

    void setup_scene() {
      if (ok_semaphores()) {
						 
	m_ok_scene = true;
      }
    }

    void setup() {
      setup_presentation();
      setup_descriptor_pool();
      setup_render_pass();
      setup_uniform_block_data();
      setup_texture_data();
      setup_depthbuffer_data();
      setup_graphics_pipeline();
      setup_vertex_buffer();
      setup_framebuffers();
      setup_command_pool();
      setup_command_buffers();
      setup_semaphores();
      setup_scene();
    }

    void set_world_to_view_transform(const mat4_t& w2v) {
      m_transform_uniform_block.data.world_to_view = w2v;
    }

    void set_view_to_clip_transform(const mat4_t& v2c) {
      m_transform_uniform_block.data.view_to_clip = v2c;
    }

    void render() {
      if (ok_scene()) {
	m_uniform_block_pool.update_block(m_transform_uniform_block.index,
					  m_vk_curr_ldevice);
	
	VK_FN(vkDeviceWaitIdle(m_vk_curr_ldevice));
	
	constexpr uint64_t k_timeout_ns = 10000000000; // 10 seconds
	uint32_t image_index = UINT32_MAX;
	
	VK_FN(vkAcquireNextImageKHR(m_vk_curr_ldevice,
				    m_vk_khr_swapchain,
				    k_timeout_ns,
				    m_vk_sem_image_available,
				    VK_NULL_HANDLE,
				    &image_index));

	if (ok()) {	  
	  ASSERT(m_vk_command_buffers.size() == m_vk_swapchain_images.size());
	  ASSERT(image_index < m_vk_command_buffers.size());

	  VkSubmitInfo submit_info = {};
	  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	  VkSemaphore wait_semaphores[] = { m_vk_sem_image_available };
	  
	  VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };	  	  
	  
	  submit_info.waitSemaphoreCount = 1;
	  submit_info.pWaitSemaphores = wait_semaphores;
	  submit_info.pWaitDstStageMask = wait_stages;

	  submit_info.commandBufferCount = 1;
	  submit_info.pCommandBuffers = &m_vk_command_buffers[image_index];

	  VkSemaphore signal_semaphores[] = { m_vk_sem_render_finished  };
	  submit_info.signalSemaphoreCount = 1;
	  submit_info.pSignalSemaphores = signal_semaphores;

	  VK_FN(vkQueueSubmit(m_vk_graphics_queue,
			      1,
			      &submit_info,
			      VK_NULL_HANDLE));

	  VkPresentInfoKHR present_info = {};
	  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	  present_info.waitSemaphoreCount = 1;
	  present_info.pWaitSemaphores = signal_semaphores;

	  VkSwapchainKHR swap_chains[] = { m_vk_khr_swapchain };

	  present_info.swapchainCount = 1;
	  present_info.pSwapchains = swap_chains;
	  present_info.pImageIndices =  &image_index;

	  present_info.pResults = nullptr;

	  VK_FN(vkQueuePresentKHR(m_vk_present_queue, &present_info));

	  VK_FN(vkDeviceWaitIdle(m_vk_curr_ldevice));
	}
      }
    }
    
    template <class vkHandleType, void (*vkDestroyFn)(vkHandleType, const VkAllocationCallbacks*)>
    void free_vk_handle(vkHandleType& handle) const {
      if (handle != VK_NULL_HANDLE) {
        vkDestroyFn(handle, nullptr);
        handle = VK_NULL_HANDLE;
      }
    }

    template <class vkHandleType, 
              void (*vkDestroyFn)(VkInstance, vkHandleType, const VkAllocationCallbacks*)>
    void free_vk_instance_handle(vkHandleType& handle) const {
      if (handle != VK_NULL_HANDLE) {
        vkDestroyFn(m_vk_instance, handle, nullptr);
        handle = VK_NULL_HANDLE;
      }
    }

    template <class vkHandleType,
              void (*vkDestroyFn)(VkDevice, vkHandleType, const VkAllocationCallbacks*)>
    void free_vk_ldevice_handle(vkHandleType& handle) const {
      if (ok_ldev()) {
	VK_FN(vkDeviceWaitIdle(m_vk_curr_ldevice));
        if (handle != VK_NULL_HANDLE) {
          vkDestroyFn(m_vk_curr_ldevice, handle, nullptr);
          handle = VK_NULL_HANDLE;
        }
      }
    }
    
    template <class vkHandleType,
              void (*vkDestroyFn)(VkDevice, vkHandleType, const VkAllocationCallbacks*)>
    void free_vk_ldevice_handles(darray<vkHandleType>& handles) const {
      if (ok_ldev()) {
	VK_FN(vkDeviceWaitIdle(m_vk_curr_ldevice));
        for (auto h: handles) {
          if (h != VK_NULL_HANDLE) {
            vkDestroyFn(m_vk_curr_ldevice, h, nullptr);
          }
        }
        handles.clear();
      }
    }

    // VkSwapchainKHR owns the VkImages
    // we've derived from it (in m_vk_swapchain_images),
    // so we don't free those VkImages ourselves
    //
    // The command pool also will free any allocated command
    // buffers.
    void free_mem() {
      if (m_vk_curr_ldevice != VK_NULL_HANDLE) {
	vkDeviceWaitIdle(m_vk_curr_ldevice);
      }

      m_first_pass.free_mem(m_vk_curr_ldevice);
      
      free_vk_ldevice_handle<VkDeviceMemory, &vkFreeMemory>(m_vk_vertex_buffer_mem);
      
      free_vk_ldevice_handle<VkBuffer, &vkDestroyBuffer>(m_vk_vertex_buffer);
      
      free_vk_ldevice_handle<VkSemaphore, &vkDestroySemaphore>(m_vk_sem_image_available);
      free_vk_ldevice_handle<VkSemaphore, &vkDestroySemaphore>(m_vk_sem_render_finished);
      
      free_vk_ldevice_handle<VkCommandPool, &vkDestroyCommandPool>(m_vk_command_pool);
      
      free_vk_ldevice_handles<VkFramebuffer, &vkDestroyFramebuffer>(m_vk_swapchain_framebuffers);
      
      m_pipeline_pool.free_mem(m_vk_curr_ldevice);
      
      m_pipeline_layout_pool.free_mem(m_vk_curr_ldevice);

      m_render_pass_pool.free_mem(m_vk_curr_ldevice);
      
      m_texture_pool.free_mem(m_vk_curr_ldevice);
      m_image_pool.free_mem(m_vk_curr_ldevice);
      
      m_depthbuffer.free_mem(m_vk_curr_ldevice);

      m_uniform_block_pool.free_mem(m_vk_curr_ldevice);

      // UBO descriptor set will eventually be moved over
      // to this pool. In fact, the actual VkDescriptorPool
      // should be moved to this class as well.
      m_descriptor_set_pool.free_mem(m_vk_curr_ldevice); 
      
      free_vk_ldevice_handle<VkDescriptorPool, &vkDestroyDescriptorPool>(m_vk_descriptor_pool);
      
      free_vk_ldevice_handles<VkImageView, &vkDestroyImageView>(m_vk_swapchain_image_views);
      
      free_vk_ldevice_handle<VkSwapchainKHR, &vkDestroySwapchainKHR>(m_vk_khr_swapchain);
      // vkDestroyDevice will destroy any associated queues along with it.
      free_vk_handle<VkDevice, &vkDestroyDevice>(m_vk_curr_ldevice);
      free_vk_instance_handle<VkSurfaceKHR, &vkDestroySurfaceKHR>(m_vk_khr_surface);
      free_vk_handle<VkInstance, &vkDestroyInstance>(m_vk_instance);
    }
  };
}
