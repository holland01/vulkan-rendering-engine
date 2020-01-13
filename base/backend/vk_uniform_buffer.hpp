#pragma once

#include "vk_common.hpp"

namespace vulkan {

  struct uniform_block_gen_params {
    descriptor_set_pool::index_type descriptor_set_index{descriptor_set_pool::k_unset};
    
    void* block_data{nullptr};
    
    uint32_t block_size{UINT32_MAX};
    uint32_t array_element_index{UINT32_MAX};
    uint32_t binding_index{UINT32_MAX};

    bool ok() const {
      bool r =
	(descriptor_set_index != descriptor_set_pool::k_unset) &&
	(block_data != nullptr) &&
	(block_size != UINT32_MAX) &&
	(array_element_index != UINT32_MAX) &&
	(binding_index != UINT32_MAX);
      ASSERT(r);
      return r;
    }    
  };
  
  class uniform_block_pool : index_traits<int16_t, darray<VkDeviceSize>> {
  public:
    typedef index_traits_this_type::index_type index_type;
    static inline constexpr index_type k_unset = index_traits_this_type::k_unset;
    
  private:
    // uniform block data
    darray<VkDeviceSize> m_dev_sizes{};
    darray<uint32_t> m_user_sizes{};
    darray<VkDeviceMemory> m_device_memories{};
    darray<VkBuffer> m_buffers{};
    darray<void*> m_user_ptrs{};

    descriptor_set_pool* m_descriptor_set_pool{nullptr};
    
    index_type new_uniform_block() {
      index_type i{this->length()};
      
      m_dev_sizes.push_back(num_max<VkDeviceSize>());
      m_user_sizes.push_back(UINT32_MAX);      
      
      m_device_memories.push_back(VK_NULL_HANDLE);
      m_buffers.push_back(VK_NULL_HANDLE);

      m_user_ptrs.push_back(nullptr);

      return i;
    }

    VkBuffer make_uniform_buffer(const device_resource_properties& resource_props,
				 VkDeviceSize sz) const {
      
      return vulkan::make_buffer(resource_props,
				 0,
				 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				 sz);

    }
    
    bool is_valid(const device_resource_properties& resource_props,
		  VkDeviceSize desired_size,
		  VkDeviceSize& out_required_size,
		  int32_t& out_property_index) const {
      
      bool ret = false;

      VkBuffer dummy = make_uniform_buffer(resource_props,
					   desired_size);
      
      if (H_OK(dummy)) {
	VkMemoryRequirements req = {};
	
	vkGetBufferMemoryRequirements(resource_props.device,
				      dummy,
				      &req);

	out_required_size = req.size;

	VkPhysicalDeviceMemoryProperties mem_props = {};

	vkGetPhysicalDeviceMemoryProperties(resource_props.physical_device,
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

	vkDestroyBuffer(resource_props.device, dummy, nullptr);
      }
      
      ASSERT(ret);
            
      return ret;
    }
    
  public:
    uniform_block_pool()
      : index_traits_this_type(m_dev_sizes)	    
    {}

    void free_mem(VkDevice device) {
      for (index_type i{0}; i < this->length(); ++i) {
	free_device_handle<VkDeviceMemory, &vkFreeMemory>(device,
							  m_device_memories[i]);

	free_device_handle<VkBuffer, &vkDestroyBuffer>(device,
						       m_buffers[i]);
      }

      m_device_memories.clear();
      m_buffers.clear();
      m_user_ptrs.clear();
      m_dev_sizes.clear();
      m_user_sizes.clear();
    }

    void set_descriptor_set_pool(descriptor_set_pool* p) {
      if (c_assert(m_descriptor_set_pool == nullptr)) {
	m_descriptor_set_pool = p;
      }
    }
    
    index_type make_uniform_block(const device_resource_properties& properties,
				  const uniform_block_gen_params& params)  {
      index_type ret{k_unset};

      if (c_assert(m_descriptor_set_pool != nullptr) &&
	  properties.ok() &&
	  params.ok() &&
	  m_descriptor_set_pool->ok_descriptor_set(params.descriptor_set_index)) {
	int32_t memory_property_index = -1;
	// THIS needs to be queried from is_valid and passed to
	// make_buffer_memory. It's the required size.
	VkDeviceSize required_size = 0;
	
	bool valid = is_valid(properties,
			      static_cast<VkDeviceSize>(params.block_size),
			      required_size,
			      memory_property_index);

	if (c_assert(valid)) {
	  VkBuffer ubuffer{VK_NULL_HANDLE};
	  VkDeviceMemory device_memory{VK_NULL_HANDLE};
      
	  if (api_ok()) {
	    device_memory = make_device_memory(properties.device,
					       params.block_data,
					       params.block_size,
					       required_size,
					       memory_property_index);
	  }

	  if (H_OK(device_memory)) {
	    ubuffer = make_uniform_buffer(properties,
					  params.block_size);
	  }
      
	  bool bound = false;
	  if (H_OK(ubuffer)) {	
	    VK_FN(vkBindBufferMemory(properties.device,
				     ubuffer,
				     device_memory,
				     0));
	    bound = api_ok();
	  }      

	  if (bound) {
	    
	    bool write_result = m_descriptor_set_pool->write_buffer(params.descriptor_set_index,
								    properties.device,
								    ubuffer,
								    static_cast<VkDeviceSize>(params.block_size),
								    params.binding_index,
								    params.array_element_index);

	    if (c_assert(write_result)) {							    
	      ret = new_uniform_block();

	      m_dev_sizes[ret] = required_size;
	      m_user_sizes[ret] = params.block_size;
	
	      m_device_memories[ret] = device_memory;
	      m_buffers[ret] = ubuffer;

	      m_user_ptrs[ret] = params.block_data;
	    }
	  }
	}
      }
      
      ASSERT(ret != k_unset);

      return ret;
    }
        
    VkBuffer buffer(index_type which) const {
      VkBuffer ret{VK_NULL_HANDLE};
      if (ok_index(which)) {
	ret = m_buffers.at(which);
      }
      return ret;
    }

    void update_block(index_type which, VkDevice device) const {
      if (ok_index(which)) {
	write_device_memory(device,
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

    bool cmd_buffer_update(VkCommandBuffer cmd_buffer) const {
      bool r = ok();
      if (r) {
	VkBuffer ubuffer = pool->buffer(index);

	vkCmdUpdateBuffer(cmd_buffer,
			  ubuffer,
			  0,
			  sizeof(data),
			  reinterpret_cast<void*>(&data));
      }
      return r;
    }
  };
}
