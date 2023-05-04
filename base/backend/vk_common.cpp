#include "vk_common.hpp"
#include <string.h>

namespace vulkan {
  const darray<VkImageLayout> k_invalid_attachment_layouts =
    {
     VK_IMAGE_LAYOUT_UNDEFINED,
     VK_IMAGE_LAYOUT_PREINITIALIZED,
     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
  
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

  VkFormat find_supported_format(VkPhysicalDevice physical_device,
				 const darray<VkFormat>& candidates,
				 VkImageTiling tiling,
				 VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

      if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
	return format;
      } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
	return format;
      }
    }

    return VK_FORMAT_UNDEFINED;
  }

  VkDescriptorSetLayoutBinding make_descriptor_set_layout_binding(uint32_t binding,
								  VkShaderStageFlags stages,
								  uint32_t num_descriptors,
								  VkDescriptorType type) {
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = binding;
    layout_binding.descriptorType = type;
    layout_binding.descriptorCount = num_descriptors;
    layout_binding.stageFlags = stages;
    layout_binding.pImmutableSamplers = nullptr;
    return layout_binding;
  }
    
  VkDescriptorSetLayout make_descriptor_set_layout(VkDevice device,
						   const VkDescriptorSetLayoutBinding* bindings,
						   uint32_t num_bindings) {
    VkDescriptorSetLayoutCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.pBindings = bindings;
    create_info.bindingCount = num_bindings;

    VkDescriptorSetLayout layout{VK_NULL_HANDLE};
      
    VK_FN(vkCreateDescriptorSetLayout(device,
				      &create_info,
				      nullptr,
				      &layout));
    return layout;
  }

  VkDescriptorSet make_descriptor_set(VkDevice device,
				      VkDescriptorPool descriptor_pool,
				      const VkDescriptorSetLayout* layouts,
				      uint32_t num_sets) {
    VkDescriptorSet set{VK_NULL_HANDLE};

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = num_sets;
    alloc_info.pSetLayouts = layouts;

    VK_FN(vkAllocateDescriptorSets(device,
				   &alloc_info,
				   &set));

    return set;
  }

  void write_device_memory(VkDevice device,
			   VkDeviceMemory memory,
			   const void* data,
			   VkDeviceSize size) {
    void* mapped_memory = nullptr;

    VK_FN(vkMapMemory(device,
		      memory,
		      0,
		      size,
		      0,
		      &mapped_memory));

    ASSERT(mapped_memory != nullptr);
    ASSERT(api_ok());
      
    if (api_ok() && mapped_memory != nullptr) {
      memcpy(mapped_memory, data, size);

      vkUnmapMemory(device,
		    memory);
    }
  }
    
  VkDeviceMemory make_device_memory(VkDevice device,
				    const void* data,
				    VkDeviceSize size,
				    VkDeviceSize alloc_size,
				    uint32_t index) {
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkMemoryAllocateInfo info = {};
      
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = nullptr;
    info.allocationSize = alloc_size;
    info.memoryTypeIndex = index;

    VK_FN(vkAllocateMemory(device,
			   &info,
			   nullptr,
			   &memory));

    if (api_ok() &&
	memory != VK_NULL_HANDLE &&
	data != nullptr) {
      write_device_memory(device, memory, data, size);
    }

    return memory;
  }

  VkBuffer make_buffer(const device_resource_properties& resource_props,
		       VkBufferCreateFlags create_flags,
		       VkBufferUsageFlags usage_flags,
		       VkDeviceSize sz) {
    VkBufferCreateInfo create_info = {};

    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = create_flags;
    create_info.usage = usage_flags;
    create_info.sharingMode = resource_props.queue_sharing_mode;
    create_info.queueFamilyIndexCount = resource_props.queue_family_indices.size();
    create_info.pQueueFamilyIndices = resource_props.queue_family_indices.data();
    create_info.size = sz;

    VkBuffer buffer{VK_NULL_HANDLE};
      
    VK_FN(vkCreateBuffer(resource_props.device,
			 &create_info,
			 nullptr,
			 &buffer));
      
    return buffer;
  }

  VkDescriptorBufferInfo make_descriptor_buffer_info(VkBuffer buffer, VkDeviceSize size) {
    VkDescriptorBufferInfo info = {};
    info.buffer = buffer;
    info.offset = 0;
    info.range = size;
    return info;
  }

  VkWriteDescriptorSet make_write_descriptor_buffer_set(VkDescriptorSet descset,
							const VkDescriptorBufferInfo* buffer_info,
							uint32_t binding_index,
							uint32_t array_element,
							VkDescriptorType descriptor_type) {
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstSet = descset;
    write.dstBinding = binding_index;
    write.dstArrayElement = array_element;
    write.descriptorCount = 1;
    write.descriptorType = descriptor_type;
    write.pImageInfo = nullptr;
    write.pBufferInfo = buffer_info;
    write.pTexelBufferView = nullptr;
    return write;
  }

  VkWriteDescriptorSet make_write_descriptor_set(VkDescriptorSet descset,
						 const VkDescriptorImageInfo* image_info,
						 uint32_t binding,
						 VkDescriptorType type,
						 uint32_t descriptor_count) {
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstSet = descset;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = descriptor_count;
    write.descriptorType = type;
    write.pImageInfo = image_info;
    write.pBufferInfo = nullptr;
    write.pTexelBufferView = nullptr;
    return write;
  }

  bool write_descriptor_set(VkDevice device,
			    VkBuffer buffer,
			    VkDeviceSize size,
			    VkDescriptorSet descset,
			    uint32_t binding_index,
			    uint32_t array_element,
			    VkDescriptorType descriptor_type) {
    auto buffer_info = make_descriptor_buffer_info(buffer, size);
      
    auto write_desc_set = make_write_descriptor_buffer_set(descset,
							   &buffer_info,
							   binding_index,
							   array_element,
							   descriptor_type);
    vkUpdateDescriptorSets(device,
			   1,
			   &write_desc_set,
			   0,
			   nullptr);

    VK_FN(vkDeviceWaitIdle(device));

    return api_ok();
  }

  std::optional<buffer_reqs> get_buffer_requirements(const device_resource_properties& resource_props,
						     VkBufferCreateFlags create_flags,
						     VkBufferUsageFlags usage_flags,
						     VkMemoryPropertyFlags memory_property_flags,
						     VkDeviceSize desired_size) {      
    std::optional<buffer_reqs> ret;

    int32_t out_property_index;
    VkDeviceSize out_device_size;
    
    if (resource_props.ok()) {
      
      VkBuffer dummy = make_buffer(resource_props,
				   create_flags,
				   usage_flags,
				   desired_size);
      
      if (H_OK(dummy)) {
	VkMemoryRequirements req = {};
	
	vkGetBufferMemoryRequirements(resource_props.device,
				      dummy,
				      &req);

	out_device_size = req.size;

	VkPhysicalDeviceMemoryProperties mem_props = {};

	vkGetPhysicalDeviceMemoryProperties(resource_props.physical_device,
					    &mem_props);
	
	out_property_index =
	  find_memory_properties(&mem_props,
				 req.memoryTypeBits,
				 memory_property_flags);
	
	ASSERT(out_property_index != -1);

	
	if (desired_size <= req.size &&
	    out_property_index != -1) {
	  
	  buffer_reqs r
	    {
	     out_device_size,
	     static_cast<uint32_t>(out_property_index)
	    };

	  if (r.ok()) {
	    ret = r;
	  }
	}

	vkDestroyBuffer(resource_props.device, dummy, nullptr);
      }     
    }
    
    return ret;
  }
  
  void one_shot_command_buffer(const device_resource_properties& properties,
			       one_shot_command_fn_ok_t f_ok,
			       one_shot_command_fn_err_t f_err) {
    
    if (properties.ok() &&
	c_assert(static_cast<bool>(f_ok)) &&
	c_assert(static_cast<bool>(f_err))) {
      
      VkCommandBufferAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandPool = properties.command_pool;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      alloc_info.commandBufferCount = 1;

      VkCommandBuffer cmd_buffer{VK_NULL_HANDLE};	
      VK_FN(vkAllocateCommandBuffers(properties.device,
				     &alloc_info,
				     &cmd_buffer));

      if (cmd_buffer != VK_NULL_HANDLE) {
	if (api_ok()) {
	  VkCommandBufferBeginInfo begin_info = {};
	  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	  vkBeginCommandBuffer(cmd_buffer, &begin_info);

	  f_ok(cmd_buffer);
	  
	  vkEndCommandBuffer(cmd_buffer);

	  VkSubmitInfo submit_info = {};
	  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	  submit_info.commandBufferCount = 1;
	  submit_info.pCommandBuffers = &cmd_buffer;

	  VK_FN(vkQueueSubmit(properties.command_queue,
			      1,
			      &submit_info,
			      VK_NULL_HANDLE));
	  
	  VK_FN(vkQueueWaitIdle(properties.command_queue));
	}
	else {
	  f_err(one_shot_command_error::allocate_command_buffer);
	}

	vkFreeCommandBuffers(properties.device,
			     properties.command_pool,
			     1,
			     &cmd_buffer);
      }
      else {
	f_err(one_shot_command_error::allocate_command_buffer);
      }
    }
    else {
      f_err(one_shot_command_error::device_resource_properties);
    }
  }
}
