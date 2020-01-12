#pragma once

#include "vk_common.hpp"

namespace vulkan {
  struct image_gen_params {
    static constexpr std::array<uint32_t, 3> k_max_dim =
      {
       1 << 16,
       1 << 16,
       256
      };    
    
    void* data{nullptr};
    
    VkMemoryPropertyFlags memory_property_flags{0};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageLayout final_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageTiling tiling{VK_IMAGE_TILING_OPTIMAL};
    VkImageUsageFlags usage_flags{0};
    VkImageType type{VK_IMAGE_TYPE_2D};
    VkImageViewType view_type{VK_IMAGE_VIEW_TYPE_2D};
    VkImageAspectFlags aspect_flags{VK_IMAGE_ASPECT_COLOR_BIT};
    
    uint32_t width{UINT32_MAX};
    uint32_t height{UINT32_MAX};
    uint32_t depth{UINT32_MAX};

    bool ok() const {
      bool r =	
	(width <= k_max_dim[0]) &&	
	(height <= k_max_dim[1]) &&
	(depth <= k_max_dim[2]);
	
      ASSERT(r);
      return r;
    }

    uint32_t bpp() const {
      return bpp_from_format(format);
    }

    uint32_t calc_data_size() const {
      ASSERT(type == VK_IMAGE_TYPE_2D);
      uint32_t ret = 0;
      switch (type) {
      case VK_IMAGE_TYPE_2D:
	ret = width * height * bpp();
	break;
      default:
	ASSERT(false);
	break;
      }
      return ret;
    }
  };
  
  class image_pool : index_traits<int16_t, darray<void*>> {
  public:
    typedef index_traits<int16_t, darray<void*>> index_traits_type;
    typedef index_traits_type::index_type index_type;
    static constexpr inline index_type k_unset = index_traits_type::k_unset;
  private:
    darray<void*> m_user_ptrs;
    darray<VkImage> m_images;
    darray<VkImageView> m_image_views;
    darray<VkDeviceMemory> m_device_memories;
    darray<VkFormat> m_formats;
    darray<VkImageLayout> m_layouts_initial;
    darray<VkImageLayout> m_layouts_final;
    darray<uint32_t> m_widths;
    darray<uint32_t> m_heights;
    darray<uint32_t> m_depths; // bpp is derived from format
    darray<VkImageType> m_types;
    darray<VkImageTiling> m_tiling;

    bool image_create_info_valid(VkPhysicalDevice physical_device, const VkImageCreateInfo& create_info) const {
      VkImageFormatProperties properties;

      VK_FN(vkGetPhysicalDeviceImageFormatProperties(physical_device,
						     create_info.format,
						     create_info.imageType,
						     create_info.tiling,
						     create_info.usage,
						     create_info.flags,
						     &properties));

      ASSERT(api_ok());

      return api_ok();
    }

    VkImageCreateInfo make_image_create_info(const device_resource_properties& properties,
					     const image_gen_params& params) const {
      VkImageCreateInfo create_info = {};
      
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      create_info.pNext = nullptr;

      create_info.flags = 0;
      create_info.imageType = params.type;
    
      create_info.format = params.format;
      create_info.initialLayout = params.initial_layout;
      create_info.tiling = params.tiling;
      create_info.usage = params.usage_flags;
    
      create_info.extent.width = params.width;
      create_info.extent.height = params.height;
      create_info.extent.depth = params.depth;

      create_info.mipLevels = 1;
      create_info.arrayLayers = 1;

      create_info.samples = VK_SAMPLE_COUNT_1_BIT;

      create_info.sharingMode = properties.queue_sharing_mode;

      create_info.queueFamilyIndexCount = properties.queue_family_indices.size();
      create_info.pQueueFamilyIndices = properties.queue_family_indices.data();     

      return create_info;
    }

    VkImageViewCreateInfo make_image_view_create_info(const image_gen_params& params) const {
      VkImageViewCreateInfo create_info = {};
      
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.pNext = nullptr;
      create_info.flags = 0;
      create_info.image = VK_NULL_HANDLE;

      create_info.viewType = params.view_type;
      create_info.format = params.format;
    
      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

      create_info.subresourceRange.aspectMask = params.aspect_flags;

      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;

      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;

      return create_info;
    }

