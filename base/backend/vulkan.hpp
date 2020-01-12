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
#include "vk_uniform_buffer.hpp"
#include "vk_image.hpp"

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
  
  VkPipelineVertexInputStateCreateInfo default_vertex_input_state_settings();
  VkPipelineInputAssemblyStateCreateInfo default_input_assembly_state_settings();
  VkPipelineViewportStateCreateInfo default_viewport_state_settings();
  VkPipelineRasterizationStateCreateInfo default_rasterization_state_settings();
  VkPipelineMultisampleStateCreateInfo default_multisample_state_settings();
  VkPipelineColorBlendAttachmentState default_color_blend_attach_state_settings();
  VkPipelineColorBlendStateCreateInfo default_color_blend_state_settings();
  VkPipelineLayoutCreateInfo default_pipeline_layout_settings();

  VkAttachmentDescription default_colorbuffer_settings(VkFormat swapchain_format);
  VkAttachmentReference default_colorbuffer_ref_settings();

  VkAttachmentDescription default_depthbuffer_settings();
  VkAttachmentReference default_depthbuffer_ref_settings();

  VkStencilOpState default_stencilop_state();
  
  VkShaderModuleCreateInfo make_shader_module_settings(darray<uint8_t>& spv_code);

  VkPipelineShaderStageCreateInfo make_shader_stage_settings(VkShaderModule module, VkShaderStageFlagBits type);  
    
  depthbuffer_data make_depthbuffer(const device_resource_properties& properties,
				    uint32_t width,
				    uint32_t height);
    
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

  //
  // The following struct defines a simple
  // fluent interface for a VkImageMemoryBarrier/VkCommandBuffer
  // operation.
  //
  // ---------------------
  // WRT image layout transitions
  // ---------------------
  // Vulkan requires that images be created
  // with a particular layout, and then
  // sampled with a particular layout.
  //
  // In a nutshell, the same image must use
  // different layouts for different tasks.
  //
  // In addition, we also must keep track
  // of synchronization and memory dependencies
  // that are associated with transitions between
  // layouts at different pipeline stages.
  //
  // For this particular case, we use a pipeline barrier
  // that is inserted between two stages. These stages
  // may be different, or they may the same. To resolve
  // ambiguity, we also expect a before and after
  // state that is expressed in terms of overall usage of
  // the memory that we plan to use.
  //
  // For example, the state of the image
  // is not guaranteed unless we explicitly
  // make it clear that at pipeline stage A
  // we expect permission set (1) to hold for the image,
  // and then once we've reached pipeline stage B,
  // we expect permission set (2) to hold for the image.
  //
  // By permission set, we mean the actual accessibility
  // constraints (read/write capabilities, usage that's associated
  // with stages, etc.).
  //
  // It's also worth noting that pipeline stage B may or may not
  // come directly after pipeline stage A. We simply know that
  // it occurs at _some point after_ pipeline stage A. There may
  // or may not be intermediate pipeline stages A0 -> A1 -> A2 -> ... -> An
  // that occur before B is hit.
  //
  // That's a loose, general definition of the overal concept. In our case,
  // for basic texturing in the shader, we're inserting
  // the pipeline barrier somewhere between the very beginning
  // of the pipeline, and the fragment shader stage.
  //
  // At the time of insertion, we communicate to the driver that we do not
  // expect any particular access capabilities for our image memory.
  // _After_ the image memory barrier, once we've reached the fragment shader stage,
  // we _do_, however, require that we can read the image memory within the shader.
  //
  // Note that, because we are recording an image layout transition inside of a
  // command buffer via the memory barrier, we also require a subpass dependency
  // that's created with the pipeline. This subpass dependency refers to itself,
  // and is defined as follows:
  // 
  //	VkSubpassDependency self_dependency = {};
  //	self_dependency.srcSubpass = 0;
  //	self_dependency.dstSubpass = 0;
  //	self_dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  //	self_dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  //	self_dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  //	self_dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
  //
  // Currently, our only subpass is the 0th subpass. If additional subpasses
  // are used, that number is subject to change.
  //
  struct image_layout_transition {
    VkImageMemoryBarrier barrier;

    VkPipelineStageFlags src_stage_mask;
    VkPipelineStageFlags dst_stage_mask;
    
    image_layout_transition() {
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.pNext = nullptr;
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = 0;
	
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;
    }

    image_layout_transition& from_stage(VkPipelineStageFlags flags) {
      src_stage_mask = flags; return *this;
    }

    image_layout_transition& to_stage(VkPipelineStageFlags flags) {
      dst_stage_mask = flags; return *this;
    }

    image_layout_transition& for_aspect(VkImageAspectFlags aspect_mask) {
      barrier.subresourceRange.aspectMask = aspect_mask; return *this;
    }

    image_layout_transition& from_access(VkAccessFlags src_mask) {
      barrier.srcAccessMask = src_mask; return *this;
    }

    image_layout_transition& to_access(VkAccessFlags dst_mask) {
      barrier.dstAccessMask = dst_mask; return *this;
    }

    image_layout_transition& from(VkImageLayout layout) {
      barrier.oldLayout = layout; return *this;
    }

    image_layout_transition& to(VkImageLayout layout) {
      barrier.newLayout = layout; return *this;
    }

    image_layout_transition& for_image(VkImage image) {
      barrier.image = image; return *this;
    }

    image_layout_transition& via(VkCommandBuffer buffer) {       
      if (api_ok()) {
	vkCmdPipelineBarrier(buffer,
			     src_stage_mask,
			     dst_stage_mask,
			     0,
			     0,
			     nullptr,
			     0,
			     nullptr,
			     1,
			     &barrier);
			       
      }
      return *this;
    }
  };

  struct transform_data {
    mat4_t view_to_clip{R(1.0)};
    mat4_t world_to_view{R(1.0)};
  };

  struct vertex_data {
    vec3_t position;
    vec2_t st;
    vec3_t color;
  };

  struct rot_cmd {
    vec3_t axes;
    real_t rad;
  };
  
  using vertex_list_t = darray<vertex_data>;
  
  class renderer {
    uint32_t m_instance_count{0};
    
    std::unique_ptr<uniform_block_pool> m_uniform_block_pool{nullptr};
    
    darray<VkPhysicalDevice> m_vk_physical_devs;

    darray<VkImage> m_vk_swapchain_images;

    darray<VkImage> m_vk_depth_buffers;
    
    darray<VkImageView> m_vk_swapchain_image_views;

    darray<VkFramebuffer> m_vk_swapchain_framebuffers;

    darray<VkCommandBuffer> m_vk_command_buffers;   

    image_pool m_image_pool{};

    texture_pool m_texture_pool{};

    depthbuffer_data m_depthbuffer{};
    
    uniform_block_data<transform_data> m_vertex_uniform_block{};

    static inline constexpr vec3_t k_room_cube_center = R3v(0, 0, 0);
    static inline constexpr vec3_t k_room_cube_size = R3(20);
    static inline constexpr vec3_t k_mirror_cube_center = R3v(0, 0, 0);
    static inline constexpr vec3_t k_mirror_cube_size = R3(1);
    
    static inline constexpr vec3_t k_color_green = R3v(0, 1, 0);
    static inline constexpr vec3_t k_color_red = R3v(1, 0, 0);
    static inline constexpr vec3_t k_color_blue = R3v(0, 0, 1);

    // NOTE:
    // right handed system,
    // so positive rotation about a given axis
    // is counter-clockwise
    vertex_list_t m_vertex_buffer_vertices =
      model_triangle(R3v(-2.25, 0, 0)) +
      model_triangle(R3v(2.25, 0, 1), R3v(0, 0.5, 0.8)) +
      model_cube(k_room_cube_center, R3(1), k_room_cube_size) +
      model_cube(k_mirror_cube_center, R3(1), k_mirror_cube_size);
      
    VkCommandPool m_vk_command_pool{VK_NULL_HANDLE};

    VkDescriptorPool m_vk_descriptor_pool{VK_NULL_HANDLE};
    
    VkPipelineLayout m_vk_pipeline_layout{VK_NULL_HANDLE};

    VkRenderPass m_vk_render_pass{VK_NULL_HANDLE};

    VkPipeline m_vk_graphics_pipeline;

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

    texture_pool::index_type m_test_texture_index{texture_pool::k_unset};
    
    bool m_ok_present{false};
    bool m_ok_descriptor_pool{false};
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
		   k_color_red * color,
		   scale,
		   {
		    {
		     R3v(0, 1, 0),
		     glm::half_pi<real_t>()
		    }
		   }) +

	// right face
	model_quad(translate + R3v(1, 0, 0),
		   k_color_green * color,
		   scale,
		   {
		    {
		     R3v(0, 1, 0),
		     glm::half_pi<real_t>()
		    }
		   }) +

	// up face
	model_quad(translate + R3v(0, 1, 0),
		   k_color_blue * color,
		   scale,
		   {
		    {
		     R3v(1, 0, 0),
		     -glm::half_pi<real_t>()
		    }
		   }) +

	// down face
	model_quad(translate + R3v(0, -1, 0),
		   k_color_red * color,
		   scale,
		   {
		    {
		     R3v(1, 0, 0),
		     glm::half_pi<real_t>()
		    }
		   }) +
      
	// front face
	model_quad(translate + R3v(0, 0, 1),
		   k_color_green * color,
		   scale,
		   {}) +

	// back face
	model_quad(translate + R3v(0, 0, -1),
		   k_color_blue * color,
		   scale,
		   {});
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

    VkShaderModule make_shader_module(darray<uint8_t> spv_code) const {
      VkShaderModule ret;

      if (ok_present()) {
	VkShaderModuleCreateInfo create_info = {};

	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = spv_code.size();
	create_info.pCode = reinterpret_cast<uint32_t*>(spv_code.data());
      
	VK_FN(vkCreateShaderModule(m_vk_curr_ldevice, &create_info, nullptr, &ret));
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
	   { texture2d_data::k_descriptor_type, 1 },
	   { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
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

    void setup_uniform_block_data() {
      if (ok_descriptor_pool()) {
	m_uniform_block_pool.reset(new uniform_block_pool{make_device_resource_properties()});
	
	m_vertex_uniform_block.index =
	  m_uniform_block_pool->add(&m_vertex_uniform_block.data,
				    sizeof(m_vertex_uniform_block.data),
				    1,
				    0,
				    VK_SHADER_STAGE_VERTEX_BIT);

	m_vertex_uniform_block.pool = m_uniform_block_pool.get();
	
	m_ok_uniform_block_data = m_vertex_uniform_block.ok();	
      }
    }
    
    void setup_texture_data() {
      if (ok_uniform_block_data()) {	       
	// generate image data
	uint32_t image_w = 256;
	uint32_t image_h = 256;


	uint32_t bpp = 4;

	darray<uint8_t> buffer(image_w * image_h * bpp, 0);

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

	texture_gen_params texture_params =
	  {
	   // image_params
	   {
	    // data
	    static_cast<void*>(buffer.data()), 
	    // memory property flags
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	    // format
	    VK_FORMAT_R8G8B8A8_UNORM,
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
	    // width
	    image_w,
	    // height
	    image_h,
	    // depth
	    1
	   },
	   // end image params

	   
	   // descriptor array element
	   0,
	   // binding index
	   0,
	   // descriptor type
	   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	   // shader stage flags
	   VK_SHADER_STAGE_FRAGMENT_BIT
	  };

	m_texture_pool.set_image_pool(&m_image_pool);

	m_test_texture_index = m_texture_pool.make_texture(make_device_resource_properties(),
							   texture_params);       
	
	m_ok_texture_data = m_texture_pool.ok_texture(m_test_texture_index);
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

    void setup_graphics_pipeline() {
      if (ok_depthbuffer_data()) {
	//
	// create shader programs
	// 
	auto spv_vshader = read_file("resources/shaders/tri_ubo/tri_ubo.vert.spv");
	auto spv_fshader = read_file("resources/shaders/tri_ubo/tri_ubo.frag.spv");

	ASSERT(!spv_vshader.empty());
	ASSERT(!spv_fshader.empty());
	
	VkShaderModule vshader_module = make_shader_module(spv_vshader);
	VkShaderModule fshader_module = make_shader_module(spv_fshader);

	VkPipelineShaderStageCreateInfo vshader_create = {};
	vshader_create.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vshader_create.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vshader_create.module = vshader_module;
	vshader_create.pName = "main";

	VkPipelineShaderStageCreateInfo fshader_create = {};
	fshader_create.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fshader_create.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fshader_create.module = fshader_module;
	fshader_create.pName = "main";

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages =
	  {
	   vshader_create,
	   fshader_create
	  };

	auto vertex_input_state = default_vertex_input_state_settings();

	VkVertexInputAttributeDescription iad_position = {};
        iad_position.location = 0;
	iad_position.binding = 0;
	iad_position.format = VK_FORMAT_R32G32B32_SFLOAT;
	iad_position.offset = offsetof(vertex_data, position);

	VkVertexInputAttributeDescription iad_texture = {};
	iad_texture.location = 1;
	iad_texture.binding = 0;
	iad_texture.format = VK_FORMAT_R32G32_SFLOAT;
	iad_texture.offset = offsetof(vertex_data, st);

	VkVertexInputAttributeDescription iad_color = {};
	iad_color.location = 2;
	iad_color.binding = 0;
	iad_color.format = VK_FORMAT_R32G32B32_SFLOAT;
	iad_color.offset = offsetof(vertex_data, color);

        darray<VkVertexInputAttributeDescription> input_attrs =
	  {
	   iad_position,
	   iad_texture,
	   iad_color
	  };

	vertex_input_state.vertexAttributeDescriptionCount = input_attrs.size();
	vertex_input_state.pVertexAttributeDescriptions = input_attrs.data();
	
	VkVertexInputBindingDescription ibd = {};
	ibd.binding = 0;
	ibd.stride = sizeof(vertex_data);
	ibd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	vertex_input_state.vertexBindingDescriptionCount = 1;
	vertex_input_state.pVertexBindingDescriptions = &ibd;
		
	auto input_assembly_state = default_input_assembly_state_settings();
	
	auto viewport = make_viewport(R2(0), m_vk_swapchain_extent, 0.0f, 1.0f);
	
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = m_vk_swapchain_extent;

	auto viewport_state = default_viewport_state_settings();
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	auto rasterization_state = default_rasterization_state_settings();
	auto multisample_state = default_multisample_state_settings();
	
	auto color_blend_attach_state = default_color_blend_attach_state_settings();
	auto color_blend_state = default_color_blend_state_settings();
	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments = &color_blend_attach_state;

	std::array<VkDescriptorSetLayout, 2> set_layouts =
	  {
	   m_texture_pool.descriptor_set_layout(m_test_texture_index),
	   m_uniform_block_pool->descriptor_set_layout(m_vertex_uniform_block.index)
	  };
	
	auto pipeline_layout = default_pipeline_layout_settings();
	pipeline_layout.setLayoutCount = set_layouts.size();
	pipeline_layout.pSetLayouts = set_layouts.data();
	
	VK_FN(vkCreatePipelineLayout(m_vk_curr_ldevice,
				     &pipeline_layout,
				     nullptr,
				     &m_vk_pipeline_layout));

	auto colorbuffer = default_colorbuffer_settings(m_vk_khr_swapchain_format.format);
	auto colorbuffer_ref = default_colorbuffer_ref_settings();

	auto depthbuffer = default_depthbuffer_settings();
	auto depthbuffer_ref = default_depthbuffer_ref_settings();
	
	VkSubpassDescription color_subpass = {};
	color_subpass.inputAttachmentCount = 0;
	color_subpass.pInputAttachments = nullptr;
	color_subpass.pResolveAttachments = nullptr;
	color_subpass.preserveAttachmentCount = 0;
	color_subpass.pPreserveAttachments = nullptr;
	color_subpass.flags = 0;
	color_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	color_subpass.colorAttachmentCount = 1;
	color_subpass.pColorAttachments = &colorbuffer_ref;
	color_subpass.pDepthStencilAttachment = &depthbuffer_ref;

	std::array<VkSubpassDescription, 1> subpasses =
	  {
	   color_subpass
	  };
	
	constexpr uint32_t k_depth_subpass_index = 0;
	constexpr uint32_t k_color_subpass_index = 0;

	VkSubpassDependency depth_dependency = {};
	depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	depth_dependency.dstSubpass = k_depth_subpass_index;

	depth_dependency.srcStageMask =
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	depth_dependency.srcAccessMask = 0;

	depth_dependency.dstStageMask =
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	
	depth_dependency.dstAccessMask = depthbuffer_data::k_access_flags;
	  	
	VkSubpassDependency color_dependency = {};
	color_dependency.srcSubpass = k_depth_subpass_index;
	color_dependency.dstSubpass = k_color_subpass_index;

	color_dependency.srcStageMask =
	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	color_dependency.srcAccessMask = 0;

	color_dependency.dstStageMask =
	  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	color_dependency.dstAccessMask =
	  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency self_dependency = {};	
	self_dependency.srcSubpass = k_color_subpass_index;
	self_dependency.dstSubpass = k_color_subpass_index;
	
	self_dependency.srcStageMask =
	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	self_dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	
	self_dependency.dstStageMask =
	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	self_dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	
	std::array<VkSubpassDependency, 3> dependencies =
	  {
	   depth_dependency,
	   color_dependency,
	   self_dependency
	  };

	std::array<VkAttachmentDescription, 2> attachments =
	  {
	   colorbuffer,
	   depthbuffer	   
	  };
	
	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.flags = 0;
	render_pass_info.attachmentCount = attachments.size();
	render_pass_info.pAttachments = attachments.data();

	render_pass_info.subpassCount = subpasses.size();
	render_pass_info.pSubpasses = subpasses.data();
	
	render_pass_info.dependencyCount = dependencies.size();
	render_pass_info.pDependencies = dependencies.data();

	VK_FN(vkCreateRenderPass(m_vk_curr_ldevice,
				 &render_pass_info,
				 nullptr,
				 &m_vk_render_pass));

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = nullptr;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = VK_TRUE;
	depth_stencil_state.depthWriteEnable = VK_TRUE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = VK_FALSE;
	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;
	depth_stencil_state.front = default_stencilop_state();
	depth_stencil_state.back = default_stencilop_state();
	
	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages.data();
	pipeline_info.pVertexInputState = &vertex_input_state;
	pipeline_info.pInputAssemblyState = &input_assembly_state;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterization_state;
	pipeline_info.pMultisampleState = &multisample_state;
	pipeline_info.pDepthStencilState = &depth_stencil_state;
	pipeline_info.pColorBlendState = &color_blend_state;
	pipeline_info.pDynamicState = nullptr;
	pipeline_info.layout = m_vk_pipeline_layout;
	pipeline_info.renderPass = m_vk_render_pass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	
	VK_FN(vkCreateGraphicsPipelines(m_vk_curr_ldevice,
					VK_NULL_HANDLE,
					1,
					&pipeline_info,
					nullptr,
					&m_vk_graphics_pipeline));
	
	//
	// Free memory
	//
	
	free_vk_ldevice_handle<VkShaderModule, &vkDestroyShaderModule>(vshader_module);
	free_vk_ldevice_handle<VkShaderModule, &vkDestroyShaderModule>(fshader_module);

	m_ok_graphics_pipeline = true;
      }
    }

    void setup_framebuffers() {
      if (ok_vertex_buffer()) {
	m_vk_swapchain_framebuffers.resize(m_vk_swapchain_image_views.size());

	for (size_t i = 0; i < m_vk_swapchain_image_views.size(); ++i) {
	  std::array<VkImageView, 2> attachments =
	    {
	     m_vk_swapchain_image_views[i],
	     m_depthbuffer.image_view
	    };

	  VkFramebufferCreateInfo framebuffer_info = {};
	  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	  framebuffer_info.pNext = nullptr;
	  framebuffer_info.flags = 0;
	  framebuffer_info.renderPass = m_vk_render_pass;
	  framebuffer_info.attachmentCount = attachments.size();
	  framebuffer_info.pAttachments = attachments.data();
	  framebuffer_info.width = m_vk_swapchain_extent.width;
	  framebuffer_info.height = m_vk_swapchain_extent.height;
	  framebuffer_info.layers = 1;

	  VK_FN(vkCreateFramebuffer(m_vk_curr_ldevice,
				    &framebuffer_info,
				    nullptr,
				    &m_vk_swapchain_framebuffers[i]));
	}

	m_ok_framebuffers = true;
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
	VkDescriptorImageInfo image_info =
	  m_texture_pool.make_descriptor_image_info(m_test_texture_index);
	VkWriteDescriptorSet write_desc_set =
	  m_texture_pool.make_write_descriptor_set(m_test_texture_index, &image_info);

	vkUpdateDescriptorSets(m_vk_curr_ldevice,
			       1,
			       &write_desc_set,
			       0,
			       nullptr);

	vkDeviceWaitIdle(m_vk_curr_ldevice);	
	
	run_cmds(
		 [this](VkCommandBuffer cmd_buf) {
		   puts("image_layout_transition");
		   
		   image_layout_transition().
		     from_stage(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT).
		     to_stage(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT).
		     for_aspect(VK_IMAGE_ASPECT_COLOR_BIT).
		     from_access(0).
		     to_access(VK_ACCESS_SHADER_READ_BIT).
		     from(texture2d_data::k_initial_layout).
		     to(texture2d_data::k_final_layout).
		     for_image(m_image_pool.image(m_texture_pool.image_index(m_test_texture_index))).
		     via(cmd_buf);

		   m_ok_scene = true;
		 },
		 [this]() {
		   puts("run_cmds ERROR");
		   ASSERT(false);
		   m_ok_scene = false;
		 });
	
	m_vk_command_buffers.resize(m_vk_swapchain_framebuffers.size());
	
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = m_vk_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = static_cast<uint32_t>(m_vk_command_buffers.size());

	VK_FN(vkAllocateCommandBuffers(m_vk_curr_ldevice, &alloc_info, m_vk_command_buffers.data()));

	std::array<VkDescriptorSet, 2> descriptor_sets =
	  {
	   m_texture_pool.descriptor_set(m_test_texture_index),
	   m_uniform_block_pool->descriptor_set(m_vertex_uniform_block.index)
	  };
	
	size_t i = 0;

	VkDeviceSize vertex_buffer_ofs = 0;
	
        while (i < m_vk_command_buffers.size() && ok()) {
	  VkCommandBufferBeginInfo begin_info = {};
	  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	  begin_info.flags = 0;
	  begin_info.pInheritanceInfo = nullptr;

	  VK_FN(vkBeginCommandBuffer(m_vk_command_buffers[i], &begin_info));	  
	  
	  if (ok()) {
	    vkCmdBindPipeline(m_vk_command_buffers[i],
			      VK_PIPELINE_BIND_POINT_GRAPHICS,
			      m_vk_graphics_pipeline);
	    
	    vkCmdBindVertexBuffers(m_vk_command_buffers[i],
				   0,
				   1,
				   &m_vk_vertex_buffer,
				   &vertex_buffer_ofs);

	    vkCmdUpdateBuffer(m_vk_command_buffers[i],
			      m_vk_vertex_buffer,
			      0,
			      sizeof(m_vertex_buffer_vertices[0]) * m_vertex_buffer_vertices.size(),
			      m_vertex_buffer_vertices.data());

	    vkCmdBindDescriptorSets(m_vk_command_buffers[i],
				    VK_PIPELINE_BIND_POINT_GRAPHICS,
				    m_vk_pipeline_layout,
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
	      to(depthbuffer_data::k_final_layout).
	      for_image(m_depthbuffer.image).
	      via(m_vk_command_buffers[i]);
	    
	    VkRenderPassBeginInfo render_pass_info = {};
	    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	    render_pass_info.renderPass = m_vk_render_pass;
	    render_pass_info.framebuffer = m_vk_swapchain_framebuffers[i];

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

	    {	    
	      vkCmdBeginRenderPass(m_vk_command_buffers[i],
				   &render_pass_info,
				   VK_SUBPASS_CONTENTS_INLINE);

	      vkCmdDraw(m_vk_command_buffers[i],
			m_instance_count * 3,
			m_instance_count,
			0,
			0);

	      vkCmdEndRenderPass(m_vk_command_buffers[i]);
	    }

	    VK_FN(vkEndCommandBuffer(m_vk_command_buffers[i]));
	  }

	  i++;
	}

	if (ok()) {
	  m_ok_command_buffers = true;
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
      m_vertex_uniform_block.data.world_to_view = w2v;
    }

    void set_view_to_clip_transform(const mat4_t& v2c) {
      m_vertex_uniform_block.data.view_to_clip = v2c;
    }

    void render() {
      if (ok_scene()) {
	m_uniform_block_pool->update_block(m_vertex_uniform_block.index);
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

      free_vk_ldevice_handle<VkDeviceMemory, &vkFreeMemory>(m_vk_vertex_buffer_mem);
      
      free_vk_ldevice_handle<VkBuffer, &vkDestroyBuffer>(m_vk_vertex_buffer);
      
      free_vk_ldevice_handle<VkSemaphore, &vkDestroySemaphore>(m_vk_sem_image_available);
      free_vk_ldevice_handle<VkSemaphore, &vkDestroySemaphore>(m_vk_sem_render_finished);
      
      free_vk_ldevice_handle<VkCommandPool, &vkDestroyCommandPool>(m_vk_command_pool);
      
      free_vk_ldevice_handles<VkFramebuffer, &vkDestroyFramebuffer>(m_vk_swapchain_framebuffers);
      
      free_vk_ldevice_handle<VkPipeline, &vkDestroyPipeline>(m_vk_graphics_pipeline);
      free_vk_ldevice_handle<VkPipelineLayout, &vkDestroyPipelineLayout>(m_vk_pipeline_layout);
      free_vk_ldevice_handle<VkRenderPass, &vkDestroyRenderPass>(m_vk_render_pass);

      m_texture_pool.free_mem(m_vk_curr_ldevice);
      
      m_depthbuffer.free_mem(m_vk_curr_ldevice);

      m_uniform_block_pool->free_mem();
      
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
