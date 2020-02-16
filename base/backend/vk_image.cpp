#include "vk_image.hpp"
#include <unordered_set>

namespace vulkan {
    // at the time of this writing texture_gen_params isn't necessary,
  // but it will be in the future.
  VkSampler texture_pool::make_sampler(const device_resource_properties& properties,
				       const texture_gen_params& params) const {
    VkSampler sampler{VK_NULL_HANDLE};

    VkSamplerCreateInfo create_info = {};

    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.pNext = nullptr;
    
    create_info.flags = 0;
    
    create_info.magFilter = VK_FILTER_LINEAR;
    create_info.minFilter = VK_FILTER_LINEAR;

    create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    create_info.mipLodBias = 0.0f;
    
    create_info.anisotropyEnable = VK_FALSE;
    create_info.maxAnisotropy = 0.0f;

    create_info.compareEnable = VK_FALSE;
    create_info.compareOp = VK_COMPARE_OP_ALWAYS;
    create_info.minLod = 0.0f;
    create_info.maxLod = 0.0f;
    
    create_info.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;

    create_info.unnormalizedCoordinates = VK_FALSE;

    VK_FN(vkCreateSampler(properties.device,
			  &create_info,
			  nullptr,
			  &sampler));
    return sampler;
  }
    
  texture_pool::index_type texture_pool::new_texture() {
    texture_pool::index_type index = this->length();
      
    m_images.push_back(image_pool::k_unset);
    m_samplers.push_back(VK_NULL_HANDLE);

    m_descriptor_sets.push_back(descriptor_set_pool::k_unset);

    m_desc_layout_binding_indices.push_back(UINT32_MAX);
    m_desc_array_element_indices.push_back(UINT32_MAX);

    return index;
  }
    
  texture_pool::texture_pool()
    : index_traits_this_type(m_images),
      m_image_pool{nullptr},
      m_descriptor_set_pool{nullptr}
  {}

  void texture_pool::set_image_pool(image_pool* p) {
    // should only be set once
    ASSERT(m_image_pool == nullptr);
    m_image_pool = p;
  }

  void texture_pool::set_descriptor_set_pool(descriptor_set_pool* p) {
    ASSERT(m_descriptor_set_pool == nullptr);
    m_descriptor_set_pool = p;
  }
    
  texture_pool::index_type texture_pool::make_texture(const device_resource_properties& properties,
						      const texture_gen_params& params) {
    texture_pool::index_type texture{k_unset};

    if (m_image_pool != nullptr &&
	m_descriptor_set_pool != nullptr &&
	properties.ok() &&
	params.ok()) {

      if (m_descriptor_set_pool->ok_descriptor_set(params.descriptor_set_index)) {
	if (m_image_pool->ok_image(params.image_index)) {
	  image_pool::index_type img{image_pool::k_unset};
	  VkSampler smp{VK_NULL_HANDLE};     

	  smp = make_sampler(properties, params);

	  if (H_OK(smp)) {
	    texture = new_texture();
	  
	    m_images[texture] = params.image_index;
	    m_samplers[texture] = smp;

	    m_desc_layout_binding_indices[texture] = params.binding_index;
	    m_desc_array_element_indices[texture] = params.descriptor_array_element;	
		
	    m_descriptor_sets[texture] = params.descriptor_set_index;
	  }
	}
      }
    }
    ASSERT(ok_texture(texture));
    return texture;
  }

  void texture_pool::free_mem(VkDevice device) {
    for (texture_pool::index_type index{0}; index < length(); ++index) {
      free_device_handle<VkSampler, &vkDestroySampler>(device, m_samplers[index]);
    }

    m_images.clear();
    m_samplers.clear();

    m_descriptor_sets.clear();
      
    m_desc_layout_binding_indices.clear();      
    m_desc_array_element_indices.clear();            
  }

  bool texture_pool::ok_texture(texture_pool::index_type index) const {
    bool r =
      this->ok_index(index) &&

      (m_image_pool != nullptr) &&
      (m_image_pool->ok_image(m_images.at(index))) &&

      (m_descriptor_set_pool != nullptr) &&
      (m_descriptor_set_pool->ok_descriptor_set(m_descriptor_sets.at(index))) &&

      (m_samplers.at(index) != VK_NULL_HANDLE) &&
      
      (m_desc_layout_binding_indices.at(index) != UINT32_MAX) &&
      (m_desc_array_element_indices.at(index) != UINT32_MAX);

    ASSERT(r);
    return r;
  }
    
