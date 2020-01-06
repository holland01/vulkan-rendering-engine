#include "vulkan.hpp"
#include "vk_pipeline.hpp"

#define BASE_TEXTURE2D_DEFAULT_VK_FORMAT VK_FORMAT_R8G8B8A8_UNORM

namespace vulkan {
  VkPipelineVertexInputStateCreateInfo default_vertex_input_state_settings() {
    VkPipelineVertexInputStateCreateInfo vinput_info = {};
    vinput_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vinput_info.vertexBindingDescriptionCount = 0;
    vinput_info.pVertexBindingDescriptions = nullptr; // optional
    vinput_info.vertexAttributeDescriptionCount = 0; 
    vinput_info.pVertexAttributeDescriptions = nullptr; // optional
    return vinput_info;
  }

  VkPipelineInputAssemblyStateCreateInfo default_input_assembly_state_settings() {
    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;
    return ia;
  }

  VkPipelineViewportStateCreateInfo default_viewport_state_settings() {
    VkPipelineViewportStateCreateInfo vps = {};
    vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 0;
    vps.pViewports = nullptr;
    vps.scissorCount = 0;
    vps.pScissors = nullptr;
    return vps;
  }

  VkPipelineRasterizationStateCreateInfo default_rasterization_state_settings() {
    VkPipelineRasterizationStateCreateInfo rs = {};

    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	
    rs.depthBiasEnable = VK_FALSE;
    rs.depthBiasConstantFactor = 0.0f;
    rs.depthBiasClamp = 0.0f;
    rs.depthBiasSlopeFactor = 0.0f;

    return rs;
  }

  VkPipelineMultisampleStateCreateInfo default_multisample_state_settings() {
    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.sampleShadingEnable = VK_FALSE;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ms.minSampleShading = 1.0f;
    ms.pSampleMask = nullptr;
    ms.alphaToCoverageEnable = VK_FALSE;
    ms.alphaToOneEnable = VK_FALSE;
    return ms;
  }

  VkPipelineColorBlendAttachmentState default_color_blend_attach_state_settings() {
    VkPipelineColorBlendAttachmentState cba = {};
  
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    cba.blendEnable = VK_FALSE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;

    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    cba.alphaBlendOp = VK_BLEND_OP_ADD;

    return cba;
  }

  VkPipelineColorBlendStateCreateInfo default_color_blend_state_settings() {
    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.logicOpEnable = VK_FALSE;
    cb.logicOp = VK_LOGIC_OP_COPY;
    cb.attachmentCount = 0;
    cb.pAttachments = nullptr;
    for (size_t i = 0; i < 4; ++i) {
      cb.blendConstants[i] = 0.0f;
    }
    return cb;
  }

