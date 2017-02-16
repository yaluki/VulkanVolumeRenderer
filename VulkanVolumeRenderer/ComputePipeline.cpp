#include "ComputePipeline.h"

// private

void ComputePipeline::prepareStorageBuffers(std::string path) {
	std::vector<uint32_t> voxelData;
	datastructure::loadVoxelDataFromTxt(path, &voxelData);
	datastructure::Octree* octree = new datastructure::Octree(&voxelData, glm::vec3(0.0f, 0.000001f, 0.0f), 0.001);
	octree->removeEmptyNodes();
	res.ubo.octreeData.pos = octree->pos;
	res.ubo.octreeData.voxelFreq = octree->voxelFreq;
	res.ubo.octreeData.numVoxelsSide = octree->numVoxelsSide;

	VkDeviceSize storageBufferSize = octree->numNodes() * sizeof(datastructure::Node);
	std::cout << "Octree size: " << storageBufferSize / 1000000000.0f << " GB" << std::endl;

	initStorageBuffer(octree->data(), &res.storageBuffers.voxels, storageBufferSize);
}

void ComputePipeline::initStorageBuffer(void* data, vk::Buffer* buffer, VkDeviceSize storageBufferSize) {
	vk::Buffer stagingBuffer;
	vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		storageBufferSize,
		data);

	vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		buffer,
		storageBufferSize);

	// copy to staging buffer
	VkCommandBuffer copyCmd = util::createCommandBuffer(vulkanDevice->logicalDevice, vulkanDevice->commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion = {};
	copyRegion.size = storageBufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, buffer->buffer, 1, &copyRegion);
	util::flushCommandBuffer(vulkanDevice->logicalDevice, vulkanDevice->commandPool, copyCmd, *queue, true);

	stagingBuffer.destroy();
}

void ComputePipeline::prepareUniformBuffers() {
	vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&res.uniformBuffer,
		sizeof(res.ubo));

	updateUniformBuffers(glm::mat4(0), glm::vec3(0));
}

void ComputePipeline::prepareTextureTarget(vkTools::VulkanTexture *tex, uint32_t width, uint32_t height, VkFormat format) {
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, format, &formatProperties);
	// checks if requested image format supports image storage operations
	assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

	// prepare target texture
	tex->width = width;
	tex->height = height;

	VkImageCreateInfo imageCreateInfo = vkTools::initializers::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// the fragment shader samples the image and the compute shader uses it as storage target
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageCreateInfo.flags = 0;

	VkMemoryAllocateInfo memAllocInfo = vkTools::initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &tex->image));
	vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, tex->image, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &tex->deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, tex->image, tex->deviceMemory, 0));

	VkCommandBuffer layoutCmd = util::createCommandBuffer(vulkanDevice->logicalDevice, vulkanDevice->commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	tex->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	vkTools::setImageLayout(
		layoutCmd,
		tex->image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		tex->imageLayout);

	util::flushCommandBuffer(vulkanDevice->logicalDevice, vulkanDevice->commandPool, layoutCmd, *queue, true);

	VkSamplerCreateInfo sampler = vkTools::initializers::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 0;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = 0.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &tex->sampler));

	VkImageViewCreateInfo view = vkTools::initializers::imageViewCreateInfo();
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = format;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	view.image = tex->image;
	VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &view, nullptr, &tex->view));

	// initialize a descriptor for later use
	tex->descriptor.imageLayout = tex->imageLayout;
	tex->descriptor.imageView = tex->view;
	tex->descriptor.sampler = tex->sampler;
}

void ComputePipeline::buildComputeCommandBuffer(vkTools::VulkanTexture *textureComputeTarget) {
	VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

	VK_CHECK_RESULT(vkBeginCommandBuffer(this->res.commandBuffer, &cmdBufInfo));

	vkCmdBindPipeline(this->res.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, this->res.pipeline);
	vkCmdBindDescriptorSets(this->res.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, this->res.pipelineLayout, 0, 1, &this->res.descriptorSet, 0, 0);

	vkCmdDispatch(this->res.commandBuffer, textureComputeTarget->width / 16, textureComputeTarget->height / 16, 1);

	vkEndCommandBuffer(this->res.commandBuffer);
}

// public
ComputePipeline::ComputePipeline(vk::VulkanDevice *vulkanDevice, VkQueue *queue) {
	this->vulkanDevice = vulkanDevice;
	this->queue = queue;
}

