#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>

#include "vulkantools.h"
#include "vulkandevice.hpp"
#include "vulkanTextureLoader.hpp"

#include "Octree.hpp"
#include "DatastructureCreator.hpp"
#include "utility.hpp"

class ComputePipeline {

private:
	vk::VulkanDevice *vulkanDevice;
	VkQueue *queue;

	// save for cleanup
	VkShaderModule shaderModule = VK_NULL_HANDLE;

	// prepares the compute shader storage buffer containing the volumetric data set
	void prepareStorageBuffers(std::string path);

	void initStorageBuffer(void* data, vk::Buffer* buffer, VkDeviceSize storageBufferSize);

	// prepares the uniform buffer containing shader uniforms
	void prepareUniformBuffers();

	// prepares the texture target that is used to store the rendering of the compute shader
	void prepareTextureTarget(vkTools::VulkanTexture *tex, uint32_t width, uint32_t height, VkFormat format);

	void buildComputeCommandBuffer(vkTools::VulkanTexture *textureComputeTarget);

public:
	struct Resources {
		struct StorageBuffers {
			vk::Buffer voxels;
		} storageBuffers;
		vk::Buffer uniformBuffer;					// scene data
		VkQueue queue;								// queue for compute commands
		VkCommandPool commandPool;					// compute command pool
		VkCommandBuffer commandBuffer;				// stores the dispatch commands and barriers
		VkFence fence;								// fence to avoid rewriting compute CB if still in use
		VkDescriptorSetLayout descriptorSetLayout;	// compute shader binding layout
		VkDescriptorSet descriptorSet;				// compute shader bindings
		VkPipelineLayout pipelineLayout;			// layout of the compute pipeline
		VkPipeline pipeline;						// compute raytracing pipeline
		struct UBOCompute {							// compute shader uniform block object
			glm::vec3 lightPos;
			float aspectRatio;
			glm::mat4 viewMat = glm::mat4(0.0f);
			struct OctreeData {
				glm::vec3 pos;
				float voxelFreq;
				int32_t numVoxelsSide;
				float _pad[3];
			} octreeData;
			struct Camera {
				glm::vec3 pos = glm::vec3(0.0f, 0.0f, 4.0f);
				glm::vec3 lookat = glm::vec3(0.0f, 0.5f, 0.0f);
				float fov = 10.0f;
			} camera;
		} ubo;
	} res;

	ComputePipeline(vk::VulkanDevice *vulkanDevice,
		VkQueue *queue);

	~ComputePipeline();

	void prepare(std::string path, vkTools::VulkanTexture *tex, uint32_t width, uint32_t height);

	void updateUniformBuffers(glm::mat4 viewMat, glm::vec3 pos);

	// prepare the compute pipeline that generates the ray traced image
	void prepareCompute(vkTools::VulkanTexture *textureComputeTarget, VkDescriptorPool *descriptorPool, VkPipelineCache* pipelineCache);
};

