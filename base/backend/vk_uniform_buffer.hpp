#pragma once

#include "vk_common.hpp"

namespace vulkan {

  class uniform_block_pool {
  public:
    using index_type = int16_t;
    static constexpr index_type k_unset = num_max<index_type>();
    
  private:
    // uniform block data
    darray<uint32_t> m_binding_indices{};
    darray<uint32_t> m_set_indices{};
    darray<VkDeviceSize> m_dev_sizes{};
    darray<uint32_t> m_user_sizes{};
    darray<VkDescriptorSetLayout> m_descriptor_set_layouts{};
    darray<VkDescriptorSet> m_descriptor_sets{};
    darray<VkDeviceMemory> m_device_memories{};
    darray<VkBuffer> m_buffers{};
    darray<void*> m_user_ptrs{};

    device_resource_properties m_resource_props;
    
    index_type push_blank() {
      auto i = m_binding_indices.size();
      
      m_binding_indices.push_back(num_max<uint32_t>());
      m_dev_sizes.push_back(num_max<VkDeviceSize>());
      m_user_sizes.push_back(num_max<uint32_t>());
      m_set_indices.push_back(num_max<uint32_t>());
      
      m_descriptor_set_layouts.push_back(VK_NULL_HANDLE);
      m_descriptor_sets.push_back(VK_NULL_HANDLE);
      m_device_memories.push_back(VK_NULL_HANDLE);
      m_buffers.push_back(VK_NULL_HANDLE);

      m_user_ptrs.push_back(nullptr);

      return i;
    }

    VkBuffer make_uniform_buffer(VkDeviceSize sz) const {
      return vulkan::make_buffer(m_resource_props,
				 0,
				 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				 sz);

    }
    
