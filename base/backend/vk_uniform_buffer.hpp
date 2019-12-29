#pragma once

#include "vk_common.hpp"

namespace vulkan {
  struct uniform_block_data {
    static inline constexpr uint32_t k_set = 1;
    static inline constexpr uint32_t k_binding = 0;

    VkDescriptorSetLayout descriptor_set_layout{VK_NULL_HANDLE};
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE}; // should be allocated from a pool
    VkDeviceMemory device_memory{VK_NULL_HANDLE};
    VkBuffer buffer{VK_NULL_HANDLE};
    
    bool ok() const {
      bool r =
	descriptor_set_layout != VK_NULL_HANDLE &&
	descriptor_set != VK_NULL_HANDLE;

      ASSERT(r);
      return r;
    }

    void free_mem(VkDevice device) {
      VK_FN(vkDeviceWaitIdle(device));

      if (api_ok()) {
	vkFreeMemory(device, device_memory, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
	vkDestroyBuffer(device, buffer, nullptr);
      }
    }
  };

  class uniform_block_pool {
  public:
    using index_type = int16_t;
    static constexpr index_type k_unset = std::numeric_limits<index_type>::max();
    
  private:
    // uniform block data
    darray<uint32_t> m_binding_indices{};
    darray<uint32_t> m_sizes{};
    darray<uint32_t> m_set_indices{};
    darray<VkDescriptorSetLayout> m_descriptor_set_layouts{};
    darray<VkDescriptorSet> m_descriptor_sets{};
    darray<VkDeviceMemory> m_device_memories{};
    darray<VkBuffer> m_buffers{};

    device_resource_properties m_resource_props;
    
    index_type push_blank() {
      auto i = m_binding_indices.size();
      
      m_binding_indices.push_back(UINT32_MAX);
      m_sizes.push_back(UINT32_MAX);
      m_set_indices.push_back(UINT32_MAX);
      m_descriptor_set_layouts.push_back(VK_NULL_HANDLE);
      m_descriptor_sets.push_back(VK_NULL_HANDLE);
      m_device_memories.push_back(VK_NULL_HANDLE);
      m_buffers.push_back(VK_NULL_HANDLE);

      return i;
    }

    VkBuffer make_buffer(size_t sz) const {
      VkBufferCreateInfo create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      create_info.pNext = nullptr;
      create_info.flags = 0;
      create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      create_info.sharingMode = m_resource_props.queue_sharing_mode;
      create_info.queueFamilyIndexCount = m_resource_props.queue_family_indices.size();
      create_info.pQueueFamilyIndices = m_resource_props.queue_family_indices.data();
      create_info.size = sz;

      VkBuffer buffer{VK_NULL_HANDLE};
      
      VK_FN(vkCreateBuffer(m_resource_props.device,
			   &create_info,
			   nullptr,
			   &buffer));
      
      return buffer;
    }


    VkDescriptorSetLayoutBinding make_descriptor_set_layout_binding(uint32_t binding, VkShaderStageFlags stages) {
      VkDescriptorSetLayoutBinding layout_binding = {};
      layout_binding.binding = binding;
      layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      layout_binding.descriptorCount = 1;
      layout_binding.stageFlags = stages;
      layout_binding.pImmutableSamplers = nullptr;
      return layout_binding;
    }
    
    VkDescriptorSetLayout make_descriptor_set_layout(const VkDescriptorSetLayoutBinding* bindings, uint32_t num_bindings) {
      VkDescriptorSetLayoutCreateInfo create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      create_info.pNext = nullptr;
      create_info.flags = 0;
      create_info.pBindings = bindings;
      create_info.bindingCount = num_bindings;

      VkDescriptorSetLayout layout{VK_NULL_HANDLE};
      
      VK_FN(vkCreateDescriptorSetLayout(m_resource_props.device,
					&create_info,
					nullptr,
					&layout));
      return layout;
    }

    VkDescriptorSet make_descriptor_set(const VkDescriptorSetLayout* layouts, uint32_t num_sets) {
      VkDescriptorSet set{VK_NULL_HANDLE};

      VkDescriptorSetAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      alloc_info.pNext = nullptr;
      alloc_info.descriptorPool = m_resource_props.descriptor_pool;
      alloc_info.descriptorSetCount = num_sets;
      alloc_info.pSetLayouts = layouts;

      VK_FN(vkAllocateDescriptorSets(m_resource_props.device,
				     &alloc_info,
				     &set));

      return set;
    }

    VkDeviceMemory make_buffer_memory(void* data, size_t size, size_t alloc_size, uint32_t index) {
      VkDeviceMemory ret{};
      VkMemoryAllocateInfo info = {};
      
      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.pNext = nullptr;
      info.allocationSize = alloc_size;
      info.memoryTypeIndex = index;

      VK_FN(vkAllocateMemory(m_resource_props.device,
			     &info,
			     nullptr,
			     &ret));      

      ASSERT(false); // finish this
      return ret;
    }
    
    bool is_valid(size_t block_size, int32_t& out_property_index) const {
      bool ret = false;

      VkBuffer dummy = make_buffer(block_size);
      
      if (api_ok() && dummy != VK_NULL_HANDLE) {
	VkMemoryRequirements req = {};
	
	vkGetBufferMemoryRequirements(m_resource_props.device,
				      dummy,
				      &req);

	VkPhysicalDeviceMemoryProperties mem_props = {};

	vkGetPhysicalDeviceMemoryProperties(m_resource_props.physical_device,
					    &mem_props);
	
	out_property_index =
	  find_memory_properties(&mem_props,
				 req.memoryTypeBits,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	
	if (block_size <= req.size &&
	    out_property_index != -1) {
	  ret = true;
	}

	vkDestroyBuffer(m_resource_props.device, dummy, nullptr);
      }
            
      return ret;
    }

    
  public:
    uniform_block_pool(device_resource_properties drp)
	: m_resource_props{drp}
	    
    {}
    
    index_type add(void* block_data,
		   size_t block_size,
		   uint32_t set,
		   uint32_t binding_index,
		   VkShaderStageFlags stage_flags) {
      index_type ret = -1;
      
      int32_t memory_property_index = -1;
      // THIS needs to be queried from is_valid and passed to
      // make_buffer_memory. It's the required size.
      size_t required_memory = 0; 
      bool valid = is_valid(block_size, memory_property_index);
      ASSERT(valid);

      VkBuffer buffer{VK_NULL_HANDLE};
      VkDescriptorSetLayout descriptor_set_layout{VK_NULL_HANDLE};
      VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
      VkDeviceMemory device_memory{VK_NULL_HANDLE};
      
      if (valid) {
	// TODO finish here
	//device_memory = make_buffer_memory();
	  
        buffer = make_buffer(block_size);
      }

      if (api_ok() && buffer != VK_NULL_HANDLE) {
	auto binding =
	  make_descriptor_set_layout_binding(binding_index,
					     stage_flags);

	descriptor_set_layout =
	  make_descriptor_set_layout(&binding, 1);
      }

      if (api_ok() && descriptor_set_layout != VK_NULL_HANDLE) {
	descriptor_set = make_descriptor_set(&descriptor_set_layout, 1);
      }

      if (api_ok() && descriptor_set != VK_NULL_HANDLE) {

      }

      return ret;
    }
  };
}