    VkExtent3D calc_minimum_dimensions(const image_gen_params& params,
				       const VkMemoryRequirements& requirements) const {
      uint32_t bytes_per_pixel = params.bpp();
      uint32_t width = params.width;
      uint32_t height = params.height;
     
      VkExtent3D ext;

      ext.width = UINT32_MAX;
      ext.height = UINT32_MAX;
      ext.depth = 1;

      auto calc_size =
	[&width, &height, &bytes_per_pixel]() -> VkDeviceSize {
	  return static_cast<VkDeviceSize>(width * height * bytes_per_pixel);
	};
    
      uint32_t flipflop = 0;
    
      while (calc_size() < requirements.size) {
	if (flipflop == 0) {
	  width <<= 1;
	}
	else {
	  height <<= 1;
	}
	flipflop ^= 1;
      }
      
      {
	bool all_pot =
	  is_power_2(requirements.size) &&
	  is_power_2(width) &&
	  is_power_2(height);

	ASSERT((calc_size() == requirements.size && all_pot) || calc_size() >= requirements.size);

	// all we support for now.
	ASSERT(params.type == VK_IMAGE_TYPE_2D && ext.depth == 1);
      }

      ext.width = width;
      ext.height = height; 
    
      return ext;
    }
    
    image_requirements get_image_requirements(const device_resource_properties& properties,
					      const image_gen_params& params) const {
      image_requirements ret{};

      ret.desired.width = params.width;
      ret.desired.height = params.height;
      ret.desired.depth = params.depth;

      ret.bytes_per_pixel = bpp_from_format(params.format);    

      VkImageCreateInfo create_info = make_image_create_info(properties, params);

      if (image_create_info_valid(properties.physical_device,
				  create_info)) {
	
	VkImage dummy_image{VK_NULL_HANDLE};
	
	VK_FN(vkCreateImage(properties.device,
			    &create_info,
			    nullptr,
			    &dummy_image));
    
	if (api_ok() && dummy_image != VK_NULL_HANDLE) {
	  VkMemoryRequirements req = {};

	  vkGetImageMemoryRequirements(properties.device,
				       dummy_image,
				       &req);
      
	  ret.required = calc_minimum_dimensions(params, req);
      
	  // find an appropriate memory type index
	  // for our image that we can base the storage
	  // off of
	  {
	    VkPhysicalDeviceMemoryProperties memory_properties = {};
      
	    vkGetPhysicalDeviceMemoryProperties(properties.physical_device,
						&memory_properties);

	
	    int32_t memory_type_index =
	      find_memory_properties(&memory_properties,
				     req.memoryTypeBits,
				     params.memory_property_flags);
      
	    ASSERT(memory_type_index != -1);

	    ret.memory_type_index =
	      static_cast<uint32_t>(memory_type_index);
	  }

	  vkDestroyImage(properties.device, dummy_image, nullptr);
	}
      }

      ASSERT(ret.ok());
      return ret;
    }
   
    
    index_type new_image() {
      index_type index = this->length();
      
      m_user_ptrs.push_back(nullptr);

      m_images.push_back(VK_NULL_HANDLE);
      m_image_views.push_back(VK_NULL_HANDLE);
      
      m_device_memories.push_back(VK_NULL_HANDLE);
      
      m_formats.push_back(VK_FORMAT_UNDEFINED);

      m_layouts_initial.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
      m_layouts_final.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
      
      m_widths.push_back(UINT32_MAX);
      m_heights.push_back(UINT32_MAX);
      m_depths.push_back(UINT32_MAX);

      m_types.push_back(VK_IMAGE_TYPE_2D);
      m_tiling.push_back(VK_IMAGE_TILING_OPTIMAL);
            
      return index;
    }
    
  public:
    image_pool()
      : index_traits_this_type(m_user_ptrs)
    {}

    bool ok_image(index_type index) const {
      bool r =
	(index < this->length()) &&
	(m_images.at(index) != VK_NULL_HANDLE) &&
	(m_image_views.at(index) != VK_NULL_HANDLE) &&
	(m_device_memories.at(index) != VK_NULL_HANDLE) &&
	(m_formats.at(index) != VK_FORMAT_UNDEFINED) &&
	(m_layouts_final.at(index) != VK_IMAGE_LAYOUT_UNDEFINED) &&
	(m_widths.at(index) != UINT32_MAX) &&
	(m_heights.at(index) != UINT32_MAX) &&
	(m_depths.at(index) != UINT32_MAX);
      ASSERT(r);
      return r;
    }
    
