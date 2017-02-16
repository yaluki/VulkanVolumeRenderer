#include "VulkanBase.h"

VkResult VulkanBase::createInstance(bool enableValidation) {
	this->enableValidation = enableValidation;

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = name.c_str();
	appInfo.pEngineName = name.c_str();
	appInfo.apiVersion = VK_API_VERSION_1_0;

	std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

#if defined(_WIN32)
	enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
	enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (enabledExtensions.size() > 0) {
		if (enableValidation) {
			enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	}
	if (enableValidation) {
		instanceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount;
		instanceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	}
	return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

std::string VulkanBase::getWindowTitle() {
	std::string device(deviceProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device;
	if (!enableTextOverlay) {
		windowTitle += " - " + std::to_string(frameCounter) + " fps";
	}
	return windowTitle;
}

bool VulkanBase::checkCommandBuffers() {
	for (auto& cmdBuffer : drawCmdBuffers) {
		if (cmdBuffer == VK_NULL_HANDLE) {
			return false;
		}
	}
	return true;
}

void VulkanBase::createCommandBuffers() {
	// create one command buffer for each swap chain image and reuse it for rendering
	drawCmdBuffers.resize(swapChain.imageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::initializers::commandBufferAllocateInfo(
			cmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			static_cast<uint32_t>(drawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
}

void VulkanBase::destroyCommandBuffers() {
	vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

void VulkanBase::createSetupCommandBuffer() {
	if (setupCmdBuffer != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(device, cmdPool, 1, &setupCmdBuffer);
		setupCmdBuffer = VK_NULL_HANDLE;
	}

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::initializers::commandBufferAllocateInfo(
			cmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &setupCmdBuffer));

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VK_CHECK_RESULT(vkBeginCommandBuffer(setupCmdBuffer, &cmdBufInfo));
}

void VulkanBase::flushSetupCommandBuffer() {
	if (setupCmdBuffer == VK_NULL_HANDLE)
		return;

	VK_CHECK_RESULT(vkEndCommandBuffer(setupCmdBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &setupCmdBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(queue));

	vkFreeCommandBuffers(device, cmdPool, 1, &setupCmdBuffer);
	setupCmdBuffer = VK_NULL_HANDLE;
}

void VulkanBase::buildCommandBuffers() {
	// should be overwritten in derived class
}

void VulkanBase::createPipelineCache() {
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanBase::prepare() {
	if (vulkanDevice->enableDebugMarkers) {
		vkDebug::DebugMarker::setup(device);
	}
	createCommandPool();
	createSetupCommandBuffer();
	setupSwapChain();
	createCommandBuffers();
	setupDepthStencil();
	setupRenderPass();
	createPipelineCache();
	setupFrameBuffer();
	flushSetupCommandBuffer();
	createSetupCommandBuffer();

	textureLoader = new vkTools::VulkanTextureLoader(vulkanDevice, queue, cmdPool);

	if (enableTextOverlay) {
		// load the text rendering shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.push_back(util::loadShader(device, util::getAssetPath() + "shaders/base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, &shaderModules));
		shaderStages.push_back(util::loadShader(device, util::getAssetPath() + "shaders/base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, &shaderModules));
		textOverlay = new VulkanTextOverlay(
			vulkanDevice,
			queue,
			frameBuffers,
			colorformat,
			depthFormat,
			&width,
			&height,
			shaderStages
		);
		updateTextOverlay();
	}
}

void VulkanBase::renderLoop() {
	destWidth = width;
	destHeight = height;
	while (!glfwWindowShouldClose(window)) {
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated) {
			viewUpdated = false;
		}

		glfwPollEvents();

		render(tDelta);
		frameCounter++;
		auto tEnd = std::chrono::high_resolution_clock::now();
		tDelta = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer = (float)tDelta / 1000.0f;
		camera.update(frameTimer);
		if (camera.moving()) {
			viewUpdated = true;
		}

		if (!paused) {
			timer += timerSpeed * frameTimer;
			if (timer > 1.0) {
				timer -= 1.0f;
			}
		}
		fpsTimer += (float)tDelta;
		if (fpsTimer > 1000.0f) {
			if (!enableTextOverlay) {
				std::string windowTitle = getWindowTitle();
			}
			lastFPS = roundf(1.0f / frameTimer);
			updateTextOverlay();
			fpsTimer = 0.0f;
			frameCounter = 0;
		}
	}
	// flush device, thus all resources can be freed 
	vkDeviceWaitIdle(device);
}

void VulkanBase::updateTextOverlay() {
	if (!enableTextOverlay)
		return;

	textOverlay->beginTextUpdate();

	textOverlay->addText(title, 5.0f, 5.0f, VulkanTextOverlay::alignLeft);

	std::stringstream ss;
	ss << std::fixed << std::setprecision(3) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
	textOverlay->addText(ss.str(), 5.0f, 25.0f, VulkanTextOverlay::alignLeft);

	textOverlay->addText(deviceProperties.deviceName, 5.0f, 45.0f, VulkanTextOverlay::alignLeft);

	textOverlay->endTextUpdate();
}

void VulkanBase::submitFrame() {
	bool submitTextOverlay = enableTextOverlay && textOverlay->visible;

	if (submitTextOverlay) {
		// wait for color attachment output to finish before rendering the text overlay
		VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.pWaitDstStageMask = &stageFlags;

		// set semaphores
		// wait for render complete semaphore
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &semaphores.renderComplete;
		// signal ready with text overlay complete semaphpre
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &semaphores.textOverlayComplete;

		// submit current text overlay command buffer
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &textOverlay->cmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		// reset stage mask
		submitInfo.pWaitDstStageMask = &submitPipelineStages;
		// reset wait and signal semaphores for rendering next frame
		// wait for swap chain presentation to finish
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &semaphores.presentComplete;
		// signal ready with offscreen semaphore
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &semaphores.renderComplete;
	}

	VK_CHECK_RESULT(swapChain.queuePresent(queue, currentBuffer, submitTextOverlay ? semaphores.textOverlayComplete : semaphores.renderComplete));

	VK_CHECK_RESULT(vkQueueWaitIdle(queue));
}

VulkanBase::VulkanBase(bool enableValidation, PFN_GetEnabledFeatures enabledFeaturesFn) {
	if (enabledFeaturesFn != nullptr) {
		this->enabledFeatures = enabledFeaturesFn();
	}

#if defined(_WIN32)
	if (enableValidation) {
		setupConsole("Validation Console");
	}
#endif
	initVulkan(enableValidation);
}

VulkanBase::~VulkanBase() {
	swapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}
	if (setupCmdBuffer != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(device, cmdPool, 1, &setupCmdBuffer);

	}
	destroyCommandBuffers();
	vkDestroyRenderPass(device, renderPass, nullptr);
	for (uint32_t i = 0; i < frameBuffers.size(); i++) {
		vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
	}

	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
	vkDestroyImageView(device, depthStencil.view, nullptr);
	vkDestroyImage(device, depthStencil.image, nullptr);
	vkFreeMemory(device, depthStencil.mem, nullptr);

	vkDestroyPipelineCache(device, pipelineCache, nullptr);

	if (textureLoader) {
		delete textureLoader;
	}

	vkDestroyCommandPool(device, cmdPool, nullptr);

	vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
	vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
	vkDestroySemaphore(device, semaphores.textOverlayComplete, nullptr);

	if (enableTextOverlay) {
		delete textOverlay;
	}

	delete vulkanDevice;

	if (enableValidation) {
		vkDebug::freeDebugCallback(instance);
	}

	vkDestroyInstance(instance, nullptr);
}

void VulkanBase::initVulkan(bool enableValidation) {
	VkResult err;

	err = createInstance(enableValidation);
	if (err) {
		vkTools::exitFatal("Could not create Vulkan instance : \n" + vkTools::errorString(err), "Fatal error");
	}

	if (enableValidation) {
		VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		vkDebug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
	}

	// Physical device
	uint32_t gpuCount = 0;
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
	assert(gpuCount > 0);
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
	if (err) {
		vkTools::exitFatal("Could not enumerate phyiscal devices : \n" + vkTools::errorString(err), "Fatal error");
	}

	// always use the first device
	physicalDevice = physicalDevices[0];

	// Vulkan logical device
	vulkanDevice = new vk::VulkanDevice(physicalDevice);
	VK_CHECK_RESULT(vulkanDevice->createLogicalDevice(enabledFeatures));
	device = vulkanDevice->logicalDevice;

	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	// get a graphics queue
	vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

	// find a suitable depth format
	VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(physicalDevice, &depthFormat);
	assert(validDepthFormat);

	swapChain.connect(instance, physicalDevice, device);

	// create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkTools::initializers::semaphoreCreateInfo();
	// ensures that the image presentation is finished before new commands are submitted to the queue
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
	// ensures that all commands are finished before presenting the image
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));
	// ensures that all commands are finished before presenting the text overlay
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.textOverlayComplete));

	// basic submit info structure
	submitInfo = vkTools::initializers::submitInfo();
	submitInfo.pWaitDstStageMask = &submitPipelineStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &semaphores.presentComplete;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphores.renderComplete;
}

#if defined(_WIN32)
// sets up a console and redirects standard output to it
void VulkanBase::setupConsole(std::string title) {
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	FILE *stream;
	freopen_s(&stream, "CONOUT$", "w+", stdout);
	SetConsoleTitle(TEXT(title.c_str()));
}
#endif

void VulkanBase::initWindow() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

	glfwSetWindowUserPointer(window, this);
	glfwSetKeyCallback(window, handleKeyEvent);
	glfwSetCursorPosCallback(window, handleMousePosEvent);
	glfwSetScrollCallback(window, handleScrollEvent);
	glfwSetWindowSizeCallback(window, onWindowResized);
}

void VulkanBase::handleKeyEvent(GLFWwindow* window, int key, int scancode, int action, int mods) {
	VulkanBase* app = reinterpret_cast<VulkanBase*>(glfwGetWindowUserPointer(window));

	if (action == GLFW_PRESS) {
		switch (key) {
		case GLFW_KEY_W:
			app->camera.keys.up = true;
			break;
		case GLFW_KEY_S:
			app->camera.keys.down = true;
			break;
		case GLFW_KEY_A:
			app->camera.keys.left = true;
			break;
		case GLFW_KEY_D:
			app->camera.keys.right = true;
			break;
		case GLFW_KEY_P:
			app->paused = !app->paused;
			break;
		case GLFW_KEY_F1:
			if (app->enableTextOverlay) {
				app->textOverlay->visible = !app->textOverlay->visible;
			}
			break;
		}
	} else if (action == GLFW_RELEASE) {
		switch (key) {
		case GLFW_KEY_W:
			app->camera.keys.up = false;
			break;
		case GLFW_KEY_S:
			app->camera.keys.down = false;
			break;
		case GLFW_KEY_A:
			app->camera.keys.left = false;
			break;
		case GLFW_KEY_D:
			app->camera.keys.right = false;
			break;
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;
		}
	}
}

void VulkanBase::handleMousePosEvent(GLFWwindow * window, double xpos, double ypos) {
	VulkanBase* app = reinterpret_cast<VulkanBase*>(glfwGetWindowUserPointer(window));
	if (glfwGetMouseButton(app->window, GLFW_MOUSE_BUTTON_LEFT)) {
		app->rotation.x += (app->mousePos.y - (float)ypos) * 0.75f * app->rotationSpeed;
		app->rotation.y += (app->mousePos.x - (float)xpos) * 0.75f * app->rotationSpeed;
		app->camera.rotate(glm::vec3((app->mousePos.y - (float)ypos) * app->camera.rotationSpeed, -(app->mousePos.x - (float)xpos) * app->camera.rotationSpeed, 0.0f));
		app->viewUpdated = true;
	}
	app->mousePos = glm::vec2((float)xpos, (float)ypos);
}

void VulkanBase::handleScrollEvent(GLFWwindow * window, double xoffset, double yoffset) {
	VulkanBase* app = reinterpret_cast<VulkanBase*>(glfwGetWindowUserPointer(window));

	app->zoom += yoffset * 0.005f * app->zoomSpeed;
	app->camera.translate(glm::vec3(0.0f, 0.0f, yoffset * 0.005f * app->zoomSpeed));
	app->viewUpdated = true;
}

void VulkanBase::onWindowResized(GLFWwindow* window, int width, int height) {
	VulkanBase* app = reinterpret_cast<VulkanBase*>(glfwGetWindowUserPointer(window));

	app->destWidth = width;
	app->destHeight = height;
	app->windowResize();
}

void VulkanBase::createCommandPool() {
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
}

void VulkanBase::setupDepthStencil() {
	VkImageCreateInfo image = {};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.pNext = NULL;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = depthFormat;
	image.extent = { width, height, 1 };
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image.flags = 0;

	VkMemoryAllocateInfo mem_alloc = {};
	mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc.pNext = NULL;
	mem_alloc.allocationSize = 0;
	mem_alloc.memoryTypeIndex = 0;

	VkImageViewCreateInfo depthStencilView = {};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.pNext = NULL;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = depthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;

	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &depthStencil.image));
	vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);
	mem_alloc.allocationSize = memReqs.size;
	mem_alloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &mem_alloc, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

	depthStencilView.image = depthStencil.image;
	VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &depthStencil.view));
}

