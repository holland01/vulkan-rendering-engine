#pragma once

#include "vk_common.hpp"
#include "vk_image.hpp"

#include <iostream>

namespace vulkan {
  // defined in vulkan.cpp
  VkPipelineVertexInputStateCreateInfo default_vertex_input_state_settings();
  VkPipelineInputAssemblyStateCreateInfo default_input_assembly_state_settings();
  VkPipelineViewportStateCreateInfo default_viewport_state_settings();
  VkPipelineRasterizationStateCreateInfo default_rasterization_state_settings();
  VkPipelineMultisampleStateCreateInfo default_multisample_state_settings();
  VkPipelineColorBlendAttachmentState default_color_blend_attach_state_settings();
  VkPipelineColorBlendStateCreateInfo default_color_blend_state_settings();

  VkStencilOpState default_stencilop_state();

  struct pipeline_layout_gen_params {
    darray<VkDescriptorSetLayout> descriptor_set_layouts;
    darray<VkPushConstantRange> push_constant_ranges;

    bool ok() const {
      return true;
    }
  };

  class pipeline_layout_pool : index_traits<int16_t,
					    darray<VkPipelineLayout>> {
  public:
    typedef index_traits_this_type::index_type index_type;
    static constexpr inline index_type k_unset = index_traits_this_type::k_unset;
  private:
    darray<VkPipelineLayout> m_pipeline_layouts;

    index_type new_pipeline_layout() {
      index_type index{this->length()};
      m_pipeline_layouts.push_back(VK_NULL_HANDLE);
      return index;
    }

  public:
    pipeline_layout_pool()
      : index_traits_this_type(m_pipeline_layouts) {
    }

    void free_mem(VkDevice device) {
      for (VkPipelineLayout& layout: m_pipeline_layouts) {
	free_device_handle<VkPipelineLayout, &vkDestroyPipelineLayout>(device,
								       layout);
      }

      m_pipeline_layouts.clear();
    }

    bool ok_pipeline_layout(index_type index) const {
      bool r =
	ok_index(index) &&
	m_pipeline_layouts.at(index) != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }
    
    index_type make_pipeline_layout(const device_resource_properties& properties,
				    const pipeline_layout_gen_params& params) {
      index_type layout_index{k_unset};

      if (properties.ok() && params.ok()) {
	VkPipelineLayout pl_object{VK_NULL_HANDLE};

	VkPipelineLayoutCreateInfo pipeline_layout_settings = {};
	pipeline_layout_settings.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;       

	pipeline_layout_settings.setLayoutCount = params.descriptor_set_layouts.size();
	pipeline_layout_settings.pSetLayouts = null_if_empty(params.descriptor_set_layouts);

	pipeline_layout_settings.pushConstantRangeCount = params.push_constant_ranges.size();
	pipeline_layout_settings.pPushConstantRanges = null_if_empty(params.push_constant_ranges);
	
	VK_FN(vkCreatePipelineLayout(properties.device,
				     &pipeline_layout_settings,
				     nullptr,
				     &pl_object));

	if (H_OK(pl_object)) {
	  layout_index = new_pipeline_layout();

	  m_pipeline_layouts[layout_index] = pl_object;
	}
      }

      return layout_index;
    }

    VkPipelineLayout pipeline_layout(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_pipeline_layout,
			    m_pipeline_layouts,
			    VkPipelineLayout);
    }

  };
  
  struct pipeline_gen_params {
    VkRenderPass render_pass{VK_NULL_HANDLE};
    VkExtent2D viewport_extent{};
    
    std::string vert_spv_path{};
    std::string frag_spv_path{};

    pipeline_layout_pool::index_type pipeline_layout_index{pipeline_layout_pool::k_unset};

    uint32_t subpass_index{UINT32_MAX};
    
    bool ok() const {
      bool r =
	c_assert(!vert_spv_path.empty()) &&
	c_assert(!frag_spv_path.empty()) &&
	c_assert(H_OK(render_pass)) &&
	c_assert(pipeline_layout_index != pipeline_layout_pool::k_unset) &&
	c_assert(subpass_index != UINT32_MAX);
	
      return r;
    }
  };

  class pipeline_pool : index_traits<int16_t,
				     darray<VkPipeline>> {
  public:
    typedef index_traits_this_type::index_type index_type;
    static constexpr inline index_type k_unset = index_traits_this_type::k_unset;

  private:
    darray<VkPipeline> m_pipelines;
    darray<pipeline_layout_pool::index_type> m_pipeline_layouts;

    pipeline_layout_pool* m_pipeline_layout_pool{nullptr};
    
    index_type new_pipeline() {
      index_type index{this->length()};

      m_pipelines.push_back(VK_NULL_HANDLE);
      m_pipeline_layouts.push_back(pipeline_layout_pool::k_unset);
      
      return index;
    }

    VkShaderModule make_shader_module(const device_resource_properties& properties,
				      darray<uint8_t> spv_code) const {
      VkShaderModule ret{VK_NULL_HANDLE};

      if (api_ok()) {
	VkShaderModuleCreateInfo create_info = {};

	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = spv_code.size();
	create_info.pCode = reinterpret_cast<uint32_t*>(spv_code.data());
      
	VK_FN(vkCreateShaderModule(properties.device, &create_info, nullptr, &ret));
      }

      return ret;
    }
    
  public:
    pipeline_pool()
      : index_traits_this_type(m_pipelines)
    {}

    void set_pipeline_layout_pool(pipeline_layout_pool* p) {
      if (c_assert(m_pipeline_layout_pool == nullptr)) {
	m_pipeline_layout_pool = p;
      }
    }
    
    void free_mem(VkDevice device) {
      for (VkPipeline& pipeline: m_pipelines) {
	free_device_handle<VkPipeline, &vkDestroyPipeline>(device, pipeline);
      }

      m_pipelines.clear();
    }

    bool ok_pipeline(index_type index) const {
      bool r =
	ok_index(index) &&
	
	c_assert(m_pipeline_layout_pool != nullptr) &&
	m_pipeline_layout_pool->ok_pipeline_layout(m_pipeline_layouts.at(index)) &&
	c_assert(H_OK(m_pipelines.at(index)));

      ASSERT(r);
      return r;
    }

    index_type make_pipeline(const device_resource_properties& properties,
			     const pipeline_gen_params& params) {
      index_type pipeline_index{k_unset};

      if (c_assert(m_pipeline_layout_pool != nullptr) &&
	  properties.ok() &&
	  params.ok()) {

	std::cout << "BEGIN PIPELINE CREATION FOR " << params.vert_spv_path << ", " << params.frag_spv_path << std::endl; 
	
	VkPipeline pl_object{VK_NULL_HANDLE};

	//
	// create shader programs
	// 
	auto spv_vshader = read_file(params.vert_spv_path);
	auto spv_fshader = read_file(params.frag_spv_path);
		
	ASSERT(!spv_vshader.empty());
	ASSERT(!spv_fshader.empty());
	
	VkShaderModule vshader_module = make_shader_module(properties, spv_vshader);
	VkShaderModule fshader_module = make_shader_module(properties, spv_fshader);

	ASSERT(vshader_module != VK_NULL_HANDLE);
	ASSERT(fshader_module != VK_NULL_HANDLE);

	VkPipelineShaderStageCreateInfo vshader_create = {};
	vshader_create.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vshader_create.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vshader_create.module = vshader_module;
	vshader_create.pName = "main";

	VkPipelineShaderStageCreateInfo fshader_create = {};
	fshader_create.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fshader_create.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fshader_create.module = fshader_module;
	fshader_create.pName = "main";

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages =
	  {
	   vshader_create,
	   fshader_create
	  };

	auto vertex_input_state = default_vertex_input_state_settings();

	VkVertexInputAttributeDescription iad_position = {};
	iad_position.location = 0;
	iad_position.binding = 0;
	iad_position.format = VK_FORMAT_R32G32B32_SFLOAT;
	iad_position.offset = offsetof(vertex_data, position);

	VkVertexInputAttributeDescription iad_texture = {};
	iad_texture.location = 1;
	iad_texture.binding = 0;
	iad_texture.format = VK_FORMAT_R32G32_SFLOAT;
	iad_texture.offset = offsetof(vertex_data, st);

	VkVertexInputAttributeDescription iad_color = {};
	iad_color.location = 2;
	iad_color.binding = 0;
	iad_color.format = VK_FORMAT_R32G32B32_SFLOAT;
	iad_color.offset = offsetof(vertex_data, color);

	VkVertexInputAttributeDescription iad_normal = {};
	iad_normal.location = 3;
	iad_normal.binding = 0;
	iad_normal.format = VK_FORMAT_R32G32B32_SFLOAT;
	iad_normal.offset = offsetof(vertex_data, normal);
	
	darray<VkVertexInputAttributeDescription> input_attrs =
	  {
	   iad_position,
	   iad_texture,
	   iad_color,
	   iad_normal
	  };

	vertex_input_state.vertexAttributeDescriptionCount = input_attrs.size();
	vertex_input_state.pVertexAttributeDescriptions = input_attrs.data();
	
	VkVertexInputBindingDescription ibd = {};
	ibd.binding = 0;
	ibd.stride = sizeof(vertex_data);
	ibd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	vertex_input_state.vertexBindingDescriptionCount = 1;
	vertex_input_state.pVertexBindingDescriptions = &ibd;
		
	auto input_assembly_state = default_input_assembly_state_settings();
	
	auto viewport = make_viewport(R2(0), params.viewport_extent, 0.0f, 1.0f);
	
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = params.viewport_extent;

	auto viewport_state = default_viewport_state_settings();
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	auto rasterization_state = default_rasterization_state_settings();
	auto multisample_state = default_multisample_state_settings();
	
	auto color_blend_attach_state = default_color_blend_attach_state_settings();
	auto color_blend_state = default_color_blend_state_settings();
	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments = &color_blend_attach_state;
	

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = nullptr;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = VK_TRUE;
	depth_stencil_state.depthWriteEnable = VK_TRUE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = VK_FALSE;
	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;
	depth_stencil_state.front = default_stencilop_state();
	depth_stencil_state.back = default_stencilop_state();
	
	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages.data();
	pipeline_info.pVertexInputState = &vertex_input_state;
	pipeline_info.pInputAssemblyState = &input_assembly_state;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterization_state;
	pipeline_info.pMultisampleState = &multisample_state;
	pipeline_info.pDepthStencilState = &depth_stencil_state;
	pipeline_info.pColorBlendState = &color_blend_state;
	pipeline_info.pDynamicState = nullptr;
	pipeline_info.layout = m_pipeline_layout_pool->pipeline_layout(params.pipeline_layout_index);
	pipeline_info.renderPass = params.render_pass;
	pipeline_info.subpass = params.subpass_index;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;

	VK_FN(vkCreateGraphicsPipelines(properties.device,
					VK_NULL_HANDLE,
					1,
					&pipeline_info,
					nullptr,
					&pl_object));

	//
	// Free memory
	//
	
	free_device_handle<VkShaderModule, &vkDestroyShaderModule>(properties.device, vshader_module);
	free_device_handle<VkShaderModule, &vkDestroyShaderModule>(properties.device, fshader_module);

	if (H_OK(pl_object)) {
	  pipeline_index = new_pipeline();

	  m_pipeline_layouts[pipeline_index] = params.pipeline_layout_index;
	  m_pipelines[pipeline_index] = pl_object;
	}

	std::cout << "END PIPELINE CREATION FOR " << params.vert_spv_path << ", " << params.frag_spv_path << std::endl; 
      }

      return pipeline_index;
    }

    VkPipeline pipeline(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_pipeline,
			    m_pipelines,
			    VkPipeline);

    }
  };
  
  

}

