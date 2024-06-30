#include "vulkanApplication.h"
#include <iostream>

/**
 *  Creates the acceleration structure building pipeline (compute).
 */
void VulkanApplication::createAsbuildPipeline() {
	// Load compute shader
	auto            compShaderCode = readFile("../resources/shaders/asbuild.spv");
	VkShaderModule  compShaderModule = createShaderModule(compShaderCode);
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
    pipelineLayoutInfo.pSetLayouts = &asbuildBundle.descriptorSetLayout;

    //VkPushConstantRange pushConstants = VkPushConstantRange{};
    //pushConstants.offset = 0;
    //pushConstants.size = asbuildPushConstantSize;
    //pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    pipelineLayoutInfo.pPushConstantRanges = nullptr;//&pushConstants;
    pipelineLayoutInfo.pushConstantRangeCount = 0;//1;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &asbuildPipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("ERR::VULKAN::CREATE_ASBUILD_PIPELINE::PIPELINE_LAYOUT_CREATION_FAILED");

	// Pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = asbuildPipelineLayout;
    pipelineInfo.stage = compShaderStageInfo;

    //create compute pipeline
	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &asbuildPipeline) != VK_SUCCESS)
		throw std::runtime_error("ERR::VULKAN::CREATE_ASBUILD_PIPELINE::PIPELINE_CREATION_FAILED");

	vkDestroyShaderModule(device, compShaderModule, nullptr);
}

/**
 *  Records the command buffer for Acceleration structure build (compute).
 */
void VulkanApplication::recordAsbuildCommandBuffer(VkCommandBuffer commandBuffer) {
    // Bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, asbuildPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, asbuildPipelineLayout, 0, 1, &asbuildBundle.descriptorSets[currentFrame], 0, nullptr);

    // TODO: MAKE THIS MODULAR INSTEAD OF FORCED STATIC CAST
    //vkCmdPushConstants(commandBuffer, asbuildPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, asbuildPushConstantSize, (RTFrame*)asbuildPushConstantReference);

    // Dispatch
    vkCmdDispatch(commandBuffer, WIDTH / 32, HEIGHT / 32, 1);
}