void VulkanBase::setupFrameBuffer() {
	VkImageView attachments[2];

	// same depth/stencil attachment for all frame buffers
	attachments[1] = depthStencil.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = renderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	frameBufferCreateInfo.layers = 1;

	// create frame buffers for every swap chain image
	frameBuffers.resize(swapChain.imageCount);
	for (uint32_t i = 0; i < frameBuffers.size(); i++) {
		attachments[0] = swapChain.buffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
	}
}

void VulkanBase::setupRenderPass() {
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = colorformat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void VulkanBase::windowResize() {
	if (!prepared) {
		return;
	}
	prepared = false;

	// recreate swap chain
	width = destWidth;
	height = destHeight;
	createSetupCommandBuffer();
	setupSwapChain();

	// recreate frame buffers
	vkDestroyImageView(device, depthStencil.view, nullptr);
	vkDestroyImage(device, depthStencil.image, nullptr);
	vkFreeMemory(device, depthStencil.mem, nullptr);
	setupDepthStencil();

	for (uint32_t i = 0; i < frameBuffers.size(); i++) {
		vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
	}
	setupFrameBuffer();

	flushSetupCommandBuffer();

	// recreate command buffers (may reference recreated frame buffer)
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkQueueWaitIdle(queue);
	vkDeviceWaitIdle(device);

	if (enableTextOverlay) {
		textOverlay->reallocateCommandBuffers();
		updateTextOverlay();
	}

	camera.updateAspectRatio((float)width / (float)height);

	prepared = true;
}

void VulkanBase::initSwapchain() {
	swapChain.initSurface(window);
}

void VulkanBase::setupSwapChain() {
	swapChain.create(&width, &height, enableVSync);
}