    bool is_valid(VkDeviceSize desired_size, VkDeviceSize& out_required_size, int32_t& out_property_index) const {
      bool ret = false;

      VkBuffer dummy = make_uniform_buffer(desired_size);
      
      if (api_ok() && dummy != VK_NULL_HANDLE) {
	VkMemoryRequirements req = {};
	
	vkGetBufferMemoryRequirements(m_resource_props.device,
				      dummy,
				      &req);

	out_required_size = req.size;

	VkPhysicalDeviceMemoryProperties mem_props = {};

	vkGetPhysicalDeviceMemoryProperties(m_resource_props.physical_device,
					    &mem_props);
	
	out_property_index =
	  find_memory_properties(&mem_props,
				 req.memoryTypeBits,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	
	ASSERT(out_property_index != -1);

	ret =
	  desired_size <= req.size &&
	  out_property_index != -1;

	vkDestroyBuffer(m_resource_props.device, dummy, nullptr);
      }
      
      ASSERT(ret);
            
      return ret;
    }

    bool ok_index(index_type which) const {
      bool r =
	which != k_unset &&
        which < length();
      ASSERT(r);
      return r;
    }
    
    void free_uniform_block(index_type which) {
      vkDeviceWaitIdle(m_resource_props.device);
      if (ok_index(which)) {

	// in the event that any of these VK handles are null,
	// the corresponding free function will silently do nothing.
	if (api_ok()) {	  
	  vkFreeMemory(m_resource_props.device,
		       m_device_memories[which],
		       nullptr);

	  vkDestroyDescriptorSetLayout(m_resource_props.device,
				       m_descriptor_set_layouts[which],
				       nullptr);

	  vkDestroyBuffer(m_resource_props.device,
			  m_buffers[which],
			  nullptr);
	}

	m_device_memories[which] = VK_NULL_HANDLE;
	m_descriptor_set_layouts[which] = VK_NULL_HANDLE;
	m_buffers[which] = VK_NULL_HANDLE;
	m_descriptor_sets[which] = VK_NULL_HANDLE;
	
	m_user_ptrs[which] = nullptr;
	
	m_binding_indices[which] = num_max<uint32_t>();
	m_dev_sizes[which] = num_max<VkDeviceSize>();
	m_user_sizes[which] = num_max<uint32_t>();
	m_set_indices[which] = num_max<uint32_t>();
      }		       
    }
    
  public:
    uniform_block_pool(device_resource_properties drp)
	: m_resource_props{drp}
	    
    {}

    void free_mem() {
      index_type l = length();
      for (index_type i = 0; i < l; ++i) {
	free_uniform_block(i);
      }
    }
    
    index_type add(void* block_data,
		   uint32_t block_size,
		   uint32_t set,
		   uint32_t binding_index,
		   VkShaderStageFlags stage_flags) {
      index_type ret = k_unset;
      
      int32_t memory_property_index = -1;
      // THIS needs to be queried from is_valid and passed to
      // make_buffer_memory. It's the required size.
      VkDeviceSize required_size = 0; 
      bool valid = is_valid(static_cast<VkDeviceSize>(block_size),
			    required_size,
			    memory_property_index);
      ASSERT(valid);

      VkBuffer ubuffer{VK_NULL_HANDLE};
      VkDescriptorSetLayout descriptor_set_layout{VK_NULL_HANDLE};
      VkDescriptorSet descset{VK_NULL_HANDLE};
      VkDeviceMemory device_memory{VK_NULL_HANDLE};
      
      if (api_ok() && valid) {
	device_memory = make_device_memory(m_resource_props.device,
					   block_data,
					   block_size,
					   required_size,
					   memory_property_index);
      }

      if (api_ok() && device_memory != VK_NULL_HANDLE) {
	ubuffer = make_uniform_buffer(block_size);
      }
      
      bool bound = false;
      if (api_ok() && ubuffer != VK_NULL_HANDLE) {	
	VK_FN(vkBindBufferMemory(m_resource_props.device,
				 ubuffer,
				 device_memory,
				 0));
	bound = api_ok();
      }
      
      if (bound) {
	auto binding =
	  make_descriptor_set_layout_binding(binding_index,
					     stage_flags,
					     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptor_set_layout =
	  make_descriptor_set_layout(m_resource_props.device, &binding, 1);
      }

      if (api_ok() && descriptor_set_layout != VK_NULL_HANDLE) {
	descset =
	  make_descriptor_set(m_resource_props.device,
			      m_resource_props.descriptor_pool,
			      &descriptor_set_layout,
			      1);
      }

      if (api_ok() && descset != VK_NULL_HANDLE) {	
	write_descriptor_set(m_resource_props.device,
			     ubuffer,
			     static_cast<VkDeviceSize>(block_size),
			     descset,
			     binding_index,
			     0,
			     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	
	ret = push_blank();

	m_binding_indices[ret] = binding_index;
	m_set_indices[ret] = set;
	m_dev_sizes[ret] = required_size;
	m_user_sizes[ret] = block_size;
	
	m_descriptor_set_layouts[ret] = descriptor_set_layout;
	m_descriptor_sets[ret] = descset;
	m_device_memories[ret] = device_memory;
	m_buffers[ret] = ubuffer;

	m_user_ptrs[ret] = block_data;
      }
      
      ASSERT(ret != k_unset);

      return ret;
    }

    index_type length() const {
      return static_cast<index_type>(m_binding_indices.size());
    }
    
    VkDescriptorSet descriptor_set(index_type which) const {
      VkDescriptorSet ret{VK_NULL_HANDLE};
      if (ok_index(which)) {
	ret = m_descriptor_sets.at(which);
      }
      return ret;
    }

    VkDescriptorSetLayout descriptor_set_layout(index_type which) const {
      VkDescriptorSetLayout ret{VK_NULL_HANDLE};
      if (ok_index(which)) {
	ret = m_descriptor_set_layouts.at(which);
      }
      return ret;
    }

    VkBuffer buffer(index_type which) const {
      VkBuffer ret{VK_NULL_HANDLE};
      if (ok_index(which)) {
	ret = m_buffers.at(which);
      }
      return ret;
    }

    void update_block(index_type which) const {
      if (ok_index(which)) {
	write_device_memory(m_resource_props.device,
			    m_device_memories.at(which),
			    m_user_ptrs.at(which),
			    static_cast<VkDeviceSize>(m_user_sizes.at(which)));
      }
    }
  };

  template <class dataType>
  struct uniform_block_data {
    dataType data{};
    uniform_block_pool* pool{nullptr};
    uniform_block_pool::index_type index{uniform_block_pool::k_unset};

    bool ok() const {
      bool r =
	index != uniform_block_pool::k_unset &&
	pool != nullptr;
      ASSERT(r);
      return r;
    }

    void cmd_buffer_update(VkCommandBuffer cmd_buffer) {
      if (ok()) {
	VkBuffer ubuffer = pool->buffer(index);

	vkCmdUpdateBuffer(cmd_buffer,
			  ubuffer,
			  0,
			  sizeof(data),
			  reinterpret_cast<void*>(&data));
      }
    }
  };
}