    index_type make_image(const device_resource_properties& properties,
			  const image_gen_params& params) {
      index_type img_index = k_unset;
      
      VkDeviceMemory img_memory{VK_NULL_HANDLE};
      VkImage img_handle{VK_NULL_HANDLE};
      VkImageView img_view_handle{VK_NULL_HANDLE};

      if (properties.ok() && params.ok()) {
	image_requirements reqs = get_image_requirements(properties, params);

	if (reqs.ok()) {	  	  
	  img_memory = make_device_memory(properties.device,
					  params.data,
					  params.calc_data_size(),
					  reqs.memory_size(),
					  reqs.memory_type_index);
	}
	

	if (H_OK(img_memory)) {
	  VkImageCreateInfo create_info = make_image_create_info(properties, params);
	  
	  VK_FN(vkCreateImage(properties.device,
			      &create_info,
			      nullptr,
			      &img_handle));	  
	}

	bool bound = false;
	if (H_OK(img_handle)) {
	  VK_FN(vkBindImageMemory(properties.device,
				  img_handle,
				  img_memory,
				  0));
	
	  bound = api_ok();
	}

	if (bound) {
	  VkImageViewCreateInfo create_info = make_image_view_create_info(params);
	  
	  create_info.image = img_handle;

	  VK_FN(vkCreateImageView(properties.device,
				  &create_info,
				  nullptr,
				  &img_view_handle));
	}

	if (H_OK(img_view_handle)) {
	  img_index = new_image();

	  m_user_ptrs[img_index] = params.data;
	  
	  m_device_memories[img_index] = img_memory;
	  m_images[img_index] = img_handle;
	  m_image_views[img_index] = img_view_handle;

	  m_formats[img_index] = params.format;
	  m_layouts_initial[img_index] = params.initial_layout;
	  m_layouts_final[img_index] = params.final_layout;
	  
	  m_types[img_index] = params.type;
	  m_tiling[img_index] = params.tiling;

	  m_widths[img_index] = params.width;
	  m_heights[img_index] = params.height;
	  m_depths[img_index] = params.depth;
	}
      }
      ASSERT(ok_image(img_index));
      return img_index;
    }

    void free_mem(VkDevice device) {
      for (index_type index{0}; index < length(); ++index) {
	if (ok_image(index)) {
	  free_device_handle<VkImageView, &vkDestroyImageView>(device, m_image_views[index]);
	  free_device_handle<VkImage, &vkDestroyImage>(device, m_images[index]);
	  free_device_handle<VkDeviceMemory, &vkFreeMemory>(device, m_device_memories[index]);
	}
      }

      m_user_ptrs.clear();
      m_images.clear();
      m_image_views.clear();
      m_device_memories.clear();
      m_formats.clear();
      m_layouts_initial.clear();
      m_layouts_final.clear();
      m_widths.clear();
      m_heights.clear();
      m_depths.clear();
      m_types.clear();
      m_tiling.clear();
    }

    VkImage image(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_image,
			    m_images,
			    VkImage);
    }

    VkImageLayout layout_final(index_type index) const {
      HANDLE_GET_FN_IMPL(index,
			 ok_image,
			 m_layouts_final,
			 VkImageLayout,
			 VK_IMAGE_LAYOUT_UNDEFINED);
    }

