#pragma once

#include "vk_common.hpp"

namespace vulkan {
  struct render_pass_gen_params {
    VkFormat swapchain_format;
    
    bool ok() const {
      return true;
    }
  };

  class render_pass_pool : index_traits<int16_t, darray<VkRenderPass>> {
  public:
    typedef index_traits_this_type::index_type index_type;
    static constexpr inline index_type k_unset = index_traits_this_type::k_unset;

  private:
    darray<VkRenderPass> m_render_passes;

    index_type new_render_pass() {
      index_type index{this->length()};

      m_render_passes.push_back(VK_NULL_HANDLE);

      return index;
    }
    
  public:
    render_pass_pool() : index_traits_this_type(m_render_passes)
    {}

    bool ok_render_pass(index_type index) const {
      bool r =
	ok_index(index) &&
	m_render_passes.at(index) != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }
    
    void free_mem(VkDevice device) {
      for (VkRenderPass& render_pass: m_render_passes) {
	free_device_handle<VkRenderPass, &vkDestroyRenderPass>(device, render_pass);
      }

      m_render_passes.clear();
    }
    
    index_type make_render_pass(const device_resource_properties& properties,
				const render_pass_gen_params& params) {

      index_type render_pass_index{k_unset};

      if (params.ok() && properties.ok()) {
	VkRenderPass render_pass_object{VK_NULL_HANDLE};
	
	//	auto colorbuffer = default_colorbuffer_settings(m_vk_khr_swapchain_format.format);

	VkAttachmentDescription colorbuffer = {};	
	colorbuffer.format = params.swapchain_format;
	colorbuffer.samples = VK_SAMPLE_COUNT_1_BIT;
	colorbuffer.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorbuffer.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	
	colorbuffer.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorbuffer.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    
	colorbuffer.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorbuffer.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorbuffer_ref = {};
	colorbuffer_ref.attachment = 0;
	colorbuffer_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;       


	VkAttachmentDescription depthbuffer = {};
	depthbuffer.format = depthbuffer_data::k_format;
    
	depthbuffer.samples = VK_SAMPLE_COUNT_1_BIT;
	depthbuffer.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthbuffer.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	depthbuffer.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthbuffer.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	depthbuffer.initialLayout = depthbuffer_data::k_initial_layout;
	depthbuffer.finalLayout = depthbuffer_data::k_final_layout;

	VkAttachmentReference depthbuffer_ref = {};
        depthbuffer_ref.attachment = 1;
        depthbuffer_ref.layout = depthbuffer_data::k_final_layout;
	
	VkSubpassDescription color_subpass = {};
	color_subpass.inputAttachmentCount = 0;
	color_subpass.pInputAttachments = nullptr;
	color_subpass.pResolveAttachments = nullptr;
	color_subpass.preserveAttachmentCount = 0;
	color_subpass.pPreserveAttachments = nullptr;
	color_subpass.flags = 0;
	color_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	color_subpass.colorAttachmentCount = 1;
	color_subpass.pColorAttachments = &colorbuffer_ref;
	color_subpass.pDepthStencilAttachment = &depthbuffer_ref;

	std::array<VkSubpassDescription, 1> subpasses =
	  {
	   color_subpass
	  };
	
	constexpr uint32_t k_depth_subpass_index = 0;
	constexpr uint32_t k_color_subpass_index = 0;

	VkSubpassDependency depth_dependency = {};
	depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	depth_dependency.dstSubpass = k_depth_subpass_index;

	depth_dependency.srcStageMask =
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	depth_dependency.srcAccessMask = 0;

	depth_dependency.dstStageMask =
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	
	depth_dependency.dstAccessMask = depthbuffer_data::k_access_flags;
	  	
	VkSubpassDependency color_dependency = {};
	color_dependency.srcSubpass = k_depth_subpass_index;
	color_dependency.dstSubpass = k_color_subpass_index;

	color_dependency.srcStageMask =
	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	color_dependency.srcAccessMask = 0;

	color_dependency.dstStageMask =
	  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	color_dependency.dstAccessMask =
	  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency self_dependency = {};	
	self_dependency.srcSubpass = k_color_subpass_index;
	self_dependency.dstSubpass = k_color_subpass_index;
	
	self_dependency.srcStageMask =
	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	self_dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	
	self_dependency.dstStageMask =
	  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	self_dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	
	std::array<VkSubpassDependency, 3> dependencies =
	  {
	   depth_dependency,
	   color_dependency,
	   self_dependency
	  };

	std::array<VkAttachmentDescription, 2> attachments =
	  {
	   colorbuffer,
	   depthbuffer	   
	  };
	
	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.flags = 0;
	render_pass_info.attachmentCount = attachments.size();
	render_pass_info.pAttachments = attachments.data();

	render_pass_info.subpassCount = subpasses.size();
	render_pass_info.pSubpasses = subpasses.data();
	
	render_pass_info.dependencyCount = dependencies.size();
	render_pass_info.pDependencies = dependencies.data();

	VK_FN(vkCreateRenderPass(properties.device,
				 &render_pass_info,
				 nullptr,
				 &render_pass_object));

	if (H_OK(render_pass_object)) {
	  render_pass_index = new_render_pass();
	  
	  m_render_passes[render_pass_index] = render_pass_object;
	}
      }
      
      return render_pass_index;
    }

    VkRenderPass render_pass(index_type index) const {
      VK_HANDLE_GET_FN_IMPL(index,
			    ok_render_pass,
			    m_render_passes,
			    VkRenderPass);
    }
    
  };

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
    std::string vert_spv_path{};
    std::string frag_spv_path{};

    render_pass_pool::index_type render_pass_index{render_pass_pool::k_unset};

    bool ok() const {
      bool r =
	(!vert_spv_path.empty()) &&
	(!frag_spv_path.empty()) &&
	(render_pass_index != render_pass_pool::k_unset);
	
      ASSERT(r);	
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
    
  public:

  };
  
  

}

