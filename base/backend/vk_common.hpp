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
    static inline constexpr VkImageLayout k_initial_layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    static inline constexpr VkImageLayout k_final_layout = VK_IMAGE_LAYOUT_GENERAL;
    static inline constexpr VkFormat k_format = VK_FORMAT_R8G8B8A8_UNORM;
      
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
    static inline constexpr VkFormat k_format = VK_FORMAT_D32_SFLOAT;
    static inline constexpr VkImageLayout k_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    static inline constexpr VkImageLayout k_final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
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
  
  void write_descriptor_set(VkDevice device,
			    VkBuffer buffer,
			    VkDeviceSize size,
			    VkDescriptorSet descset,
			    uint32_t binding_index,
			    uint32_t array_element,
			    VkDescriptorType descriptor_type);
}
