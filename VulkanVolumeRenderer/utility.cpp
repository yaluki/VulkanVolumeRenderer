#pragma once

#include "utility.hpp"

namespace util {
	VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool cmdPool, VkCommandBufferLevel level, bool begin) {
		VkCommandBuffer cmdBuffer;

		VkCommandBufferAllocateInfo cmdBufAllocateInfo =
			vkTools::initializers::commandBufferAllocateInfo(
				cmdPool,
				level,
				1);

		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

		// If requested, also start the new command buffer
		if (begin) {
			VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();
			VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
		}

		return cmdBuffer;
	}

	void flushCommandBuffer(VkDevice device, VkCommandPool cmdPool, VkCommandBuffer commandBuffer, VkQueue queue, bool free) {
		if (commandBuffer == VK_NULL_HANDLE) {
			return;
		}

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(vkQueueWaitIdle(queue));

		if (free) {
			vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
		}
	}

	VkPipelineShaderStageCreateInfo loadShader(VkDevice device, std::string fileName, VkShaderStageFlagBits stage, std::vector<VkShaderModule> *cleanupList) {
		VkPipelineShaderStageCreateInfo shaderStage = {};
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = stage;
		shaderStage.module = vkTools::loadShader(fileName.c_str(), device, stage);
		shaderStage.pName = "main";
		assert(shaderStage.module != NULL);
		if (cleanupList != NULL) {
			cleanupList->push_back(shaderStage.module);
		}
		return shaderStage;
	}

	const std::string getAssetPath() {
		return "./../data/";
	}
}