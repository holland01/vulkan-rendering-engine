#include "vulkan.hpp"

namespace vulkan {

  VkResult g_vk_result = VK_SUCCESS;

  VkResult vk_call(VkResult call, const char* expr, int line, const char* file) {
    if (call != VK_SUCCESS) {
      write_logf("VULKAN ERROR: %s@%s:%i -> 0x%x\n", 
                 expr, 
                 file, 
                 line, 
                 call);
    }
    return call;
  }
  
  bool api_ok() {
    return g_vk_result == VK_SUCCESS;
  }

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
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
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

}
