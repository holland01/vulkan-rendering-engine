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
#include "vk_model.hpp"

#include <optional>
#include <set>
#include <initializer_list>
#include <unordered_map>
#include <iomanip>
#include <iostream>
#include <functional>
#include <variant>

#include <glm/gtc/matrix_transform.hpp>

#define STOP(x) std::cout << "made it" << std::endl; if (false) { x } 

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

  struct framebuffer_attachments {
    struct color_depth_pair {
      image_pool::index_type color_attachment;
      image_pool::index_type depth_attachment;
    };
    
    darray<color_depth_pair> data{};

    image_pool* p_image_pool{nullptr};
    
    VkImageView image_view(image_pool::index_type image) const {
      VkImageView img{VK_NULL_HANDLE};
      if (c_assert(p_image_pool != nullptr)) {
	img = p_image_pool->image_view(image);
      }
      return img;
    }
    
    VkImageView color_image_view(int index) const {
      return image_view(data.at(index).color_attachment);
    }

    VkImageView depth_image_view(int index) const {
      return image_view(data.at(index).depth_attachment);
    }
  };

  struct sampler_data {
    int sampler_index;
  };

  struct rot_cmd {
    vec3_t axes;
    real_t rad;
  };

  struct descriptors {
    darray<descriptor_set_pool::index_type> attachment_read;
  };

  namespace uniform_block {
    struct transform {
      mat4_t view_to_clip{R(1.0)};
      mat4_t world_to_view{R(1.0)};    
    };

    struct surface {
      vec3_t albedo;
      float metallic;
      float roughness;
      float ao; // ambient occlusion
    };

    static constexpr inline uint32_t k_binding_transform = 0;
    static constexpr inline uint32_t k_binding_surface = 1;

    struct series_gen {
      device_resource_properties properties;
      descriptor_set_pool::index_type series_index{descriptor_set_pool::k_unset};
      uniform_block_pool* pool{nullptr};
      

      template <class blockStructType>
      bool make(uniform_block_data<blockStructType>& block,
		uint32_t binding_index,
		uint32_t array_elem_index = 0) {
	bool ret{false};
	if (c_assert(properties.ok()) &&
	    c_assert(pool != nullptr) &&
	    c_assert(series_index != descriptor_set_pool::k_unset)) {
	  block.index = pool->make_uniform_block(properties,
						 {
						  // descriptor set index
						  series_index,
						  // block upload address
						  static_cast<void*>(&block.data),
						  // block upload size
						  sizeof(block.data),
						  //
						  array_elem_index,
						  //
						  binding_index
						 });

	  block.pool = pool;
	  
	  ret = block.ok();
	  
	}
	return ret;
      }

    };
    
  }
  
  namespace push_constant {
    // NOTE:
    // currently the basic_pbr and
    // model push constants
    // are used in the same program, just at different stages.
    // the model is used in the vertex stage, and the
    // standard surface in the fragment shage.
    // despite being used at different stages,
    // the push constants are viewed as one contiguous
    // buffer for a given pipeline layout;
    // thus, a byte offset for at least one of these
    // must be specified. For now, the model
    // offset is listed here at <k_model_offset> bytes.
    // there is a hole in the middle between the two,
    // but that's ok since basic_pbr will end
    // up having more parameters as we move forward.
    
    // autodesk - WIP
    struct basic_pbr {
      vec3_t camera_position;
      float padding0;
      vec3_t albedo;
      float padding1;
      float metallic;
      float roughness;
      float ao;
      int sampler;
    };

    struct model {
      mat4_t model_to_world{R(1.0)};
    };

    static inline constexpr uint32_t k_model_offset = 64;
    
    template <class T, VkShaderStageFlags flags>
    static inline VkPushConstantRange range(uint32_t offset = 0) {
      return
	{
	 flags,
	 offset,
	 sizeof(T)
	};
    }

    template <class T, VkShaderStageFlags flags>
    static inline void upload(T* ptr, VkCommandBuffer cmd_buffer, VkPipelineLayout layout, uint32_t offset=0) {
      vkCmdPushConstants(cmd_buffer,
			 layout,
			 flags,
			 offset,
			 sizeof(T),
			 ptr);
    }

    static inline VkPushConstantRange basic_pbr_range() {
      return range<basic_pbr,
		   VK_SHADER_STAGE_FRAGMENT_BIT>();
    }

    static inline VkPushConstantRange model_range() {
      return range<model,
		   VK_SHADER_STAGE_VERTEX_BIT>(k_model_offset);
    }

    static inline void basic_pbr_upload(basic_pbr& pc,
					       VkCommandBuffer cmd_buffer,
					       VkPipelineLayout layout) {
      upload<basic_pbr,
	     VK_SHADER_STAGE_FRAGMENT_BIT>(&pc,
					   cmd_buffer,
					   layout);
    }

    static inline void model_upload(model& pc,
				    VkCommandBuffer cmd_buffer,
				    VkPipelineLayout layout) {
      upload<model,
	     VK_SHADER_STAGE_VERTEX_BIT>(&pc,
					 cmd_buffer,
					 layout,
					 k_model_offset);
    }


    static inline basic_pbr basic_pbr_default() {
      return
	{
	 // camera position
	 R3(0),
	 // padding0
	 R(0.0),
	 // albedo
	 R3v(0.5, 0, 0),
	 // padding1
	 R(0.0),
	 // metallic
	 R(0.5),
	 // roughness
	 R(0.5),
	 // ambient occlusion
	 R(1),
	 // sampler index
	 0
	};
    }
  }
    
  class renderer {       
    darray<VkPhysicalDevice> m_vk_physical_devs;

    darray<VkImage> m_vk_swapchain_images;
    
    darray<VkImageView> m_vk_swapchain_image_views;

    darray<VkFramebuffer> m_vk_swapchain_framebuffers;

    darray<VkImage> m_vk_firstpass_images;

    darray<VkCommandBuffer> m_vk_command_buffers;   

    framebuffer_attachments m_framebuffer_attachments;

    descriptors m_descriptors;
    
    descriptor_set_pool m_descriptor_set_pool{};
    
    image_pool m_image_pool{};

    texture_pool m_texture_pool{};

    uniform_block_pool m_uniform_block_pool{};

    pipeline_layout_pool m_pipeline_layout_pool{};

    pipeline_pool m_pipeline_pool{};
    
    depthbuffer_data m_depthbuffer{};
    
    uniform_block_data<uniform_block::transform> m_transform_uniform_block{};
    uniform_block_data<uniform_block::surface> m_surface_uniform_block{};
    
    static inline constexpr vec3_t k_room_cube_center = R3v(0, 0, 0);
    static inline constexpr vec3_t k_room_cube_size = R3(20);
    static inline constexpr vec3_t k_mirror_cube_center = R3v(0, -10, 0);
    static inline constexpr vec3_t k_mirror_cube_size = R3(1);
    
    static inline constexpr vec3_t k_color_green = R3v(0, 1, 0);
    static inline constexpr vec3_t k_color_red = R3v(1, 0, 0);
    static inline constexpr vec3_t k_color_blue = R3v(0, 0, 1);

    static inline constexpr int32_t k_sampler_checkerboard = 0;
    static inline constexpr int32_t k_sampler_aqua = 1;       
    
    struct {
      darray<transform> transforms{};
      darray<uint32_t> vb_offsets{};
      darray<uint32_t> vb_lengths{};

      std::unordered_map<std::string, uint32_t> indices{};

      size_t length() const {
	return transforms.size();
      }
      
    } m_model_data{};
    
    vertex_list_t m_vertex_buffer_vertices{};

    VkCommandPool m_vk_command_pool{VK_NULL_HANDLE};

    VkDescriptorPool m_vk_descriptor_pool{VK_NULL_HANDLE};   

    VkSurfaceFormatKHR m_vk_khr_swapchain_format;

    VkExtent2D m_vk_swapchain_extent;

    VkInstance m_vk_instance {VK_NULL_HANDLE};

    VkPhysicalDevice m_vk_curr_pdevice{VK_NULL_HANDLE};

    VkRenderPass m_vk_render_pass{VK_NULL_HANDLE};
    
    VkDevice m_vk_curr_ldevice{VK_NULL_HANDLE};

    VkQueue m_vk_graphics_queue{VK_NULL_HANDLE};
    VkQueue m_vk_present_queue{VK_NULL_HANDLE};

    VkSurfaceKHR m_vk_khr_surface{VK_NULL_HANDLE};
    VkSwapchainKHR m_vk_khr_swapchain{VK_NULL_HANDLE};

    VkSemaphore m_vk_sem_image_available;
    VkSemaphore m_vk_sem_render_finished;

    VkBuffer m_vk_vertex_buffer{VK_NULL_HANDLE};
    VkDeviceMemory m_vk_vertex_buffer_mem{VK_NULL_HANDLE};

    darray<image_pool::index_type> m_test_image_indices =
      {
       image_pool::k_unset,
       image_pool::k_unset
      };
    
    darray<texture_pool::index_type> m_test_texture_indices =
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
    static constexpr inline int k_descriptor_set_input_attachment = 3;
    
    darray<descriptor_set_pool::index_type> m_test_descriptor_set_indices =
      {
       descriptor_set_pool::k_unset,  // pipeline 0
       descriptor_set_pool::k_unset,  // pipeline 0, pipeline 1
       descriptor_set_pool::k_unset,  // pipeline 1
       descriptor_set_pool::k_unset
      };   
    
    static constexpr inline int k_pass_texture2d = 0;
    static constexpr inline int k_pass_test_fbo = 1; // test FBO pass   

    darray<pipeline_layout_pool::index_type> m_pipeline_layout_indices =
      {
       pipeline_layout_pool::k_unset,
       pipeline_layout_pool::k_unset,
       pipeline_layout_pool::k_unset
      };

    darray<pipeline_pool::index_type> m_pipeline_indices =
      {
       pipeline_pool::k_unset,
       pipeline_pool::k_unset,
       pipeline_pool::k_unset
      };

    vec3_t m_camera_position{R(0)};

    uint32_t m_instance_count{0};   
    
    bool m_ok_present{false};
    bool m_ok_vertex_data{false};
    bool m_ok_descriptor_pool{false};
    bool m_ok_render_pass{false};
    bool m_ok_attachment_read_descriptors{false};
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
    
    VkPipelineLayout pipeline_layout(int index) const {
      return
	m_pipeline_layout_pool.pipeline_layout(m_pipeline_layout_indices.at(index));
    }

    VkPipeline pipeline(int index) const {
      return
	m_pipeline_pool.pipeline(m_pipeline_indices.at(index));
    }

    VkDescriptorSet descriptor_set(int index) const {
      return
	m_descriptor_set_pool
	.descriptor_set(m_test_descriptor_set_indices.at(index));
    }

    darray<VkDescriptorSet> descriptor_sets(const darray<descriptor_set_pool::index_type>& indices) const {
      return c_fmap<descriptor_set_pool::index_type,
		    VkDescriptorSet>(indices,
				     [this](const descriptor_set_pool::index_type& i)
				     {
				       return descriptor_set(i);
				     });
    }

    VkDescriptorSetLayout descriptor_set_layout(int index) const {
      return
	m_descriptor_set_pool
	.descriptor_set_layout(m_test_descriptor_set_indices.at(index));
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

      buffer_info.usage =
	VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
	VK_BUFFER_USAGE_TRANSFER_DST_BIT;

      buffer_info.size = size;
      
      buffer_info.sharingMode = query_queue_sharing_mode();

      uint32_t num_indices = 0;
      
      buffer_info.pQueueFamilyIndices = query_graphics_buffer_indices(&num_indices);
      buffer_info.queueFamilyIndexCount = num_indices;

      return buffer_info;
    }

    // Create a frame buffer attachment
    image_pool::index_type make_framebuffer_attachment(VkFormat format, VkImageUsageFlags usage)
    {
      image_pool::index_type ret{image_pool::k_unset};

      VkImageAspectFlags aspect_mask = 0;
      VkImageLayout image_layout;

      if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
	aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
      
      if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
	aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
	image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      }

      image_gen_params gen_image {};

      gen_image.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      gen_image.format = format;
      gen_image.attachment_layout = image_layout;
      gen_image.final_layout = image_layout;
      gen_image.width = m_vk_swapchain_extent.width;
      gen_image.height = m_vk_swapchain_extent.height;
      gen_image.depth = 1;
      gen_image.aspect_flags = aspect_mask;
      gen_image.usage_flags = usage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

      ret = m_image_pool.make_image(make_device_resource_properties(),
				    gen_image);

      ASSERT(ret != image_pool::k_unset);

      return ret;
      
      /*
      VkImageAspectFlags aspectMask = 0;
      VkImageLayout imageLayout;

      if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
	aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
      if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
	aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      }

      VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
      imageCI.imageType = VK_IMAGE_TYPE_2D;
      imageCI.format = format;
      imageCI.extent.width = width;
      imageCI.extent.height = height;
      imageCI.extent.depth = 1;
      imageCI.mipLevels = 1;
      imageCI.arrayLayers = 1;
      imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
      imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
      // VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT flag is required for input attachments;
      imageCI.usage = usage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
      imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &attachment->image));

      VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
      VkMemoryRequirements memReqs;
      vkGetImageMemoryRequirements(device, attachment->image, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &attachment->memory));
      VK_CHECK_RESULT(vkBindImageMemory(device, attachment->image, attachment->memory, 0));

      VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
      imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
      imageViewCI.format = format;
      imageViewCI.subresourceRange = {};
      imageViewCI.subresourceRange.aspectMask = aspectMask;
      imageViewCI.subresourceRange.baseMipLevel = 0;
      imageViewCI.subresourceRange.levelCount = 1;
      imageViewCI.subresourceRange.baseArrayLayer = 0;
      imageViewCI.subresourceRange.layerCount = 1;
      imageViewCI.image = attachment->image;
      VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &attachment->view));
      */
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

    bool ok_vertex_data() const {
      return
	ok() &&
	c_assert(m_ok_vertex_data);
    }
    
    bool ok_descriptor_pool() const {
      bool r = ok() && m_ok_descriptor_pool;
      ASSERT(r);
      return r;
    }
    
    bool ok_render_pass() const {
      bool r = ok() && m_ok_render_pass;
      ASSERT(r);
      return r;
    }

    bool ok_attachment_read_descriptors() const {
      bool r = ok() && m_ok_attachment_read_descriptors;
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

    void setup_vertex_data() {
      if (ok_present()) {
	auto add_verts =
	  [this](const std::string& name,
		 mesh_builder& mb) {
	    
	    m_model_data.indices[name] = m_model_data.length();
	    
	    m_model_data.vb_offsets.push_back(m_vertex_buffer_vertices.size());
	    m_model_data.vb_lengths.push_back(mb.vertices.size());
	    m_model_data.transforms.push_back(mb.taccum);
	    
	    m_instance_count += mb.vertices.size() / 3;
	    
	    m_vertex_buffer_vertices =
	      m_vertex_buffer_vertices + mb.vertices;

	    // erase previous state,
	    // so we can add a new model
	    mb.reset();
	  };

	{
	  mesh_builder mb{};

	  // generate left triangle
	  mb
	    .set_transform(transform()
			   .translate(R3v(-2.25, 0, 0)))
	    .triangle();
	  
	  add_verts("left-triangle", mb);

	  // generate right triangle
	  mb
	    .set_transform(transform()
			   .translate(R3v(2.25, 0, 1)))
	    .set_color(R3v(0, 0.5, 0.8))
	    .triangle();
	  
	  add_verts("right-triangle", mb);

	  // generate inner cube
	  mb
	    .set_transform(transform()
			   .translate(k_mirror_cube_center))
	    .cube()
	    .with_scale(k_mirror_cube_size);
	  
	  add_verts("inner-cube", mb);	       


	  // generate sphere
	  mb
	    .set_color(R3v(0, 0.5, 0.8))
	    .set_transform(transform()
			   .scale(R3v(5, 5, 5))
			   .translate(R3v(0, 5, 0)))
	    .sphere();


	  add_verts("sphere", mb);
	  
	  // generate outer cube
	  mb
	    .set_transform(transform()
			   .translate(k_room_cube_center))
	    .cube()
	    .with_scale(k_room_cube_size);

	  add_verts("outer-cube", mb);
	}
	
	m_ok_vertex_data = true;
      }
    }
    
    void setup_descriptor_pool() {
      if (ok_vertex_data()) {       
        darray<VkDescriptorPoolSize> pool_sizes =
	  {
	   // type, descriptorCount
	   { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
	   { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
	   { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2 }
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
    
    void setup_render_pass() {
      if (ok_descriptor_pool()) {
	ASSERT(!m_vk_swapchain_images.empty());
	
	const VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
	const VkFormat depth_format = depthbuffer_data::query_format();
	
	m_framebuffer_attachments.data.resize(m_vk_swapchain_images.size());
	
	for (size_t i{0}; i < m_framebuffer_attachments.data.size(); i++) {

	  m_framebuffer_attachments.data[i].color_attachment =
	    make_framebuffer_attachment(color_format,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	  m_framebuffer_attachments.data[i].depth_attachment =
	    make_framebuffer_attachment(depth_format,
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	  
	  
	}

	std::array<VkAttachmentDescription, 3> attachments{};

	// Swap chain image color attachment
	// Will be transitioned to present layout
	attachments[0].format = m_vk_khr_swapchain_format.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Input attachments
	// These will be written in the first subpass, transitioned to input attachments 
	// and then read in the secod subpass

	// Color
	attachments[1].format = color_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	// Depth
	attachments[2].format = depth_format;
	attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkSubpassDescription,2> subpass_descriptions{};

	/*
	  First subpass
	  Fill the color and depth attachments
	*/
	VkAttachmentReference colorReference = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthReference = { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	subpass_descriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_descriptions[0].colorAttachmentCount = 1;
	subpass_descriptions[0].pColorAttachments = &colorReference;
	subpass_descriptions[0].pDepthStencilAttachment = &depthReference;

	/*
	  Second subpass
	  Input attachment read and swap chain color attachment write
	*/

	// Color reference (target) for this sub pass is the swap chain color attachment
	VkAttachmentReference color_reference_swapchain = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	subpass_descriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_descriptions[1].colorAttachmentCount = 1;
	subpass_descriptions[1].pColorAttachments = &color_reference_swapchain;

	// Color and depth attachment written to in first sub pass will
	// be used as input attachments to be read in the fragment shader
	VkAttachmentReference input_references[2];
	input_references[0] = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	input_references[1] = { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		
	// Use the attachments filled in the first pass as input attachments
	subpass_descriptions[1].inputAttachmentCount = 2;
	subpass_descriptions[1].pInputAttachments = input_references;

	/*
	  Subpass dependencies for layout transitions
	*/
	std::array<VkSubpassDependency, 3> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// This dependency transitions the input attachment from color attachment to shader read
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass = 0;
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo render_pass_info_ci{};
	render_pass_info_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info_ci.attachmentCount = static_cast<uint32_t>(attachments.size());
	render_pass_info_ci.pAttachments = attachments.data();
	render_pass_info_ci.subpassCount = static_cast<uint32_t>(subpass_descriptions.size());
	render_pass_info_ci.pSubpasses = subpass_descriptions.data();
	render_pass_info_ci.dependencyCount = static_cast<uint32_t>(dependencies.size());
	render_pass_info_ci.pDependencies = dependencies.data();
	VK_FN(vkCreateRenderPass(m_vk_curr_ldevice, &render_pass_info_ci, nullptr, &m_vk_render_pass));
	
	if (H_OK(m_vk_render_pass)) {
	  m_framebuffer_attachments.p_image_pool = &m_image_pool;
	  m_ok_render_pass = true;
	}
      }
    }

    void setup_attachment_read_descriptors() {
      if (ok_render_pass()) {

	bool good = true;

	{
	  descriptor_set_gen_params attachment_read_params =
	    {
	     // stages
	     {
	      // binding 0: color attachment input
	      VK_SHADER_STAGE_FRAGMENT_BIT,
	      // binding 1: depth attachment input
	      VK_SHADER_STAGE_FRAGMENT_BIT
	     },
	     // descriptor counts
	     {
	      // binding 0: color attachment input
	      1,
	      // binding 1: depth attachment input
	      1
	     },
	     // type for this set
	     VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
	    };

	  m_descriptors
	    .attachment_read
	    .resize(m_framebuffer_attachments.data.size());
	  
	  size_t i{0};
	  while (i < m_framebuffer_attachments.data.size() && good) {
	    
	    m_descriptors.attachment_read[i] =
	      m_descriptor_set_pool
	      .make_descriptor_set(make_device_resource_properties(),
				   attachment_read_params);

	    good = m_descriptor_set_pool.ok_descriptor_set(m_descriptors.attachment_read.at(i));

	    if (c_assert(good)) {
	    
	      VkImageView color = m_framebuffer_attachments.color_image_view(i);	      	       
	      VkImageView depth = m_framebuffer_attachments.depth_image_view(i);
	    
	      darray<VkDescriptorImageInfo> descriptor_image_infos =
		{
		 // sampler,          imageView,  imageLayout
		 { VK_NULL_HANDLE,    color,      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		 { VK_NULL_HANDLE,    depth,      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		};

	      VkDescriptorSet descset = m_descriptor_set_pool.descriptor_set(m_descriptors.attachment_read.at(i));
	    
	      darray<VkWriteDescriptorSet> write_descriptor_sets =
		{
		 // color input attachment
		 make_write_descriptor_set(descset,
					   // corresponding image info
					   descriptor_image_infos.data(),
					   // binding
					   0,
					   // type
					   VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
					   // descriptor count
					   1),
		 // depth input attachment
		 make_write_descriptor_set(descset,
					   // corresponding image info
					   descriptor_image_infos.data() + 1,
					   // binding
					   1,
					   // type
					   VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
					   // descriptor count
					   1)
		};
	    
	      vkUpdateDescriptorSets(m_vk_curr_ldevice,
				     static_cast<uint32_t>(write_descriptor_sets.size()),
				     write_descriptor_sets.data(),
				     0,
				     nullptr);
	    }

	    i++;
	  }

	  m_ok_attachment_read_descriptors = good;
	}	
      }
    }
    
    void setup_uniform_block_data() {
      if (ok_attachment_read_descriptors()) {
	// setup descriptor set for all uniform blocks
	{
	  descriptor_set_gen_params uniform_block_desc_set_params =
	    {
	     {
	      VK_SHADER_STAGE_VERTEX_BIT, // binding 0: transform block
	      VK_SHADER_STAGE_VERTEX_BIT // binding 1: surface block
	     },
	     // descriptor counts for each binding
	     {
	      1, // binding 0
	      1 // binding 1
	     },
	     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	    };

	  m_test_descriptor_set_indices[k_descriptor_set_uniform_blocks] =
	    m_descriptor_set_pool.make_descriptor_set(make_device_resource_properties(),
						      uniform_block_desc_set_params);
	}

	// the pool will forward descriptor set info when
	// creating a uniform block
	m_uniform_block_pool.set_descriptor_set_pool(&m_descriptor_set_pool);

	//
	// generate the uniform blocks.
	//
	// m_uniform_block_pool will be assigned to each
	// created block data instance
	//
	uniform_block::series_gen gen
	  {
	   make_device_resource_properties(),
	   m_test_descriptor_set_indices.at(k_descriptor_set_uniform_blocks),
	   &m_uniform_block_pool
	  };

	m_ok_uniform_block_data =
	  gen.make<uniform_block::transform>(m_transform_uniform_block, uniform_block::k_binding_transform) &&
	  gen.make<uniform_block::surface>(m_surface_uniform_block, uniform_block::k_binding_surface);
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
	     // stages
	     {
	      VK_SHADER_STAGE_FRAGMENT_BIT
	     },
	     // descriptor_counts
	     {
	      2
	     },
	     // type
	     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	    };

	  m_test_descriptor_set_indices[k_descriptor_set_samplers] =
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
			int subpass_index,
			pipeline_layout_gen_params layout_params,
			pipeline_gen_params params) {
      bool success = false;
      
      m_pipeline_layout_indices[render_phase_index] =
	m_pipeline_layout_pool.make_pipeline_layout(make_device_resource_properties(),
						    layout_params);

      params.subpass_index = subpass_index;
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
      return setup_pipeline(k_pass_texture2d,
			    // subpass index
			    0, 
			    // pipeline layout
			    {
			     // descriptor set layouts
			     {
			      descriptor_set_layout(k_descriptor_set_samplers),			      
			      descriptor_set_layout(k_descriptor_set_uniform_blocks)
			     },
			     // push constant ranges
			     {
			      push_constant::basic_pbr_range(),
			      push_constant::model_range()
			     }
			    },
			    // pipeline
			    {
			     // render pass
			     m_vk_render_pass,
			     // viewport extent
			     m_vk_swapchain_extent,	   
			     // vert spv path
			     realpath_spv("tri_ubo.vert.spv"),
			     // frag spv path
			     realpath_spv("tri_ubo.frag.spv")

			    });
    }

    bool setup_pipeline_test_fbo() {
      return setup_pipeline(k_pass_test_fbo,
			    // subpass index
			    1,
			    // pipeline layout
			    {
			     m_descriptor_set_pool.descriptor_set_layouts(m_descriptors.attachment_read),
			     // push constant ranges
			     {
			     }
			    },
			    // pipeline
			    {
			     // render pass
			     m_vk_render_pass,
			     // viewport extent
			     m_vk_swapchain_extent,	   
			     // vert spv path
			     realpath_spv("attachment_read.vert.spv"),
			     // frag spv path
			     realpath_spv("attachment_read.frag.spv")			     
			    });
    }
    
    void setup_graphics_pipeline() {      
      if (ok_depthbuffer_data()) {
	m_pipeline_pool.set_pipeline_layout_pool(&m_pipeline_layout_pool);

	m_ok_graphics_pipeline =
	  setup_pipeline_texture2d() &&
	  setup_pipeline_test_fbo();       
      }
    }

    darray<VkFramebuffer> make_framebuffer_list(VkRenderPass fb_render_pass,
						const darray<VkImageView>& color_image_views) {
      darray<VkFramebuffer> ret(color_image_views.size(), VK_NULL_HANDLE);

      size_t i{0};
      bool good{true};
      
      while (i < ret.size() && good) {
	std::array<VkImageView, 3> attachments =
	  {
	   color_image_views.at(i),
	   m_framebuffer_attachments.color_image_view(i),
	   m_framebuffer_attachments.depth_image_view(i)
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
	good = H_OK(ret.at(i));
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
	  make_framebuffer_list(m_vk_render_pass,
				m_vk_swapchain_image_views);	

	m_ok_framebuffers = !m_vk_swapchain_framebuffers.empty(); 
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
				 bool with_vertex_buffer,
				 const darray<VkDescriptorSet>& descriptor_sets) {

      VkDeviceSize vertex_buffer_ofs = 0;
      
      vkCmdBindPipeline(cmd_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline);

      if (with_vertex_buffer) {
	vkCmdBindVertexBuffers(cmd_buffer,
			       0,
			       1,
			       &m_vk_vertex_buffer,
			       &vertex_buffer_ofs);
      }

      vkCmdBindDescriptorSets(cmd_buffer,
			      VK_PIPELINE_BIND_POINT_GRAPHICS,
			      pipeline_layout,
			      0,
			      descriptor_sets.size(),
			      descriptor_sets.data(),
			      0,
			      nullptr);
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

      std::array<VkClearValue, 3> clear_values;
	    
      clear_values[0].color.float32[0] = 1.0f;
      clear_values[0].color.float32[1] = 0.0f;
      clear_values[0].color.float32[2] = 0.0f;
      clear_values[0].color.float32[3] = 1.0f;

      clear_values[1].color.float32[0] = 1.0f;
      clear_values[1].color.float32[1] = 0.0f;
      clear_values[1].color.float32[2] = 0.0f;
      clear_values[1].color.float32[3] = 1.0f;
      
      clear_values[2].depthStencil.depth = 1.0f;
      clear_values[2].depthStencil.stencil = 0;
	    
      // for the color attachment's VK_ATTACHMENT_LOAD_OP_CLEAR
      // operation
      render_pass_info.clearValueCount = clear_values.size();
      render_pass_info.pClearValues = clear_values.data();
	    	    
      vkCmdBeginRenderPass(cmd_buffer,
			   &render_pass_info,
			   VK_SUBPASS_CONTENTS_INLINE);      
    }

    bool commands_begin_buffer(VkCommandBuffer cmd_buffer) {
      bool ret = ok();
      
      if (ret) {
	VkCommandBufferBeginInfo begin_info = {};
	  
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = nullptr;

	VK_FN(vkBeginCommandBuffer(cmd_buffer, &begin_info));

	ret = ok();
      }

      return ret;
    }

    bool commands_end_buffer(VkCommandBuffer cmd_buffer) {
      VK_FN(vkEndCommandBuffer(cmd_buffer));
      return ok();
    }

    void commands_copy_image(VkCommandBuffer cmd_buffer, image_pool::index_type src, image_pool::index_type dst) const {

      VkImageCopy copy_region = m_image_pool.image_copy(src);
      
      vkCmdCopyImage(cmd_buffer,
		     m_image_pool.image(src),
		     m_image_pool.layout_final(src),
		     m_image_pool.image(dst),
		     m_image_pool.layout_final(dst),
		     1,
		     &copy_region);		     
    }

    bool commands_layout_transition(VkCommandBuffer cmd_buffer,
				    image_pool::index_type image) const {
      bool ret = ok();
      if (ret) {
	auto layout_transition =
	  m_image_pool.make_layout_transition(image);

        ret = c_assert(layout_transition.ok());
	  
	if (ret) {
	  layout_transition.via(cmd_buffer);
	  ret = c_assert(ok());
	}
      }
      return ret;
    }

    void commands_draw_inner_objects(VkCommandBuffer cmd_buffer, VkPipelineLayout pipeline_layout) const {
      for (auto const& [name, index]: m_model_data.indices) {
	if (name != "outer-cube") {
	  commands_draw_model(index,
			      cmd_buffer,
			      pipeline_layout);
	}
      }
    }

    void commands_draw_room(VkCommandBuffer cmd_buffer, VkPipelineLayout pipeline_layout) const {
      commands_draw_model("outer-cube",
			  cmd_buffer,
			  pipeline_layout);
    }

    void commands_draw_quad_no_vb(VkCommandBuffer cmd_buffer) const {
      vkCmdDraw(cmd_buffer,
		4,
		1,
		0,
		0);
    }
    
    void commands_draw_model(uint32_t model,
			     VkCommandBuffer cmd_buffer,
			     VkPipelineLayout pipeline_layout) const {

      push_constant::model pc_m{};
      
      pc_m.model_to_world = m_model_data.transforms.at(model)();
      push_constant::model_upload(pc_m, cmd_buffer, pipeline_layout);
      
      vkCmdDraw(cmd_buffer,
		m_model_data.vb_lengths.at(model), // num vertices
	        m_model_data.vb_lengths.at(model) / 3, // num instances
		m_model_data.vb_offsets.at(model), // first vertex
		m_model_data.vb_offsets.at(model) / 3); // first instance

    }

    void commands_draw_model(const std::string& name,
			     VkCommandBuffer cmd_buffer,
			     VkPipelineLayout pipeline_layout) const {
      commands_draw_model(m_model_data.indices.at(name),
			  cmd_buffer,
			  pipeline_layout);
    }
    
    void commands_draw_main(VkCommandBuffer cmd_buffer,
			    VkPipelineLayout pipeline_layout) const {
      
      push_constant::basic_pbr pc_bp{push_constant::basic_pbr_default()};

      pc_bp.camera_position = m_camera_position;
      pc_bp.sampler = 0;
      push_constant::basic_pbr_upload(pc_bp, cmd_buffer, pipeline_layout);
      
      commands_draw_inner_objects(cmd_buffer, pipeline_layout);
      
      pc_bp.sampler = 1;
      push_constant::basic_pbr_upload(pc_bp, cmd_buffer, pipeline_layout);
      
      commands_draw_room(cmd_buffer, pipeline_layout);
    }

    //
    // we always use the command pool here to allocate command buffer memory
    //
    bool make_command_buffers(darray<VkCommandBuffer>& command_buffers) const {
      bool ret =
	c_assert(!command_buffers.empty()) &&
	c_assert(ok_command_pool());
      
      if (ret) {
	VkCommandBufferAllocateInfo alloc_info = {};

	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = m_vk_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());

	VK_FN(vkAllocateCommandBuffers(m_vk_curr_ldevice, &alloc_info, command_buffers.data()));

	ret = ok() && !command_buffers.empty();
      }
      return ret;
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
      m_image_pool.print_images_info();
      if (ok_command_pool()) {
	
	//
	// bind sampler[i] to test_texture_indices[i] via
	// test_texture_index's array element index (should be i)
	//
	darray<texture_pool::index_type> tex_indices(m_test_texture_indices.begin(),
						     m_test_texture_indices.end());

	// first descriptor set update for textures
	if (c_assert(m_texture_pool.update_descriptor_sets(m_vk_curr_ldevice,
							   std::move(tex_indices)))) {
	  //
	  // perform the image layout transition for
	  // test_image_indices[i]
	  //
	  run_cmds([this](VkCommandBuffer cmd_buf) {
		     puts("image_layout_transition");		     
		     m_ok_scene =
		       m_image_pool.make_layout_transitions(cmd_buf,
							    m_test_image_indices);
		   },
	    [this]() {
	      puts("run_cmds ERROR");
	      ASSERT(false);
	      m_ok_scene = false;
	    });

	  //
	  // update the descriptor sets here to include the input attachment
	  // for the shader
	  //	   	    

	  m_vk_command_buffers.resize(m_vk_swapchain_image_views.size());

	  bool good = c_assert(make_command_buffers(m_vk_command_buffers));
	    
	  if (good) {	    
	    //
	    // begin the command buffer series;
	    // we obviously have two render passes,
	    // and the code corresponding to each pass
	    // is labeled in inner scope blocks as follows
	    //
	    size_t i{0};	      
	    while (i < m_vk_command_buffers.size() &&
		   c_assert(good)) {			        

	      VkCommandBuffer cmd_buff = m_vk_command_buffers.at(i);

	      good = c_assert(commands_begin_buffer(cmd_buff));
	      
	      if (good) {
		vkCmdUpdateBuffer(cmd_buff,
				  m_vk_vertex_buffer,
				  0,
				  sizeof(m_vertex_buffer_vertices[0]) * m_vertex_buffer_vertices.size(),
				  m_vertex_buffer_vertices.data());
		
		commands_start_render_pass(cmd_buff,
					   m_vk_swapchain_framebuffers.at(i),
					   m_vk_render_pass);

		//
		// subpass 1: fill depth and color attachments
		//

		commands_begin_pipeline(cmd_buff,
					pipeline(k_pass_texture2d),
					pipeline_layout(k_pass_texture2d),
					true,
				        {
					 descriptor_set(k_descriptor_set_samplers),
					 descriptor_set(k_descriptor_set_uniform_blocks)
					});

		commands_draw_main(cmd_buff,
				   pipeline_layout(k_pass_texture2d));


		vkCmdNextSubpass(cmd_buff, VK_SUBPASS_CONTENTS_INLINE);
		
		//
		// subpass 2: read depth and color attachments
		//

		commands_begin_pipeline(cmd_buff,
					pipeline(k_pass_test_fbo),
					pipeline_layout(k_pass_test_fbo),
					false, // do not bind vertex buffer
					m_descriptor_set_pool.descriptor_sets(m_descriptors.attachment_read));		
		
		commands_draw_quad_no_vb(cmd_buff);

		vkCmdEndRenderPass(cmd_buff);
#if 0
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
#endif
		
		good = c_assert(commands_end_buffer(cmd_buff));
	      }
	      	      
	      i++;
	    }

	    m_ok_command_buffers = good;	      
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
      setup_vertex_data();
      setup_descriptor_pool();
      setup_render_pass();
      setup_attachment_read_descriptors();
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
      m_camera_position = -vec3_t{w2v[3]};
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
      
      free_vk_ldevice_handle<VkDeviceMemory, &vkFreeMemory>(m_vk_vertex_buffer_mem);
      
      free_vk_ldevice_handle<VkBuffer, &vkDestroyBuffer>(m_vk_vertex_buffer);
      
      free_vk_ldevice_handle<VkSemaphore, &vkDestroySemaphore>(m_vk_sem_image_available);
      free_vk_ldevice_handle<VkSemaphore, &vkDestroySemaphore>(m_vk_sem_render_finished);
      
      free_vk_ldevice_handle<VkCommandPool, &vkDestroyCommandPool>(m_vk_command_pool);
      
      free_vk_ldevice_handles<VkFramebuffer, &vkDestroyFramebuffer>(m_vk_swapchain_framebuffers);
      
      m_pipeline_pool.free_mem(m_vk_curr_ldevice);
      
      m_pipeline_layout_pool.free_mem(m_vk_curr_ldevice);

      free_vk_ldevice_handle<VkRenderPass, &vkDestroyRenderPass>(m_vk_render_pass);
      
      //      m_render_pass_pool.free_mem(m_vk_curr_ldevice);
      
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
