#include "vulkanApplication.h"

#include <iostream>


/**
 *  Creates the compute pipeline.
 */
void VulkanApplication::createComputePipeline() {
	// Load compute shader
	auto compShaderCode = readFile("../resources/shaders/comp.spv");

	// Create shader module
	VkShaderModule  compShaderModule = createShaderModule(compShaderCode);

	// Assign pipeline stages
	VkPipelineShaderStageCreateInfo compShaderStageInfo{};
	compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = compShaderModule;
	compShaderStageInfo.pName = "main"; //entrypoint
	compShaderStageInfo.pSpecializationInfo = nullptr; //can be used to set const values in shader before compilation!

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &computeBundle.descriptorSetLayout;

    VkPushConstantRange computePushConstants = VkPushConstantRange{};
    computePushConstants.offset = 0;
    computePushConstants.size = computePushConstantSize;
    computePushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    pipelineLayoutInfo.pPushConstantRanges = &computePushConstants;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("ERR::VULKAN::CREATE_COMPUTE_PIPELINE::PIPELINE_LAYOUT_CREATION_FAILED");

	// Pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = computePipelineLayout;
    pipelineInfo.stage = compShaderStageInfo;

    //create compute pipeline
	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS)
		throw std::runtime_error("ERR::VULKAN::CREATE_COMPUTE_PIPELINE::PIPELINE_CREATION_FAILED");

	vkDestroyShaderModule(device, compShaderModule, nullptr);
}

/**
 *  Records the command buffer for Compute.
 */
void VulkanApplication::recordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
    // Bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeBundle.descriptorSets[currentFrame], 0, nullptr);

    // TODO: MAKE THIS MODULAR INSTEAD OF FORCED STATIC CAST
    vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, computePushConstantSize, (RTFrame*)computePushConstantReference);

    // Dispatch and commit
    vkCmdDispatch(commandBuffer, WIDTH / 32, HEIGHT / 32, 1);
}