  VkPipelineLayoutCreateInfo default_pipeline_layout_settings() {
    VkPipelineLayoutCreateInfo pl = {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 0;
    pl.pSetLayouts = nullptr;
    pl.pushConstantRangeCount = 0;
    pl.pPushConstantRanges = nullptr;
    return pl;
  }

  VkAttachmentDescription default_colorbuffer_settings(VkFormat swapchain_format) {
    VkAttachmentDescription color_attachment = {};
	
    color_attachment.format = swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return color_attachment;
  }

  VkAttachmentReference default_colorbuffer_ref_settings() {
    VkAttachmentReference attachment_ref = {};
    attachment_ref.attachment = 0;
    attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return attachment_ref;
  }

  VkAttachmentDescription default_depthbuffer_settings() {
    VkAttachmentDescription depth_attachment = {};

    depth_attachment.format = depthbuffer_data::k_format;
    
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    depth_attachment.initialLayout = depthbuffer_data::k_initial_layout;
    depth_attachment.finalLayout = depthbuffer_data::k_final_layout;

    return depth_attachment;
  }

  VkAttachmentReference default_depthbuffer_ref_settings() {
    VkAttachmentReference attachment_ref = {};
    attachment_ref.attachment = 1;
    attachment_ref.layout = depthbuffer_data::k_final_layout;
    return attachment_ref;
  }

  VkStencilOpState default_stencilop_state() {
    VkStencilOpState op_state = {};
    op_state.failOp = VK_STENCIL_OP_KEEP;
    op_state.passOp = VK_STENCIL_OP_KEEP;
    op_state.depthFailOp = VK_STENCIL_OP_KEEP;
    op_state.compareOp = VK_COMPARE_OP_NEVER;
    op_state.compareMask = 0;
    op_state.writeMask = 0;
    op_state.reference = 0;
    return op_state;
  }


  VkShaderModuleCreateInfo make_shader_module_settings(darray<uint8_t>& spv_code) {
    VkShaderModuleCreateInfo module_create_info = {};
    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_create_info.codeSize = spv_code.size();
    module_create_info.pCode = reinterpret_cast<uint32_t*>(spv_code.data());
    return module_create_info;
  }

  VkPipelineShaderStageCreateInfo make_shader_stage_settings(VkShaderModule module, VkShaderStageFlagBits type) {
    VkPipelineShaderStageCreateInfo stage_info = {};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = type;
    stage_info.module = module;
    stage_info.pName = "main";
    return stage_info;
  }

  

  // We will not call vkGetPhysicalDeviceImageFormatProperties()
  // if g_vk_result is not equal to VK_SUCCESS beforehand. We may as well
  // consider this a false result, given that at that point something
  // else that's wrong has happened.
  static
  bool image_create_info_valid(VkPhysicalDevice physical_device, const VkImageCreateInfo& create_info) {
    VkImageFormatProperties properties;

    VK_FN(vkGetPhysicalDeviceImageFormatProperties(physical_device,
						   create_info.format,
						   create_info.imageType,
						   create_info.tiling,
						   create_info.usage,
						   create_info.flags,
						   &properties));

    return api_ok();
  }

  template <typename imageContainerType>
  VkImageCreateInfo default_image_create_info_texture2d(const device_resource_properties& properties) {
    VkImageCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.pNext = nullptr;

    create_info.flags = 0;
    create_info.imageType = VK_IMAGE_TYPE_2D;
    
    create_info.format = imageContainerType::k_format;
    create_info.initialLayout = imageContainerType::k_initial_layout;
    create_info.tiling = imageContainerType::k_image_tiling;
    create_info.usage = imageContainerType::k_image_usage_flags;
    
    create_info.extent.width = 0;
    create_info.extent.height = 0;
    create_info.extent.depth = 1;

    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;

    create_info.samples = VK_SAMPLE_COUNT_1_BIT;

    create_info.sharingMode = properties.queue_sharing_mode;

    create_info.queueFamilyIndexCount = properties.queue_family_indices.size();
    create_info.pQueueFamilyIndices = properties.queue_family_indices.data();     

    ASSERT(image_create_info_valid(properties.physical_device,
				   create_info));
    
    return create_info;
  }

  template <typename imageContainerType>
  VkImageViewCreateInfo default_image_view_create_info_texture2d() {
    VkImageViewCreateInfo create_info = {};
    
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.image = VK_NULL_HANDLE;

    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = imageContainerType::k_format;
    
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    create_info.subresourceRange.aspectMask = imageContainerType::k_image_aspect_flags;

    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;

    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    return create_info;
  }
  
  static VkSamplerCreateInfo default_sampler_create_info_texture2d() {
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

    return create_info;
  }

  static
  VkExtent3D calc_minimum_dimensions_texture2d(uint32_t width,
					       uint32_t height,
					       uint32_t bytes_per_pixel,
					       const VkMemoryRequirements& requirements) {
    // If any of these are not a power of 2,
    // there is no guarantee that,
    // once the loop finishes,
    // calc_size() == requirements.size will be true.
    //
    // We want this to be true:
    // any deviation from this format
    // will require a different method
    // of adjusting the width and height
    // to meet the required size.
    //    ASSERT(is_power_2(requirements.size));
    //    ASSERT(is_power_2(width));
    //    ASSERT(is_power_2(height));
    ASSERT(is_power_2(bytes_per_pixel));

    printf("width: %" PRIu32 "\n"
	   "height: %" PRIu32 "\n"
	   "requirements.size: %" PRIu64 "\n",
	   width, height, requirements.size);

    bool all_pot =
      is_power_2(requirements.size) &&
      is_power_2(width) &&
      is_power_2(height);
     
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

    ASSERT((calc_size() == requirements.size && all_pot) || calc_size() >= requirements.size);
    
    ext.width = width;
    ext.height = height; 
    
    return ext;
  }


  // gets allocation requirements
  // for a given width/height/bpp/image layout combination,
  // while also verifying compatibility with these parameters,
  // in addition to our chosen image format.
  template <class imageContainerType>
  image_requirements get_image_requirements_texture2d(const device_resource_properties& properties,
						      uint32_t width,
						      uint32_t height) {
    ASSERT(imageContainerType::k_bpp ==
	   bpp_from_format(imageContainerType::k_format));
    
    image_requirements ret{};

    ret.desired.width = width;
    ret.desired.height = height;
    ret.desired.depth = 1;

    ret.bytes_per_pixel = imageContainerType::k_bpp;
    
    VkImageCreateInfo create_info =
      default_image_create_info_texture2d<imageContainerType>(properties);

    create_info.extent.width = width;
    create_info.extent.height = height;

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
      
      ret.required = calc_minimum_dimensions_texture2d(width,
						       height,
						       imageContainerType::k_bpp,
						       req);
      
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
				 imageContainerType::k_memory_property_flags);
      
	ASSERT(memory_type_index != -1);

	ret.memory_type_index =
	  static_cast<uint32_t>(memory_type_index);
      }

