#pragma once

#include "vk_common.hpp"
#include <iostream>

namespace vulkan {
  static inline bool validate_attachment(VkImageUsageFlags usage_flags,
					 VkImageLayout attachment_layout) {
    return ((is_image_usage_attachment(usage_flags) &&
	     !c_in(attachment_layout,
		   k_invalid_attachment_layouts))

	    ||


	    (!is_image_usage_attachment(usage_flags) &&
	     attachment_layout == VK_IMAGE_LAYOUT_UNDEFINED)

	    ||

	    (is_image_usage_attachment(usage_flags) &&
	     attachment_layout == VK_IMAGE_LAYOUT_UNDEFINED)

	    );

  }
  
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

    VkImageLayout attachment_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageLayout final_layout{VK_IMAGE_LAYOUT_UNDEFINED};

    VkImageTiling tiling{VK_IMAGE_TILING_OPTIMAL};
    VkImageUsageFlags usage_flags{0};
    VkImageType type{VK_IMAGE_TYPE_2D};
    VkImageViewType view_type{VK_IMAGE_VIEW_TYPE_2D};
    VkImageAspectFlags aspect_flags{VK_IMAGE_ASPECT_COLOR_BIT};

    VkPipelineStageFlags source_pipeline_stage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    VkPipelineStageFlags dest_pipeline_stage{VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    VkAccessFlags source_access_flags{0};
    VkAccessFlags dest_access_flags{VK_ACCESS_SHADER_READ_BIT};
        
    uint32_t width{UINT32_MAX};
    uint32_t height{UINT32_MAX};
    uint32_t depth{UINT32_MAX};
    
    bool ok() const {
      bool r =	
	(width <= k_max_dim[0]) &&
	(height <= k_max_dim[1]) &&
	(depth <= k_max_dim[2]) &&
	validate_attachment(usage_flags,
			    attachment_layout);
		
      ASSERT(r);
      return r;
    }

    // see documentation @ declaration for st_config flag below
    // for more information
    bool needs_staging_convert() const {
      return
	st_config::c_image_pool::m_make_image::k_always_produce_optimal_images && 
	tiling == VK_IMAGE_TILING_LINEAR &&
	initial_layout == VK_IMAGE_LAYOUT_PREINITIALIZED;
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
    struct make_image_data {
      VkDeviceMemory memory{VK_NULL_HANDLE};
      VkImage handle{VK_NULL_HANDLE};
      VkImageView view_handle{VK_NULL_HANDLE};
      bool memory_bound{false};
      mutable bool pre{false};

      const image_pool* self{nullptr};
      const device_resource_properties* properties{nullptr};
      const image_gen_params* params{nullptr};
      
      bool ok_pre() const {
	if (!pre) {
	  pre = 
	    c_assert(self != nullptr) &&
	    c_assert(properties != nullptr) &&
	    c_assert(params != nullptr) &&
	    c_assert(properties->ok()) &&
	    c_assert(params->ok());
	}
	return pre;
      }

      make_image_data& set_pre(const image_pool* self,
		   const device_resource_properties* properties,
			       const image_gen_params* params) {
	this->self = self;
	this->properties = properties;
	this->params = params;
	
	return *this;
      }

      bool ok_memory() const {
	return c_assert(H_OK(memory));
      }

      bool ok_handle() const {
	return c_assert(H_OK(handle));
      }

      bool ok_view_handle() const {
	return c_assert(H_OK(view_handle));
      }
      
      make_image_data& make_image_memory() {
	if (ok_pre() && CA_H_NULL(memory)) {
	  image_requirements reqs = self->get_image_requirements(*properties, *params);

	  if (reqs.ok()) {	  	  
	    memory = make_device_memory(properties->device,
					params->data,
					params->calc_data_size(),
					reqs.memory_size(),
					reqs.memory_type_index);
	  }

	}

	return *this;
      }

      make_image_data& create_image() {
	if (ok_pre() && ok_memory() && CA_H_NULL(handle)) {
	  VkImageCreateInfo create_info = self->make_image_create_info(*properties, *params);
	  
	  VK_FN(vkCreateImage(properties->device,
			      &create_info,
			      nullptr,
			      &handle));
	}
	return *this;
      }
      
      make_image_data& bind_image_memory() {
	if (ok_pre() && !memory_bound && ok_handle()) {
	  VK_FN(vkBindImageMemory(properties->device,
				  handle,
				  memory,
				  0));
	  memory_bound = api_ok();
	}
	return *this;
      }

      make_image_data& create_image_view() {
	if (ok_pre() && memory_bound && CA_H_NULL(view_handle)) {
	  VkImageViewCreateInfo create_info =
	    self->make_image_view_create_info(*params);
	  
	  create_info.image = handle;

	  VK_FN(vkCreateImageView(properties->device,
				  &create_info,
				  nullptr,
				  &view_handle));
	}
	return *this;
      }
      

      make_image_data& all() {	
	return
	  make_image_memory()
	  .create_image()
	  .bind_image_memory()
	  .create_image_view();
      }
      
      image_layout_transition make_layout_transition() const {
	auto ret = image_layout_transition(false);

	if (ok()) {
	  ret
	    .from_stage(params->source_pipeline_stage)
	    .to_stage(params->dest_pipeline_stage)
	    .for_aspect(params->aspect_flags)
	    .from_access(params->source_access_flags)
	    .to_access(params->dest_access_flags)
	    .from(params->initial_layout)
	    .to(params->final_layout)
	    .for_image(handle)
	    .ready()
	    ;
	}
	
	return ret;
      }

      
      void free_device_mem() {
	free_device_handle<VkImageView,
			   &vkDestroyImageView>(properties->device,
						view_handle);

	free_device_handle<VkImage,
			   &vkDestroyImage>(properties->device,
					    handle);
	
	free_device_handle<VkDeviceMemory,
			   &vkFreeMemory>(properties->device,
					  memory);
      }     
      
      bool ok() const {
	return
	  ok_memory() &&
	  ok_handle() &&
	  ok_view_handle();
      }
    };

  private:
    darray<void*> m_user_ptrs;
    darray<VkImage> m_images;
    darray<VkImageView> m_image_views;
    darray<VkDeviceMemory> m_device_memories;
    darray<VkFormat> m_formats;
    darray<VkImageLayout> m_layouts_initial;
    darray<VkImageLayout> m_layouts_final;
    darray<VkImageLayout> m_layouts_attach_opt;
    darray<uint32_t> m_widths;
    darray<uint32_t> m_heights;
    darray<uint32_t> m_depths; // bpp is derived from format
    darray<VkImageType> m_types;
    darray<VkImageTiling> m_tiling;
    // layout transition info
    darray<VkImageAspectFlags> m_aspect_flags;

    darray<VkPipelineStageFlags> m_src_pipeline_stages;
    darray<VkPipelineStageFlags> m_dst_pipeline_stages;

    darray<VkAccessFlags> m_src_access_flags;
    darray<VkAccessFlags> m_dst_access_flags;

    darray<VkImageUsageFlags> m_usage_flags;

    // We will not call vkGetPhysicalDeviceImageFormatProperties()
    // if g_vk_result is not equal to VK_SUCCESS beforehand. We may as well
    // consider this a false result, given that at that point something
    // else that's wrong has happened.
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
      
      m_layouts_attach_opt.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
      
      m_widths.push_back(UINT32_MAX);
      m_heights.push_back(UINT32_MAX);
      m_depths.push_back(UINT32_MAX);

      m_types.push_back(VK_IMAGE_TYPE_2D);
      m_tiling.push_back(VK_IMAGE_TILING_OPTIMAL);

      m_aspect_flags.push_back(VK_IMAGE_ASPECT_COLOR_BIT);

      m_src_pipeline_stages.push_back(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
      m_dst_pipeline_stages.push_back(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
      
      m_src_access_flags.push_back(0);
      m_dst_access_flags.push_back(VK_ACCESS_SHADER_READ_BIT);

      m_usage_flags.push_back(0);
            
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
	(m_depths.at(index) != UINT32_MAX) &&
	(m_usage_flags.at(index) != 0) &&
	validate_attachment(m_usage_flags.at(index),
			    m_layouts_attach_opt.at(index));
      
      ASSERT(r);
      return r;
    }

    index_type make_image(const device_resource_properties& properties,
			  const image_gen_params& params) {
      index_type img_index = k_unset;
      
      make_image_data mid{};

      // unless the image parameters are a sepcific configuration,
      // we won't use this
      make_image_data mid_conv{};

      // this is just a helper ptr since we may be using a different
      // make_image_data instance, as the above comment implies.
      make_image_data* mid_used = nullptr;

      // overall flag which determines success
      bool good{false};

      // these may not be used, but if they are,
      // they need to be in scope throughout the lifetime
      // of the method call. they're used if
      // params.needs_staging_convert() is true.
      image_gen_params cparams_src{params};
      image_gen_params cparams_dest{params};
      
      // here we check to see if we're going to optimize the image
      // by taking the data that the user specified and altering the layout
      // parameters. in order to do this, we use the initial image as a staging
      // image, and transfer the memory from that over. Then we free the
      // the "mid" initialized above, since we no longer need it.
      if (params.needs_staging_convert()) {       	
	// it's probable that the original params
	// don't have the proper image usage flags;

	cparams_src.usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	  
	mid
	  .set_pre(this, &properties, &cparams_src)
	  .all();
	
	// create dst image
	// we null out the data ptr since ::make_device_memory()
	// checks for a non-null value, and if it is non-null,
	// writes the memory to the newly created memory - this is unnecessary.
	// also we use device local memory here; this is very important.
	cparams_dest.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	cparams_dest.final_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	cparams_dest.tiling = VK_IMAGE_TILING_OPTIMAL;
	cparams_dest.usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	cparams_dest.data = nullptr;
	cparams_dest.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	mid_conv
	  .set_pre(this, &properties, &cparams_dest)
	  .all();
	
	good =
	  c_assert(mid.ok()) &&
	  c_assert(mid_conv.ok());

	if (good) {
	  // this is a synchronous call;
	  // generate a one time submit command buffer
	  // and then create two image layout transitions
	  // so that we can perform a copy from the temporary image
	  // to the main image whose memory is strictly device local.	  
	  one_shot_command_buffer(properties,
				  [this,
				   &mid_conv,
				   &mid,
				   &good](VkCommandBuffer cmd_buf) {
				    auto ilt = mid.make_layout_transition();
				    
				    if (ilt.ok()) {
				      ilt.via(cmd_buf);
				      
				      auto ilt_conv = mid_conv.make_layout_transition();

				      if (ilt_conv.ok()) {
					ilt_conv.via(cmd_buf);
					
					VkImageCopy region{};

					VkImageSubresourceLayers subresource{};
					subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					subresource.mipLevel = 0;
					subresource.baseArrayLayer = 0;
					subresource.layerCount = 1;
				    
					region.srcSubresource = subresource;
					region.srcOffset.x = 0;
					region.srcOffset.y = 0;
					region.srcOffset.z = 0;

					region.dstSubresource = subresource;
					region.dstOffset.x = 0;
					region.dstOffset.y = 0;
					region.dstOffset.z = 0;

					region.extent.width = mid.params->width;
					region.extent.height = mid.params->height;
					region.extent.depth = mid.params->depth;
				    
					vkCmdCopyImage(cmd_buf,
						       mid.handle,
						       mid.params->final_layout,
						       mid_conv.handle,
						       mid_conv.params->final_layout,
						       1,
						       &region);
				      }
				    }						   
				  },
				  [this, &good](one_shot_command_error err) {
				    good = false;
				  });

	  if (good) {
	    mid_used = &mid_conv;
	  }
	}

	// this is no longer needed,
	// so we free it here.
	mid.free_device_mem();
      }
      else {
	mid
	  .set_pre(this, &properties, &params)
	  .all();

	good =
	  c_assert(mid.ok());

	if (good) {
	  mid_used = &mid;
	}
      }
	      
      if (good) {
	ASSERT(mid_used != nullptr);
	
	img_index = new_image();

	m_user_ptrs[img_index] = params.data;
	  
	m_device_memories[img_index] = mid_used->memory;
	m_images[img_index] = mid_used->handle;
	m_image_views[img_index] = mid_used->view_handle;

	m_layouts_initial[img_index] = mid_used->params->initial_layout;
	m_layouts_final[img_index] = mid_used->params->final_layout;
	m_layouts_attach_opt[img_index] = mid_used->params->attachment_layout;
	  
	m_formats[img_index] = mid_used->params->format;
	  
	m_widths[img_index] = mid_used->params->width;
	m_heights[img_index] = mid_used->params->height;
	m_depths[img_index] = mid_used->params->depth;
	  
	m_types[img_index] = mid_used->params->type;
	m_tiling[img_index] = mid_used->params->tiling;

	m_aspect_flags[img_index] = mid_used->params->aspect_flags;

	m_src_pipeline_stages[img_index] = mid_used->params->source_pipeline_stage;
	m_dst_pipeline_stages[img_index] = mid_used->params->dest_pipeline_stage;
      
	m_src_access_flags[img_index] = mid_used->params->source_access_flags;
	m_dst_access_flags[img_index] = mid_used->params->dest_access_flags;

	m_usage_flags[img_index] = mid_used->params->usage_flags;
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
      m_layouts_attach_opt.clear();
      m_layouts_initial.clear();
      m_layouts_final.clear();
      m_widths.clear();
      m_heights.clear();
      m_depths.clear();
      m_types.clear();
      m_tiling.clear();

      m_aspect_flags.clear();

      m_src_pipeline_stages.clear();
      m_dst_pipeline_stages.clear();
      
      m_src_access_flags.clear();
      m_dst_access_flags.clear();

      m_usage_flags.clear();
    }

    VkImage image(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_image,
			    m_images,
			    VkImage);
    }

    VkImageLayout layout_initial(index_type index) const {
      HANDLE_GET_FN_IMPL(index,
			 ok_image,
			 m_layouts_initial,
			 VkImageLayout,
			 VK_IMAGE_LAYOUT_UNDEFINED);
    }
    
    VkImageLayout layout_final(index_type index) const {
      HANDLE_GET_FN_IMPL(index,
			 ok_image,
			 m_layouts_final,
			 VkImageLayout,
			 VK_IMAGE_LAYOUT_UNDEFINED);
    }

    VkImageLayout layout_attach(index_type index) const {
      HANDLE_GET_FN_IMPL(index,
			 ok_image,
			 m_layouts_attach_opt,
			 VkImageLayout,
			 VK_IMAGE_LAYOUT_UNDEFINED);
    }

    VkImageView image_view(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_image,
			    m_image_views,
			    VkImageView);
    }

    darray<VkImageView> image_views(const darray<index_type>& indices) const {
      darray<VkImageView> ret;
      for (const auto& i: indices) {
	ret.push_back(image_view(i));
      }
      return ret;
    }

    VkImageCopy image_copy(index_type index) const {
      VkImageCopy copy = {};
      
      copy.srcSubresource.aspectMask = m_aspect_flags.at(index);
      copy.srcSubresource.mipLevel = 0;
      copy.srcSubresource.baseArrayLayer = 0;
      copy.srcSubresource.layerCount = 1;

      copy.dstSubresource = copy.srcSubresource;
      
      copy.srcOffset.x = 0;
      copy.srcOffset.y = 0;
      copy.srcOffset.z = 0;
      
      copy.dstOffset = copy.srcOffset;

      copy.extent.width = m_widths.at(index);
      copy.extent.height = m_heights.at(index);
      copy.extent.depth = m_depths.at(index);

      return copy;
    }

    image_layout_transition make_layout_transition(index_type index) const {
      auto ret = image_layout_transition(false);

      if (ok_image(index)) {
	ret.
	  from_stage(m_src_pipeline_stages.at(index)).
	  to_stage(m_dst_pipeline_stages.at(index)).
	  for_aspect(m_aspect_flags.at(index)).
	  from_access(m_src_access_flags.at(index)).
	  to_access(m_dst_access_flags.at(index)).
	  from(m_layouts_initial.at(index)).
	  to(m_layouts_final.at(index)).
	  for_image(m_images.at(index)).
	  ready();
      }

      return ret;
    }

    bool make_layout_transitions(VkCommandBuffer cmd_buf, const darray<index_type>& indices) const {
      size_t i{0};
      bool good = true;

      while (i < indices.size() && good) {
	auto image_index = indices.at(i);
		     
	auto layout_transition = make_layout_transition(image_index);		    

	good =
	  c_assert(layout_transition.ok()) &&
	  c_assert(api_ok());

	if (c_assert(good)) {
	  layout_transition.via(cmd_buf);
	}

	i++;
      }

      return c_assert(good);
    }

    void print_images_info() const {
      for (index_type i{0}; i < length(); ++i) {
	std::cout << "Image: " SS_HEX(m_images.at(i)) << "\n"
		  << "..usage flags: " << SS_HEX(m_usage_flags.at(i)) << "\n"; 
      }

      std::cout << std::endl;
    }
    
  };
  
  struct descriptor_set_gen_params {    
    darray<VkShaderStageFlags> stages;
    darray<uint32_t> descriptor_counts;
    VkDescriptorType type{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    
    bool ok() const {      
      return
	c_assert(!stages.empty()) &&
	c_assert(descriptor_counts.size() == stages.size());      
    }

    darray<VkDescriptorSetLayoutBinding> make_bindings() const {
      darray<VkDescriptorSetLayoutBinding> bindings{};

      for (size_t i = 0; i < stages.size(); ++i) {
	bindings.push_back(make_descriptor_set_layout_binding(static_cast<uint32_t>(i),
							      stages.at(i),
							      descriptor_counts.at(i),
							      type));
      }
      
      ASSERT(!bindings.empty());
      return bindings;
    }
  };

  class descriptor_set_pool : index_traits<int16_t,
					   darray<VkDescriptorSet>> {
  public:
    //typedef index_traits<int16_t,
    //			 darray<VkDescriptorSet>> index_type_traits;

    typedef index_traits_this_type::index_type index_type;
    static inline constexpr index_type k_unset = index_traits_this_type::k_unset;

  private:
    darray<VkDescriptorSet> m_descriptor_sets;
    darray<VkDescriptorSetLayout> m_descriptor_set_layouts;
    darray<VkDescriptorType> m_descriptor_types;
    darray<darray<VkDescriptorSetLayoutBinding>> m_descriptor_bindings;

    index_type new_descriptor_set() {
      index_type index = this->length();
    
      m_descriptor_sets.push_back(VK_NULL_HANDLE);
      m_descriptor_set_layouts.push_back(VK_NULL_HANDLE);
      m_descriptor_types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      m_descriptor_bindings.push_back({});

      return index;
    }
    
  public:
    descriptor_set_pool()
      : index_traits_this_type(m_descriptor_sets) {}

    void free_mem(VkDevice device) {
      for (index_type i{0}; i < length(); ++i) {
	free_device_handle<VkDescriptorSetLayout,
			   &vkDestroyDescriptorSetLayout>(device, m_descriptor_set_layouts[i]);
      }

      m_descriptor_set_layouts.clear();
      m_descriptor_sets.clear();
      m_descriptor_types.clear();
    }

    bool ok_descriptor_set(index_type index) const {
      return
	c_assert(ok_index(index)) &&
	c_assert(m_descriptor_set_layouts.at(index) != VK_NULL_HANDLE) &&
	c_assert(m_descriptor_sets.at(index) != VK_NULL_HANDLE);
    }
    
    index_type make_descriptor_set(const device_resource_properties& properties,
				   const descriptor_set_gen_params& params) {
      index_type handle{k_unset};

      if (c_assert(properties.ok()) &&
	  c_assert(params.ok())) {
	VkDescriptorSetLayout desc_set_layout{VK_NULL_HANDLE};
        VkDescriptorSet desc_set{VK_NULL_HANDLE};       

	auto bindings = params.make_bindings();
	
	desc_set_layout = make_descriptor_set_layout(properties.device,
						     bindings.data(),
						     bindings.size());

	if (c_assert(H_OK(desc_set_layout))) {
	  desc_set = vulkan::make_descriptor_set(properties.device,
						 properties.descriptor_pool,
						 &desc_set_layout,
						 1);
	}

	if (c_assert(H_OK(desc_set))) {
	  handle = new_descriptor_set();
	  
	  m_descriptor_set_layouts[handle] = desc_set_layout;
	  m_descriptor_sets[handle] = desc_set;
	  m_descriptor_types[handle] = params.type;
	  m_descriptor_bindings[handle] = bindings;
	}
      }
      ASSERT(ok_descriptor_set(handle));

      return handle;
    }

    VkDescriptorType descriptor_type(index_type index) const {
      VkDescriptorType type{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
      if (ok_descriptor_set(index)) {
	type = m_descriptor_types.at(index);
      }
      return type;
    }

    VkDescriptorSet descriptor_set(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_descriptor_set,
			    m_descriptor_sets,
			    VkDescriptorSet);
    }

    darray<VkDescriptorSet> descriptor_sets(const darray<index_type>& indices) const {
      return
	fmap_start
	fmap_from index_type
	fmap_to VkDescriptorSet
	fmap_using indices
	fmap_via
	[this](const index_type& index) {
	  return descriptor_set(index);
	}
	fmap_end;
    }

    VkDescriptorSetLayout descriptor_set_layout(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_descriptor_set,
			    m_descriptor_set_layouts,
			    VkDescriptorSetLayout);
    }

    darray<VkDescriptorSetLayout> descriptor_set_layouts(const darray<index_type>& indices) const {
      return
        fmap_start
	fmap_from index_type
	fmap_to VkDescriptorSetLayout
	fmap_using indices
	fmap_via
	[this](const index_type& index) {
	  return descriptor_set_layout(index);
	}
	fmap_end;	
    }

    bool write_buffer(index_type index,
		      VkDevice device,
		      VkBuffer buf,
		      VkDeviceSize buf_size,
		      uint32_t binding_index,
		      uint32_t array_element_index) const {
      bool r = false;
      if (ok_descriptor_set(index)) {
	r = write_descriptor_set(device,
				 buf,
				 buf_size,
				 m_descriptor_sets.at(index),
				 binding_index,
				 array_element_index,
				 m_descriptor_types.at(index));
       
      }
      return r;
    }
  };
  
  struct texture_gen_params;
  
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

    darray<descriptor_set_pool::index_type> m_descriptor_sets;
    
    darray<uint32_t> m_desc_layout_binding_indices;
    darray<uint32_t> m_desc_array_element_indices;
    
    image_pool* m_image_pool;
    descriptor_set_pool* m_descriptor_set_pool;

    VkSampler make_sampler(const device_resource_properties& properties,
			   const texture_gen_params& params) const;
    
    index_type new_texture();
    
  public:
    texture_pool();

    void set_image_pool(image_pool* p);

    void set_descriptor_set_pool(descriptor_set_pool* p);
    
    index_type make_texture(const device_resource_properties& properties,
			    const texture_gen_params& params);

    void free_mem(VkDevice device);

    bool ok_texture(index_type index) const;
    
    VkSampler sampler(index_type index) const;   

    VkDescriptorImageInfo make_descriptor_image_info(index_type index) const;

    VkWriteDescriptorSet make_write_descriptor_set(index_type index, const VkDescriptorImageInfo* image_info) const;

    bool update_descriptor_sets(VkDevice device, darray<index_type> indices) const;
  };

  struct texture_gen_params {
    image_pool::index_type image_index{image_pool::k_unset};

    descriptor_set_pool::index_type descriptor_set_index{descriptor_set_pool::k_unset};
    
    uint32_t descriptor_array_element{UINT32_MAX};
    uint32_t binding_index{UINT32_MAX};
    
    bool ok() const {
      bool r =
	(image_index != image_pool::k_unset) &&
	(descriptor_set_index != descriptor_set_pool::k_unset) &&
	(descriptor_array_element != UINT32_MAX) &&
	(binding_index != UINT32_MAX);
      
      ASSERT(r);
      return r;
    }
  };
}