  VkSampler texture_pool::sampler(texture_pool::index_type index) const {
    VK_HANDLE_GET_FN_IMPL(index,
			  ok_texture,
			  m_samplers,
			  VkSampler);
  }
    
  VkDescriptorImageInfo texture_pool::make_descriptor_image_info(texture_pool::index_type index) const {
    VkDescriptorImageInfo ret = {};
    ASSERT(m_image_pool != nullptr);
    if (ok_texture(index)) {
      ret.sampler = m_samplers.at(index);
      ret.imageView = m_image_pool->image_view(m_images.at(index));
      ret.imageLayout = m_image_pool->layout_final(m_images.at(index));
    }
    return ret;
  }

  VkWriteDescriptorSet texture_pool::make_write_descriptor_set(texture_pool::index_type index, const VkDescriptorImageInfo* image_info) const {
    VkWriteDescriptorSet ret = {};
    if (ok_texture(index)) {
      ret.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      ret.pNext = nullptr;

      ret.dstSet = m_descriptor_set_pool->descriptor_set(m_descriptor_sets.at(index));

      ret.dstBinding = m_desc_layout_binding_indices.at(index);
      ret.dstArrayElement = m_desc_array_element_indices.at(index);

      ret.descriptorCount = 1;
      ret.descriptorType = m_descriptor_set_pool->descriptor_type(m_descriptor_sets.at(index));
      
      ret.pImageInfo = image_info;
      ret.pBufferInfo = nullptr;
      ret.pTexelBufferView = nullptr;
    }
    return ret;
  }

  bool texture_pool::update_descriptor_sets(VkDevice device, darray<texture_pool::index_type> indices) const {
    bool good = c_assert(m_image_pool != nullptr && !indices.empty());

    // lots of validation here.
    // we make sure that:
    //    a) each index is valid, and
    //    b) that each index refers to the same descriptor set, and
    //    c) that each index refers to the same descriptor set layout binding.
    // Then, we generate the info as required.
    if (good) {
      size_t i{0};

      std::unordered_set<descriptor_set_pool::index_type> desc_indices;
      std::unordered_set<uint32_t> binding_indices;
      
      while (i < indices.size() && good) {
	good = ok_texture(indices[i]);

	desc_indices.insert(m_descriptor_sets.at(indices[i]));
	binding_indices.insert(m_desc_layout_binding_indices.at(indices[i]));
	
	i++;
      }

      good =
	good &&
	desc_indices.size() == 1 &&
	binding_indices.size() == 1;
    
      if (c_assert(good)) {
	darray<VkDescriptorImageInfo> v_image_info{};

	i = 0;
	while (i < indices.size()) {
	  texture_pool::index_type index = indices.at(i);
	  
	  VkDescriptorImageInfo _image_info = {};
	  
	  _image_info.sampler = m_samplers.at(index);
	  _image_info.imageView = m_image_pool->image_view(m_images.at(index));
	  _image_info.imageLayout = m_image_pool->layout_final(m_images.at(index));

	  v_image_info.push_back(_image_info);

	  i++;
	}

	// it doesn't matter which index we use here.
	texture_pool::index_type index = indices.at(0);

	// important that the first image is in fact referencing
	// sampler 0 (yes, the 'good &&' is unnecessary, but at the same time
	// a new dependency might be added at some point)

	good =
	  good && 
	  m_desc_array_element_indices.at(index) == 0;

	if (c_assert(good)) {
	  VkWriteDescriptorSet write_desc_set = {};
	
	  write_desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	  write_desc_set.pNext = nullptr;

	  write_desc_set.dstSet = m_descriptor_set_pool->descriptor_set(m_descriptor_sets.at(index));
	  write_desc_set.descriptorType = m_descriptor_set_pool->descriptor_type(m_descriptor_sets.at(index));
	  
	  write_desc_set.dstBinding = 0;
	  write_desc_set.dstArrayElement = 0;
	
	  write_desc_set.descriptorCount = v_image_info.size();
	  write_desc_set.pImageInfo = v_image_info.data();
	
	  write_desc_set.pBufferInfo = nullptr;
	  write_desc_set.pTexelBufferView = nullptr;

	  vkUpdateDescriptorSets(device,
				 1,
				 &write_desc_set,
				 0,
				 nullptr);
	}
      }
    }
    return good;
  }
}