      vkDestroyImage(properties.device, dummy_image, nullptr);
    }

    ASSERT(ret.ok());

    return ret;
  }
  
  texture2d_data make_texture2d(const device_resource_properties& properties,
				uint32_t width,
				uint32_t height,
				uint32_t bytes_per_pixel,
				const darray<uint8_t>& pixels) {
    
    texture2d_data ret{};
    
    if (properties.ok()) {
      VK_FN(vkDeviceWaitIdle(properties.device));

      image_requirements req = get_image_requirements_texture2d<texture2d_data>(properties,
										width,
										height);
							                    
      if (req.ok()) {
	ret.memory = make_device_memory(properties.device,
					pixels.data(),
					pixels.size() * sizeof(pixels[0]),
					req.memory_size(),
					req.memory_type_index);
      }

      if (ret.memory != VK_NULL_HANDLE) {
	VkImageCreateInfo create_info =
	  default_image_create_info_texture2d<texture2d_data>(properties);

	create_info.extent.width = width;
	create_info.extent.height = height;	

	ret.format = create_info.format;
      
	VK_FN(vkCreateImage(properties.device,
			    &create_info,
			    nullptr,
			    &ret.image));
      }

      bool bound = false;
      if (api_ok() && ret.image != VK_NULL_HANDLE) {
	VK_FN(vkBindImageMemory(properties.device,
				ret.image,
				ret.memory,
				0));
	
	bound = api_ok();
      }
      
      if (bound) {
	VkImageViewCreateInfo create_info =
	  default_image_view_create_info_texture2d<texture2d_data>();

	create_info.image = ret.image;

	VK_FN(vkCreateImageView(properties.device,
				&create_info,
				nullptr,
				&ret.image_view));
      }

      if (api_ok() && ret.image_view != VK_NULL_HANDLE) {
	VkSamplerCreateInfo create_info =
	  default_sampler_create_info_texture2d();
	
	VK_FN(vkCreateSampler(properties.device,
			      &create_info,
			      nullptr,
			      &ret.sampler));
      }

      if (api_ok() && ret.sampler != VK_NULL_HANDLE) {
	VkDescriptorSetLayoutBinding ds_binding =
	  make_descriptor_set_layout_binding(0,
					     VK_SHADER_STAGE_FRAGMENT_BIT,
					     texture2d_data::k_descriptor_type);
	
	ret.descriptor_set_layout =
	  make_descriptor_set_layout(properties.device,
				     &ds_binding,
				     1);             
      }
    
      if (api_ok() && ret.descriptor_set_layout != VK_NULL_HANDLE) {
	ret.descriptor_set =
	  make_descriptor_set(properties.device,
			      properties.descriptor_pool,
			      &ret.descriptor_set_layout,
			      1);
      }

      if (api_ok() && ret.descriptor_set != VK_NULL_HANDLE) {
	ret.width = width;
	ret.height = height;
	ret.format = texture2d_data::k_format;
	ret.layout = texture2d_data::k_final_layout;
      }

      ASSERT(api_ok());
    }

    ASSERT(ret.ok());
    
    return ret;
  }

  depthbuffer_data make_depthbuffer(const device_resource_properties& properties,
				    uint32_t width,
				    uint32_t height) {
    depthbuffer_data ret{};
    
    if (properties.ok()) {
      VK_FN(vkDeviceWaitIdle(properties.device));
      image_requirements req = get_image_requirements_texture2d<depthbuffer_data>(properties,
										  width,
										  height);
      if (req.ok()) {
	//	darray<uint8_t> depthvalues(width * height * depthbuffer_data::k_bpp, 0);
	//	ret.memory = make_device_memory(properties.device,
	//				reinterpret_cast<void*>(depthvalues.data()),
	//			        depthvalues.size() * sizeof(depthvalues[0]),
	//				req.memory_size(),
	//				req.memory_type_index);

	    VkDeviceMemory memory{VK_NULL_HANDLE};
	    VkMemoryAllocateInfo info = {};
      
	    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	    info.pNext = nullptr;
	    info.allocationSize = req.memory_size();
	    info.memoryTypeIndex = req.memory_type_index;

	    VK_FN(vkAllocateMemory(properties.device,
				   &info,
				   nullptr,
				   &ret.memory));

      }
      
      if (ret.memory != VK_NULL_HANDLE) {
	VkImageCreateInfo create_info =
	  default_image_create_info_texture2d<depthbuffer_data>(properties);

	create_info.extent.width = width;
	create_info.extent.height = height;
      
	VK_FN(vkCreateImage(properties.device,
			    &create_info,
			    nullptr,
			    &ret.image));
      }
      
      bool bound = false;
      if (api_ok() && ret.image != VK_NULL_HANDLE) {
	VK_FN(vkBindImageMemory(properties.device,
				ret.image,
				ret.memory,
				0));
	
	bound = api_ok();
      }
      
      if (bound) {
	VkImageViewCreateInfo create_info =
	  default_image_view_create_info_texture2d<depthbuffer_data>();

	create_info.image = ret.image;
	
	VK_FN(vkCreateImageView(properties.device,
				&create_info,
				nullptr,
				&ret.image_view));
      }
    
      if (api_ok() && ret.image_view != VK_NULL_HANDLE) {
	ret.width = width;
	ret.height = height;
      }

      ASSERT(api_ok());
    }

    std::cout << "before - " << ret.to_string() << std::endl;

    ASSERT(ret.ok());
    return ret;
  }

} // end namespace vulkan
