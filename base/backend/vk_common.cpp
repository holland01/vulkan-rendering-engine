#include "vk_common.hpp"

namespace vulkan {
  VkResult g_vk_result = VK_SUCCESS;

  VkResult vk_call(VkResult call, const char* expr, int line, const char* file) {
    if (call != VK_SUCCESS) {
      write_logf("VULKAN ERROR: %s@%s:%i -> 0x%x (%" PRId32 ")\n", 
                 expr, 
                 file, 
                 line, 
                 call,
		 call);
    }
    return call;
  }
  
  bool api_ok() {
    return g_vk_result == VK_SUCCESS;
  }

  std::string to_string(VkExtent3D e) {
    std::stringstream ss;
    ss << "(" << e.width << ", " << e.height << ", " << e.depth << ")";
    return ss.str();
  }

  std::string to_string(const image_requirements& r) {
    std::stringstream ss;

    ss << "image_requirements\n"
       << "...desired = " << to_string(r.desired) << "\n"
       << "...required = " << to_string(r.required) << "\n"
       << "...bytes_per_pixel =" << r.bytes_per_pixel << "\n"
       << "...memory_type_index = " << r.memory_type_index;
    
    return ss.str();  
  }

  // Find _a_ memory in `memory_type_bits_req` that includes all of `req_properties`
  int32_t find_memory_properties(const VkPhysicalDeviceMemoryProperties* memory_properties,
				 uint32_t memory_type_bits_req,
				 VkMemoryPropertyFlags req_properties) {
    const uint32_t memory_count = memory_properties->memoryTypeCount;
    uint32_t memory_index = 0;
    int32_t ret_val = -1;

    while (memory_index < memory_count && ret_val == -1) {
      const uint32_t type_bits = 1 << memory_index;

      if ((type_bits & memory_type_bits_req) != 0) {
	const VkMemoryPropertyFlags properties =
	  memory_properties->memoryTypes[memory_index].propertyFlags;

	if ((properties & req_properties) == req_properties) {
	  ret_val = static_cast<int32_t>(memory_index);
	}
      }

      memory_index++;
    }

    return ret_val;
  }

  VkViewport make_viewport(vec2_t origin, VkExtent2D dim, float depthmin, float depthmax) {
    VkViewport viewport = {};
    viewport.x = origin.x;
    viewport.y = origin.y;
    viewport.width = dim.width;
    viewport.height = dim.height;
    viewport.minDepth = depthmin;
    viewport.maxDepth = depthmax;
    return viewport;
  }

}
