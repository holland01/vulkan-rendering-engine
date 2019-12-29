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



  std::string to_string(VkExtent3D e);
  std::string to_string(const image_requirements& r);

    // Find _a_ memory in `memory_type_bits_req` that includes all of `req_properties`
  int32_t find_memory_properties(const VkPhysicalDeviceMemoryProperties* memory_properties,
				 uint32_t memory_type_bits_req,
				 VkMemoryPropertyFlags req_properties);
				 

  VkViewport make_viewport(vec2_t origin, VkExtent2D dim, float depthmin, float depthmax);
}