    VkImageView image_view(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_image,
			    m_image_views,
			    VkImageView);
    }
    
  };
  
  struct texture_gen_params {
    image_gen_params image_params{};

    uint32_t descriptor_array_element{UINT32_MAX};
    uint32_t binding_index{UINT32_MAX};
    VkDescriptorType descriptor_type{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}; 
    VkShaderStageFlags shader_stage_flags{VK_SHADER_STAGE_FRAGMENT_BIT};   

    bool ok() const {
      bool r =
	image_params.ok() &&
	descriptor_array_element != UINT32_MAX &&
	binding_index != UINT32_MAX;      
	
      ASSERT(r);
      return r;
    }
  };
  
  class texture_pool : index_traits<int16_t,
				    darray<image_pool::index_type>> {
  public:
    typedef index_traits<int16_t,
			 darray<image_pool::index_type>> index_type_traits;

    typedef index_type_traits::index_type index_type;
    static inline constexpr index_type k_unset = index_type_traits::k_unset;

  private:
    darray<image_pool::index_type> m_images;
    darray<VkSampler> m_samplers;
    darray<VkDescriptorSet> m_descriptor_sets;
    darray<VkDescriptorSetLayout> m_descriptor_set_layouts;
    darray<uint32_t> m_desc_layout_binding_indices;
    darray<uint32_t> m_desc_array_element_indices;
    darray<VkDescriptorType> m_descriptor_types;
    
    image_pool* m_image_pool;

    VkSampler make_sampler(const device_resource_properties& properties,
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
    
    index_type new_texture() {
      index_type index = this->length();
      
      m_images.push_back(image_pool::k_unset);
      m_samplers.push_back(VK_NULL_HANDLE);
      m_descriptor_set_layouts.push_back(VK_NULL_HANDLE);
      m_descriptor_sets.push_back(VK_NULL_HANDLE);
      m_desc_layout_binding_indices.push_back(UINT32_MAX);
      m_desc_array_element_indices.push_back(UINT32_MAX);
      m_descriptor_types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

      return index;
    }
    
  public:
    texture_pool()
      : index_traits_this_type(m_images),
	m_image_pool{nullptr}
    {}

    void set_image_pool(image_pool* p) {
      // should only be set once
      ASSERT(m_image_pool == nullptr);
      m_image_pool = p;
    }
    
    index_type make_texture(const device_resource_properties& properties,
			    const texture_gen_params& params) {
      index_type texture{k_unset};

      if (m_image_pool != nullptr &&
	  properties.ok() &&
	  params.ok()) {
	
	image_pool::index_type img{image_pool::k_unset};
	VkSampler smp{VK_NULL_HANDLE};
	VkDescriptorSetLayout desc_set_layout{VK_NULL_HANDLE};
	VkDescriptorSet desc_set{VK_NULL_HANDLE};

	img =
	  m_image_pool->make_image(properties,
					 params.image_params);
	
	if (m_image_pool->ok_image(img)) {	
	  smp = make_sampler(properties, params);
	}

	if (H_OK(smp)) {
	  auto desc_set_binding = make_descriptor_set_layout_binding(params.binding_index,
								     params.shader_stage_flags,
								     params.descriptor_type);

	  
	  desc_set_layout = make_descriptor_set_layout(properties.device,
						       &desc_set_binding,
						       1);
	}

	if (H_OK(desc_set_layout)) {
	  desc_set = make_descriptor_set(properties.device,
					 properties.descriptor_pool,
					 &desc_set_layout,
					 1);
	}

	if (H_OK(desc_set)) {
	  texture = new_texture();
	  
	  m_images[texture] = img;
	  m_samplers[texture] = smp;
	  m_descriptor_set_layouts[texture] = desc_set_layout;
	  m_descriptor_sets[texture] = desc_set;

	  m_desc_layout_binding_indices[texture] = params.binding_index;
	  m_desc_array_element_indices[texture] = params.descriptor_array_element;
	  m_descriptor_types[texture] = params.descriptor_type;
	}
      }
      ASSERT(ok_texture(texture));
      return texture;
    }

    void free_mem(VkDevice device) {
      for (index_type index{0}; index < length(); ++index) {
	free_device_handle<VkSampler, &vkDestroySampler>(device, m_samplers[index]);
	// descriptor set layout should be freed by pipeline layout
	// descriptor set should be freed by pool
      }

      m_images.clear();
      m_samplers.clear();
      m_descriptor_set_layouts.clear();
      m_descriptor_sets.clear();
      m_desc_layout_binding_indices.clear();
      m_desc_array_element_indices.clear();
      m_descriptor_types.clear();
      
      if (m_image_pool != nullptr) {
	m_image_pool->free_mem(device);
      }
    }

    bool ok_texture(index_type index) const {
      bool r =
	this->ok_index(index) &&
	(m_image_pool != nullptr) &&
	m_image_pool->ok_image(m_images.at(index)) &&
	(m_samplers.at(index) != VK_NULL_HANDLE) &&
	(m_descriptor_set_layouts.at(index) != VK_NULL_HANDLE) &&
	(m_descriptor_sets.at(index) != VK_NULL_HANDLE) &&
	(m_desc_layout_binding_indices.at(index) != UINT32_MAX) &&
	(m_desc_array_element_indices.at(index) != UINT32_MAX);

      ASSERT(r);
      return r;
    }

    image_pool::index_type image_index(index_type index) const {
      HANDLE_GET_FN_IMPL(index,
			 ok_texture,
			 m_images,
			 image_pool::index_type,
			 image_pool::k_unset);
    }
    
    VkSampler sampler(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_texture,
			    m_samplers,
			    VkSampler);
    }
    
    VkDescriptorSet descriptor_set(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_texture,
			    m_descriptor_sets,
			    VkDescriptorSet);
    }

    VkDescriptorSetLayout descriptor_set_layout(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_texture,
			    m_descriptor_set_layouts,
			    VkDescriptorSetLayout);
    }

    VkDescriptorImageInfo make_descriptor_image_info(index_type index) const {
      VkDescriptorImageInfo ret = {};
      ASSERT(m_image_pool != nullptr);
      if (ok_texture(index)) {
	ret.sampler = m_samplers.at(index);
	ret.imageView = m_image_pool->image_view(m_images.at(index));
	ret.imageLayout = m_image_pool->layout_final(m_images.at(index));
      }
      return ret;
    }

    VkWriteDescriptorSet make_write_descriptor_set(index_type index, const VkDescriptorImageInfo* image_info) const {
      VkWriteDescriptorSet ret = {};
      if (ok_texture(index)) {
	ret.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	ret.pNext = nullptr;
	ret.dstSet = m_descriptor_sets.at(index);
	ret.dstBinding = m_desc_layout_binding_indices.at(index);
	ret.dstArrayElement = m_desc_array_element_indices.at(index);
	ret.descriptorCount = 1;
	ret.descriptorType = m_descriptor_types.at(index);
	ret.pImageInfo = image_info;
	ret.pBufferInfo = nullptr;
	ret.pTexelBufferView = nullptr;
      }
      return ret;
    }
  };
}
