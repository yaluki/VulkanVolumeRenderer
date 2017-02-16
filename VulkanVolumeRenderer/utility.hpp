#pragma once

#include <vulkan/vulkan.h>
#include "vulkantools.h"

namespace util {
	VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool cmdPool, VkCommandBufferLevel level, bool begin);

	void flushCommandBuffer(VkDevice device, VkCommandPool cmdPool, VkCommandBuffer commandBuffer, VkQueue queue, bool free);

	VkPipelineShaderStageCreateInfo loadShader(VkDevice device, std::string fileName, VkShaderStageFlagBits stage, std::vector<VkShaderModule> *cleanupList);

	const std::string getAssetPath();
}