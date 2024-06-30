#include "vulkanApplication.h"

void VulkanApplication::drawFrame() {
    // --- Compute & AS Build
    // Wait until previous iteration of current in-flight-frame is finished
    VkResult res = vkWaitForFences(device, 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    if (res != VK_SUCCESS) throw std::runtime_error("ERR::VULKAN::DRAW_FRAME::UNEXPECTED_WAIT_ERROR");
    vkResetFences(device, 1, &computeInFlightFences[currentFrame]);

    // Reset command buffer and record acceleration build commands
    VkCommandBuffer computeBuffer = computeCommandBuffers[currentFrame];

    vkResetCommandBuffer( computeBuffer, 0 );
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer( computeBuffer, &beginInfo ) != VK_SUCCESS)
        throw std::runtime_error("ERR::VULKAN::DRAW_FRAME::COMMAND_BUFFER_BEGIN_FAILED");

    recordAsbuildCommandBuffer( computeBuffer );

    // Insert barrier to make sure AS build has completed writing to storage buffers before tracing begins
    VkMemoryBarrier2KHR asbuildComputeBarrier{};
    asbuildComputeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;
    asbuildComputeBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
    asbuildComputeBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
    asbuildComputeBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
    asbuildComputeBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR;

    VkDependencyInfoKHR dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &asbuildComputeBarrier;
    vkCmdPipelineBarrier2( computeBuffer, &dependencyInfo );

    // Record trace commands and commit
    recordComputeCommandBuffer( computeBuffer );
    if (vkEndCommandBuffer( computeBuffer ) != VK_SUCCESS)
        throw std::runtime_error("ERR::VULKAN::DRAW_FRAME::COMMAND_BUFFER_COMMIT_FAILED");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &computeBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];
    if (vkQueueSubmit( computeQueue, 1, &submitInfo, computeInFlightFences[currentFrame] ) != VK_SUCCESS)
        throw std::runtime_error("ERR::VULKAN::DRAW_FRAME::SUBMIT_COMPUTE_QUEUE_FAILED");
    
    // --- Graphics
    /*  // MODEL
        - Wait for the previous frame to finish
        - Acquire an image from the swap chain
        - Record a command buffer which draws the scene onto that image
        - Submit the recorded command buffer
        - Present the swap chain image
    */

    // Wait for previous frame to finish (CPU fence, no timeout)
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // Fetch image from swapchain
    uint32_t imageIndex;
    res = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        // If the swapchain was out of date for presentation, cancel presentation and recreate it
        recreateSwapChain();
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swap chain image!");

    // Reset fence right before we are about to record command buffer
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // Record command buffer
    //make sure its empty
    vkResetCommandBuffer(graphicsCommandBuffers[currentFrame], 0);
    //rerecord
    recordGraphicsCommandBuffer(graphicsCommandBuffers[currentFrame], imageIndex);

    // Submit command buffer
    submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { computeFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame] };
    //for now, there is risk of vertex shader running before img available
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 2;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &graphicsCommandBuffers[currentFrame];

    // Tell Vulkan which semaphores to signal once command buffer is done executing
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // Submit command buffer to graphics queue!
    // (in the future there should be an array of many command buffers, not just one)
    auto err = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
    if (err != VK_SUCCESS)
        throw std::runtime_error("failed to submit draw command buffer!");

    // Presentation
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    res = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    }
    else if (res != VK_SUCCESS)
        throw std::runtime_error("failed to present swap chain image!");

    // Advance to next frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}