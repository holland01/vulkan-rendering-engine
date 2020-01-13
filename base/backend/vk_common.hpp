#pragma once

#include "common.hpp"

#define VK_FN(expr)						\
  do {								\
    if (api_ok()) {						\
      g_vk_result = vk_call((expr), #expr, __LINE__, __FILE__);	\
    }								\
  } while (0)

namespace vulkan {
  extern VkResult g_vk_result;
  
  VkResult vk_call(VkResult call, const char* expr, int line, const char* file);
  
  bool api_ok();

#define H_OK(h) api_ok() && ((h) != VK_NULL_HANDLE)

#define HANDLE_GET_FN_IMPL(index_name, ok_fn_name, vector_member, handle_type, null_value) \
  handle_type ret{null_value};						\
  if (ok_fn_name(index_name)) {						\
    ret = vector_member.at(index_name);					\
  }									\
  return ret
  
#define VK_HANDLE_GET_FN_IMPL(index_name, ok_fn_name, vector_member, vk_handle_type) \
  HANDLE_GET_FN_IMPL(index_name, ok_fn_name, vector_member, vk_handle_type, VK_NULL_HANDLE)

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
  private:
    bool m_ready;
    
  public:    
    VkImageMemoryBarrier barrier;

    VkPipelineStageFlags src_stage_mask;
    VkPipelineStageFlags dst_stage_mask;
    
    image_layout_transition(bool pre_ready = true)
      : m_ready(pre_ready) {     
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

    image_layout_transition& ready() {
      m_ready = true;
      return *this;
    }

    bool ok() const {
      bool r =
	api_ok() && m_ready == true;
      ASSERT(r);
      return r;
    }

    image_layout_transition& via(VkCommandBuffer buffer) {       
      if (ok()) {
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
  
  struct image_requirements {
    VkExtent3D desired{UINT32_MAX, UINT32_MAX, UINT32_MAX};
    VkExtent3D required{UINT32_MAX, UINT32_MAX, UINT32_MAX};
    uint32_t bytes_per_pixel{UINT32_MAX};
    uint32_t memory_type_index{UINT32_MAX};

    VkDeviceSize memory_size() const {
      ASSERT(ok());
      
      return
	static_cast<VkDeviceSize>((bytes_per_pixel * required.width * required.height) *
				  required.depth);
    }
    
    bool ok() const {
      const bool ok_desired =
	desired.width != UINT32_MAX &&
	desired.height != UINT32_MAX &&
	desired.depth != UINT32_MAX;
      
      const bool ok_required =
	required.width != UINT32_MAX &&
	required.height != UINT32_MAX &&
	required.depth != UINT32_MAX;

      const bool ok_cmp =
	desired.width <= required.width &&
	desired.height <= required.height &&
	desired.depth <= required.depth;

      return
	(bytes_per_pixel <= 4) &&
	(memory_type_index < 32) &&
	ok_desired &&
	ok_required &&
	ok_cmp;		    
    }
  };

  struct vertex_data {
    vec3_t position;
    vec2_t st;
    vec3_t color;
  };
  
  struct device_resource_properties {
    darray<uint32_t> queue_family_indices;
    VkPhysicalDevice physical_device{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkSharingMode queue_sharing_mode{VK_SHARING_MODE_EXCLUSIVE};
    VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};

    bool ok() const {
      bool r =
 	!queue_family_indices.empty() &&
	physical_device != VK_NULL_HANDLE &&
	device != VK_NULL_HANDLE &&
	descriptor_pool != VK_NULL_HANDLE;

      ASSERT(r);
      
      return r;
    }
  };

  static inline uint32_t bpp_from_format(VkFormat in) {
    uint32_t ret = 0;
    switch (in) {
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_R8G8B8A8_UNORM:
      ret = 4;
      break;
    default:
      ASSERT(false);
      break;
    }
    return ret;
  }

  template <class vkHandleType,
	    void (*vkDestroyFn)(VkDevice, vkHandleType, const VkAllocationCallbacks*)>
  void free_device_handle(VkDevice device, vkHandleType& handle) {
    if (api_ok()) {
      ASSERT(device != VK_NULL_HANDLE);
      VK_FN(vkDeviceWaitIdle(device));
      if (handle != VK_NULL_HANDLE) {
	vkDestroyFn(device, handle, nullptr);
	handle = VK_NULL_HANDLE;
      }
    }
  }
  
  struct texture2d_data {
    static inline constexpr uint32_t k_binding = 0;
    static inline constexpr uint32_t k_binding_count = 1;
    static inline constexpr uint32_t k_array_elem = 0;
    static inline constexpr uint32_t k_descriptor_count = 1;
    static inline constexpr VkDescriptorType k_descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    static inline constexpr uint32_t k_bpp = 4;
    static inline constexpr VkFormat k_format =
      VK_FORMAT_R8G8B8A8_UNORM;

    static inline constexpr VkImageLayout k_initial_layout =
      VK_IMAGE_LAYOUT_PREINITIALIZED;

    static inline constexpr VkImageLayout k_final_layout =
      VK_IMAGE_LAYOUT_GENERAL;

    static inline constexpr VkImageAspectFlags k_image_aspect_flags =
      VK_IMAGE_ASPECT_COLOR_BIT;

    static inline constexpr VkImageUsageFlags k_image_usage_flags =
      VK_IMAGE_USAGE_SAMPLED_BIT;

    static inline constexpr VkImageTiling k_image_tiling =
      VK_IMAGE_TILING_LINEAR;

    static inline constexpr VkAccessFlags k_access_flags =
      VK_ACCESS_SHADER_READ_BIT;

    static inline constexpr VkMemoryPropertyFlags k_memory_property_flags =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    VkSampler sampler{VK_NULL_HANDLE};
    VkImage image{VK_NULL_HANDLE};
    VkImageView image_view{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptor_set_layout{VK_NULL_HANDLE};
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE}; // should be allocated from a pool
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkImageLayout layout{texture2d_data::k_final_layout};
    
    uint32_t width{UINT32_MAX};
    uint32_t height{UINT32_MAX};    

    bool ok() const {
      bool r = 
	sampler != VK_NULL_HANDLE &&
	image != VK_NULL_HANDLE &&
	image_view != VK_NULL_HANDLE &&
	memory != VK_NULL_HANDLE &&
	descriptor_set_layout != VK_NULL_HANDLE &&
	descriptor_set != VK_NULL_HANDLE &&
	format != VK_FORMAT_UNDEFINED &&
	width != UINT32_MAX &&
	height != UINT32_MAX;
      ASSERT(r);
      return r;
    }

    VkDescriptorImageInfo make_descriptor_image_info() const {
      VkDescriptorImageInfo ret = {};
      if (ok()) {
	ret.sampler = sampler;
	ret.imageView = image_view;
	ret.imageLayout = layout;
      }
      return ret;
    }
    
    VkWriteDescriptorSet make_write_descriptor_set(const VkDescriptorImageInfo* image_info) const {
      VkWriteDescriptorSet ret = {};
      if (ok()) {
	ret.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	ret.pNext = nullptr;
	ret.dstSet = descriptor_set;
	ret.dstBinding = k_binding;
	ret.dstArrayElement = k_array_elem;
	ret.descriptorCount = k_descriptor_count;
	ret.descriptorType = k_descriptor_type;
	ret.pImageInfo = image_info;
	ret.pBufferInfo = nullptr;
	ret.pTexelBufferView = nullptr;
      }
      return ret;
    }

    void free_mem(VkDevice device) {
      VK_FN(vkDeviceWaitIdle(device));

      if (api_ok()) {
	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);	
	vkDestroySampler(device, sampler, nullptr);
	vkDestroyImageView(device, image_view, nullptr);
	vkDestroyImage(device, image, nullptr);	
	vkFreeMemory(device, memory, nullptr);
      }
    }
  };

  struct depthbuffer_data {
    static inline constexpr uint32_t k_bpp = 4;
    static inline constexpr VkFormat k_format = VK_FORMAT_D24_UNORM_S8_UINT;
    static inline constexpr VkImageLayout k_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    static inline constexpr VkImageLayout k_final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    static inline constexpr VkImageAspectFlags k_image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    static inline constexpr VkImageUsageFlags k_image_usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    static inline constexpr VkImageTiling k_image_tiling = VK_IMAGE_TILING_OPTIMAL;
    static inline constexpr VkAccessFlags k_access_flags =
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    static inline constexpr VkMemoryPropertyFlags k_memory_property_flags =      
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    uint32_t width{UINT32_MAX};
    uint32_t height{UINT32_MAX};
    
    VkImage image{VK_NULL_HANDLE};
    VkImageView image_view{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    
    bool ok() const {
      bool r =
	width != UINT32_MAX &&
	height != UINT32_MAX &&
	image != VK_NULL_HANDLE &&
	image_view != VK_NULL_HANDLE &&
	memory != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    std::string to_string() const {
      std::stringstream ss;
      ss << "depthbuffer_data:\n"
	 << "..." << AS_STRING_SS(width) << "\n"
	 << "..." << AS_STRING_SS(height) << "\n"
	 << "..." << SS_HEX_NAME(image) << "\n"
	 << "..." << SS_HEX_NAME(image_view) << "\n"
	 << "..." << SS_HEX_NAME(memory) << "\n";
      return ss.str();
    }
    
    void free_mem(VkDevice device) {
      VK_FN(vkDeviceWaitIdle(device));
      
      free_device_handle<VkImageView, &vkDestroyImageView>(device, image_view);
      free_device_handle<VkImage, &vkDestroyImage>(device, image);
      free_device_handle<VkDeviceMemory, vkFreeMemory>(device, memory);
    }
  };
  
  std::string to_string(VkExtent3D e);
  std::string to_string(const image_requirements& r);

    // Find _a_ memory in `memory_type_bits_req` that includes all of `req_properties`
  int32_t find_memory_properties(const VkPhysicalDeviceMemoryProperties* memory_properties,
				 uint32_t memory_type_bits_req,
				 VkMemoryPropertyFlags req_properties);
				 

  VkViewport make_viewport(vec2_t origin, VkExtent2D dim, float depthmin, float depthmax);

  VkFormat find_supported_format(VkPhysicalDevice physical_device,
				 const darray<VkFormat>& candidates,
				 VkImageTiling tiling,
				 VkFormatFeatureFlags features);

  VkDescriptorSetLayoutBinding make_descriptor_set_layout_binding(uint32_t binding,
								  VkShaderStageFlags stages,
								  uint32_t num_descriptors,
								  VkDescriptorType type);


    
  VkDescriptorSetLayout make_descriptor_set_layout(VkDevice device,
						   const VkDescriptorSetLayoutBinding* bindings,
						   uint32_t num_bindings);

  VkDescriptorSet make_descriptor_set(VkDevice device,
				      VkDescriptorPool descriptor_pool,
				      const VkDescriptorSetLayout* layouts,
				      uint32_t num_sets);

  void write_device_memory(VkDevice device,
			   VkDeviceMemory memory,
			   const void* data,
			   VkDeviceSize size);
  
  VkDeviceMemory make_device_memory(VkDevice device,
				    const void* data,
				    VkDeviceSize size,
				    VkDeviceSize alloc_size,
				    uint32_t index);

  VkBuffer make_buffer(const device_resource_properties& resource_props,
		       VkBufferCreateFlags create_flags,
		       VkBufferUsageFlags usage_flags,
		       VkDeviceSize sz);

  VkDescriptorBufferInfo make_descriptor_buffer_info(VkBuffer buffer, VkDeviceSize size);

  VkWriteDescriptorSet make_write_descriptor_buffer_set(VkDescriptorSet descset,
							const VkDescriptorBufferInfo* buffer_info,
							uint32_t binding_index,
							uint32_t array_element,
							VkDescriptorType descriptor_type);
  
  bool write_descriptor_set(VkDevice device,
			    VkBuffer buffer,
			    VkDeviceSize size,
			    VkDescriptorSet descset,
			    uint32_t binding_index,
			    uint32_t array_element,
			    VkDescriptorType descriptor_type);
}
