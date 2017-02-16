#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:console")
#endif

#include <iostream>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <array>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkantools.h"
#include "vulkandebug.h"

#include "vulkandevice.hpp"
#include "vulkanswapchain.hpp"
#include "vulkanTextureLoader.hpp"
#include "vulkantextoverlay.hpp"
#include "Camera.hpp"
#include "utility.hpp"

// function pointer for getting physical device features to be enabled
typedef VkPhysicalDeviceFeatures(*PFN_GetEnabledFeatures)();

class VulkanBase {
private:
	bool enableValidation = false;
	bool enableVSync = false;
	VkPhysicalDeviceFeatures enabledFeatures = {};
	// fps timer (1 sec interval)
	float fpsTimer = 0.0f; 
	double tDelta = 0.0;
	bool viewUpdated = false;
	uint32_t destWidth;
	uint32_t destHeight;
	bool resizing = false;

	VkResult createInstance(bool enableValidation);
	std::string getWindowTitle();
	void windowResize();

protected:
	float frameTimer = 1.0f;
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	VkDevice device;
	vk::VulkanDevice *vulkanDevice;
	// command queue for the graphics pipeline
	VkQueue queue;
	VkFormat colorformat = VK_FORMAT_B8G8R8A8_UNORM;
	VkFormat depthFormat;
	VkCommandPool cmdPool;
	VkCommandBuffer setupCmdBuffer = VK_NULL_HANDLE;
	// graphics pipeline stages
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; 
	// contains command buffers and semaphores to be presented to the queue
	VkSubmitInfo submitInfo;
	// command buffers used for rendering
	std::vector<VkCommandBuffer> drawCmdBuffers;
	// global render pass for frame buffer writes
	VkRenderPass renderPass;
	// list of available frame buffers (same as number of swap chain images)
	std::vector<VkFramebuffer>frameBuffers;
	// active frame buffer index
	uint32_t currentBuffer = 0;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkShaderModule> shaderModules; // stored for cleanup
	VkPipelineCache pipelineCache;
	VulkanSwapChain swapChain;

	// synchronization semaphores
	struct {
		VkSemaphore presentComplete; // swap chain image presentation
		VkSemaphore renderComplete; // command buffer submission and execution
		VkSemaphore textOverlayComplete; // text overlay submission and execution
	} semaphores;

	vkTools::VulkanTextureLoader *textureLoader = nullptr;

public:
	bool prepared = false;
	uint32_t width = 1920;
	uint32_t height = 1080;

	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	float zoom = 0;

	// frame rate independent timer (value: [-1.0;1.0])
	float timer = 0.0f;
	// global timer
	float timerSpeed = 0.25f;

	bool paused = false;

	bool enableTextOverlay = false;
	VulkanTextOverlay *textOverlay;

	// mouse rotation speed
	float rotationSpeed = 1.0f;
	// mouse zoom speed
	float zoomSpeed = 1.0f;

	Camera camera;

	glm::vec3 rotation = glm::vec3();
	glm::vec3 cameraPos = glm::vec3();
	glm::vec2 mousePos;

	std::string title = "Vulkan Application";
	std::string name = "vulkanApplication";

	struct {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} depthStencil;

	GLFWwindow* window;

	VulkanBase(bool enableValidation, PFN_GetEnabledFeatures enabledFeaturesFn = nullptr);
	~VulkanBase();

	// sets up the vulkan instance, enables required extensions and connects to the physical device.
	void initVulkan(bool enableValidation);

	void setupConsole(std::string title);
	void initWindow();
	static void handleKeyEvent(GLFWwindow * window, int key, int scancode, int action, int mods);
	static void handleMousePosEvent(GLFWwindow* window, double xpos, double ypos);
	static void handleScrollEvent(GLFWwindow* window, double xoffset, double yoffset);
	static void onWindowResized(GLFWwindow* window, int width, int height);

	// virtual render function (override in derived class)
	virtual void render(double tDelta) = 0;

	// creates a new command pool object storing command buffers
	void createCommandPool();
	// setup default depth and stencil views
	void setupDepthStencil();
	// create framebuffers for all requested swap chain images
	void setupFrameBuffer();
	// setup a default render pass
	void setupRenderPass();

	// connect and prepare the swap chain
	void initSwapchain();
	// create the swap chain images
	void setupSwapChain();

	// check if command buffers are valid (!= VK_NULL_HANDLE)
	bool checkCommandBuffers();
	// create command buffers for drawing commands
	void createCommandBuffers();
	// destroy all command buffers and set their handles to VK_NULL_HANDLE
	void destroyCommandBuffers();
	// create command buffer for setup commands
	void createSetupCommandBuffer();
	// finalize setup command buffer, submit it to the queue and remove it
	void flushSetupCommandBuffer();

	// should be called if the framebuffer has to be rebuilt and thus all command buffers that reference it (e.g. resizing the window)
	virtual void buildCommandBuffers();

	// create a cache pool for rendering pipelines
	void createPipelineCache();

	// prepare core Vulkan functions
	virtual void prepare();

	void renderLoop();

	void updateTextOverlay();

	// submit the frames workload and if enabled, the text overlay
	void submitFrame();
};