ComputePipeline::~ComputePipeline() {
	vkDestroyShaderModule(vulkanDevice->logicalDevice, this->shaderModule, nullptr);
	vkDestroyPipeline(vulkanDevice->logicalDevice, this->res.pipeline, nullptr);
	vkDestroyPipelineLayout(vulkanDevice->logicalDevice, this->res.pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, this->res.descriptorSetLayout, nullptr);
	vkDestroyFence(vulkanDevice->logicalDevice, this->res.fence, nullptr);
	vkDestroyCommandPool(vulkanDevice->logicalDevice, this->res.commandPool, nullptr);
	this->res.uniformBuffer.destroy();
	this->res.storageBuffers.voxels.destroy();
}

void ComputePipeline::prepare(std::string path, vkTools::VulkanTexture *tex, uint32_t width, uint32_t height) {
	prepareStorageBuffers(path);
	prepareUniformBuffers();
	prepareTextureTarget(tex, width, height, VK_FORMAT_R8G8B8A8_SNORM);
}

void ComputePipeline::updateUniformBuffers(glm::mat4 viewMat, glm::vec3 pos) {
	res.ubo.viewMat = viewMat;
	res.ubo.camera.pos = pos;
	VK_CHECK_RESULT(this->res.uniformBuffer.map());
	memcpy(res.uniformBuffer.mapped, &res.ubo, sizeof(res.ubo));
	res.uniformBuffer.unmap();
}

void ComputePipeline::prepareCompute(vkTools::VulkanTexture *textureComputeTarget, VkDescriptorPool *descriptorPool, VkPipelineCache* pipelineCache) {
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.pNext = NULL;
	queueCreateInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
	queueCreateInfo.queueCount = 1;
	vkGetDeviceQueue(vulkanDevice->logicalDevice, vulkanDevice->queueFamilyIndices.compute, 0, &res.queue);

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// binding 0: storage image (raytraced output)
		vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_SHADER_STAGE_COMPUTE_BIT,
			0),
		// binding 1: uniform buffer block
		vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_COMPUTE_BIT,
			1),
		// binding 2: shader storage buffer for the voxels
		vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_COMPUTE_BIT,
			2)
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout =
		vkTools::initializers::descriptorSetLayoutCreateInfo(
			setLayoutBindings.data(),
			setLayoutBindings.size());

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayout, nullptr, &this->res.descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vkTools::initializers::pipelineLayoutCreateInfo(
			&res.descriptorSetLayout,
			1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanDevice->logicalDevice, &pPipelineLayoutCreateInfo, nullptr, &this->res.pipelineLayout));

	VkDescriptorSetAllocateInfo allocInfo =
		vkTools::initializers::descriptorSetAllocateInfo(
			*descriptorPool,
			&res.descriptorSetLayout,
			1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &this->res.descriptorSet));

	std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets =
	{
		// binding 0: output storage image
		vkTools::initializers::writeDescriptorSet(
			res.descriptorSet,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			0,
			&textureComputeTarget->descriptor),
		// binding 1: uniform buffer block
		vkTools::initializers::writeDescriptorSet(
			res.descriptorSet,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			&res.uniformBuffer.descriptor),
		// binding 2: shader storage buffer for the voxels
		vkTools::initializers::writeDescriptorSet(
			res.descriptorSet,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			2,
			&res.storageBuffers.voxels.descriptor)
	};

	vkUpdateDescriptorSets(vulkanDevice->logicalDevice, computeWriteDescriptorSets.size(), computeWriteDescriptorSets.data(), 0, NULL);

	// create compute shader pipelines
	VkComputePipelineCreateInfo computePipelineCreateInfo =
		vkTools::initializers::computePipelineCreateInfo(
			res.pipelineLayout,
			0);

	computePipelineCreateInfo.stage = util::loadShader(vulkanDevice->logicalDevice, util::getAssetPath() + "shaders/raytracing/raytracing.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT, NULL);
	this->shaderModule = computePipelineCreateInfo.stage.module;
	VK_CHECK_RESULT(vkCreateComputePipelines(vulkanDevice->logicalDevice, *pipelineCache, 1, &computePipelineCreateInfo, nullptr, &this->res.pipeline));

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(vulkanDevice->logicalDevice, &cmdPoolInfo, nullptr, &this->res.commandPool));

	// create a command buffer for compute operations
	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::initializers::commandBufferAllocateInfo(
			res.commandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &cmdBufAllocateInfo, &this->res.commandBuffer));

	// fence for synchronization
	VkFenceCreateInfo fenceCreateInfo = vkTools::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK_RESULT(vkCreateFence(vulkanDevice->logicalDevice, &fenceCreateInfo, nullptr, &this->res.fence));

	// build a single command buffer containing the compute dispatch commands
	buildComputeCommandBuffer(textureComputeTarget);
}