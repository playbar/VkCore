#include "VkTexture.h"

std::vector<const char*> VkTexture::args;

VkResult VkTexture::createInstance(bool enableValidation)
{
	this->mEnableValidation = enableValidation;

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = name.c_str();
	appInfo.pEngineName = name.c_str();
	appInfo.apiVersion = VK_API_VERSION_1_0;

	std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

	// Enable surface extensions depending on os
#if defined(_WIN32)
	enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__ANDROID__)
	enabledExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
	enabledExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(__linux__)
	enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (enabledExtensions.size() > 0)
	{
		if (enableValidation)
		{
			enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	}
	if (enableValidation)
	{
		instanceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount;
		instanceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	}
	return vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance);
}

std::string VkTexture::getWindowTitle()
{
	std::string device(gVulkanDevice->mProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device;
	if (!mEnableTextOverlay)
	{
		windowTitle += " - " + std::to_string(frameCounter) + " fps";
	}
	return windowTitle;
}

const std::string VkTexture::getAssetPath()
{
#if defined(__ANDROID__)
	return "";
#else
	return "./../data/";
#endif
}

bool VkTexture::checkCommandBuffers()
{
	for (auto& cmdBuffer : mDrawCmdBuffers)
	{
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			return false;
		}
	}
	return true;
}

void VkTexture::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	mDrawCmdBuffers.resize(gSwapChain.mImageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::commandBufferAllocateInfo(
			mCmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			static_cast<uint32_t>(mDrawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(gVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, mDrawCmdBuffers.data()));
}

void VkTexture::destroyCommandBuffers()
{
	vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, static_cast<uint32_t>(mDrawCmdBuffers.size()), mDrawCmdBuffers.data());
}

void VkTexture::createSetupCommandBuffer()
{
	if (setupCmdBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, 1, &setupCmdBuffer);
		setupCmdBuffer = VK_NULL_HANDLE; // todo : check if still necessary
	}

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::commandBufferAllocateInfo(
			mCmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(gVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, &setupCmdBuffer));

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VK_CHECK_RESULT(vkBeginCommandBuffer(setupCmdBuffer, &cmdBufInfo));
}

void VkTexture::flushSetupCommandBuffer()
{
	if (setupCmdBuffer == VK_NULL_HANDLE)
		return;

	VK_CHECK_RESULT(vkEndCommandBuffer(setupCmdBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &setupCmdBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(mQueue));

	vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, 1, &setupCmdBuffer);
	setupCmdBuffer = VK_NULL_HANDLE; 
}

VkCommandBuffer VkTexture::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::commandBufferAllocateInfo(
			mCmdPool,
			level,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(gVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

void VkTexture::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
	if (commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}
	
	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(queue));

	if (free)
	{
		vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, 1, &commandBuffer);
	}
}

void VkTexture::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(gVulkanDevice->mLogicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VkTexture::prepare()
{
	if (gVulkanDevice->mEnableDebugMarkers)
	{
		vkDebug::DebugMarker::setup(gVulkanDevice->mLogicalDevice);
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
	// Recreate setup command buffer for derived class
	createSetupCommandBuffer();
	// Create a simple texture loader class
	textureLoader = new vkTools::VulkanTextureLoader(gVulkanDevice, mQueue, mCmdPool);
#if defined(__ANDROID__)
	textureLoader->assetManager = androidApp->activity->assetManager;
#endif
	if (mEnableTextOverlay)
	{
		// Load the text rendering shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
		mTextOverlay = new VulkanTextOverlay(
			gVulkanDevice,
			mQueue,
			mFrameBuffers,
			mColorformat,
			mDepthFormat,
			&width,
			&height,
			shaderStages
			);
		updateTextOverlay();
	}

	generateQuad();
	setupVertexDescriptions();
	prepareUniformBuffers();
	loadTexture(
		//getAssetPath() + "textures/test.jpg",
		getAssetPath() + "textures/pattern_02_bc2.ktx",
		VK_FORMAT_BC2_UNORM_BLOCK,
		false);
	setupDescriptorSetLayout();
	preparePipelines();
	setupDescriptorPool();
	setupDescriptorSet();
	buildCommandBuffers();
	prepared = true;
}

VkPipelineShaderStageCreateInfo VkTexture::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
#if defined(__ANDROID__)
	shaderStage.module = vkTools::loadShader(androidApp->activity->assetManager, fileName.c_str(), gVulkanDevice->mLogicalDevice, stage);
#else
	shaderStage.module = vkTools::loadShader(fileName.c_str(), gVulkanDevice->mLogicalDevice, stage);
#endif
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != NULL);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

VkBool32 VkTexture::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory)
{
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc = vkTools::memoryAllocateInfo();
	VkBufferCreateInfo bufferCreateInfo = vkTools::bufferCreateInfo(usageFlags, size);

	VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &bufferCreateInfo, nullptr, buffer));

	vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, *buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = gVulkanDevice->getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, memory));
	if (data != nullptr)
	{
		void *mapped;
		VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, *memory, 0, size, 0, &mapped));
		memcpy(mapped, data, size);
		vkUnmapMemory(gVulkanDevice->mLogicalDevice, *memory);
	}
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, *buffer, *memory, 0));

	return true;
}

VkBool32 VkTexture::createBuffer(VkBufferUsageFlags usage, VkDeviceSize size, void * data, VkBuffer *buffer, VkDeviceMemory *memory)
{
	return createBuffer(usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data, buffer, memory);
}

VkBool32 VkTexture::createBuffer(VkBufferUsageFlags usage, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor)
{
	VkBool32 res = createBuffer(usage, size, data, buffer, memory);
	if (res)
	{
		descriptor->offset = 0;
		descriptor->buffer = *buffer;
		descriptor->range = size;
		return true;
	}
	else
	{
		return false;
	}
}

VkBool32 VkTexture::createBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor)
{
	VkBool32 res = createBuffer(usage, memoryPropertyFlags, size, data, buffer, memory);
	if (res)
	{
		descriptor->offset = 0;
		descriptor->buffer = *buffer;
		descriptor->range = size;
		return true;
	}
	else
	{
		return false;
	}
}

void VkTexture::loadMesh(std::string filename, vkMeshLoader::MeshBuffer * meshBuffer, std::vector<vkMeshLoader::VertexLayout> vertexLayout, float scale)
{
	vkMeshLoader::MeshCreateInfo meshCreateInfo;
	meshCreateInfo.scale = glm::vec3(scale);
	meshCreateInfo.center = glm::vec3(0.0f);
	meshCreateInfo.uvscale = glm::vec2(1.0f);
	loadMesh(filename, meshBuffer, vertexLayout, &meshCreateInfo);
}

void VkTexture::loadMesh(std::string filename, vkMeshLoader::MeshBuffer * meshBuffer, std::vector<vkMeshLoader::VertexLayout> vertexLayout, vkMeshLoader::MeshCreateInfo *meshCreateInfo)
{
	VulkanMeshLoader *mesh = new VulkanMeshLoader(gVulkanDevice);

#if defined(__ANDROID__)
	mesh->assetManager = androidApp->activity->assetManager;
#endif

	mesh->LoadMesh(filename);
	assert(mesh->m_Entries.size() > 0);

	VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	mesh->createBuffers(
		meshBuffer,
		vertexLayout,
		meshCreateInfo,
		true,
		copyCmd,
		mQueue);

	vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, 1, &copyCmd);

	meshBuffer->dim = mesh->dim.size;

	delete(mesh);
}

void VkTexture::renderLoop()
{
	destWidth = width;
	destHeight = height;
	MSG msg;
	while (TRUE)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated)
		{
			viewUpdated = false;
			viewChanged();
		}

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		render();
		frameCounter++;
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer = (float)tDiff / 1000.0f;
		mCamera.update(frameTimer);
		if (mCamera.moving())
		{
			viewUpdated = true;
		}
		// Convert to clamped timer value
		if (!paused)
		{
			timer += timerSpeed * frameTimer;
			if (timer > 1.0)
			{
				timer -= 1.0f;
			}
		}
		fpsTimer += (float)tDiff;
		if (fpsTimer > 1000.0f)
		{
			if (!mEnableTextOverlay)
			{
				std::string windowTitle = getWindowTitle();
				SetWindowText(mHwndWinow, windowTitle.c_str());
			}
			lastFPS = roundf(1.0f / frameTimer);
			updateTextOverlay();
			fpsTimer = 0.0f;
			frameCounter = 0;
		}
	}

	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(gVulkanDevice->mLogicalDevice);
}

void VkTexture::updateTextOverlay()
{
	if (!mEnableTextOverlay)
		return;

	mTextOverlay->beginTextUpdate();

	mTextOverlay->addText(title, 5.0f, 5.0f, VulkanTextOverlay::alignLeft);

	std::stringstream ss;
	ss << std::fixed << std::setprecision(3) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
	mTextOverlay->addText(ss.str(), 5.0f, 25.0f, VulkanTextOverlay::alignLeft);

	mTextOverlay->addText(gVulkanDevice->mProperties.deviceName, 5.0f, 45.0f, VulkanTextOverlay::alignLeft);

	getOverlayText(mTextOverlay);

	mTextOverlay->endTextUpdate();
}

void VkTexture::prepareFrame()
{
	// Acquire the next image from the swap chaing
	VK_CHECK_RESULT(gSwapChain.acquireNextImage(semaphores.presentComplete));
}

void VkTexture::submitFrame()
{
	bool submitTextOverlay = mEnableTextOverlay && mTextOverlay->mVisible;

	if (submitTextOverlay)
	{
		// Wait for color attachment output to finish before rendering the text overlay
		VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		mSubmitInfo.pWaitDstStageMask = &stageFlags;

		// Set semaphores
		// Wait for render complete semaphore
		mSubmitInfo.waitSemaphoreCount = 1;
		mSubmitInfo.pWaitSemaphores = &semaphores.renderComplete;
		// Signal ready with text overlay complete semaphpre
		mSubmitInfo.signalSemaphoreCount = 1;
		mSubmitInfo.pSignalSemaphores = &semaphores.textOverlayComplete;

		// Submit current text overlay command buffer
		mSubmitInfo.commandBufferCount = 1;
		mSubmitInfo.pCommandBuffers = &mTextOverlay->mCmdBuffers[gSwapChain.mCurrentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		// Reset stage mask
		mSubmitInfo.pWaitDstStageMask = &submitPipelineStages;
		// Reset wait and signal semaphores for rendering next frame
		// Wait for swap chain presentation to finish
		mSubmitInfo.waitSemaphoreCount = 1;
		mSubmitInfo.pWaitSemaphores = &semaphores.presentComplete;
		// Signal ready with offscreen semaphore
		mSubmitInfo.signalSemaphoreCount = 1;
		mSubmitInfo.pSignalSemaphores = &semaphores.renderComplete;
	}

	VK_CHECK_RESULT(gSwapChain.queuePresent(mQueue, submitTextOverlay ? semaphores.textOverlayComplete : semaphores.renderComplete));

	VK_CHECK_RESULT(vkQueueWaitIdle(mQueue));
}

VkTexture::VkTexture(bool enableValidation, PFN_GetEnabledFeatures enabledFeaturesFn)
{
	mZoom = -2.5f;
	mRotation = { 0.0f, 15.0f, 0.0f };
	title = "VkCoreTexturing";
	mEnableTextOverlay = true;

	// Parse command line arguments
	for (auto arg : args)
	{
		if (arg == std::string("-validation"))
		{
			enableValidation = true;
		}
		if (arg == std::string("-vsync"))
		{
			enableVSync = true;
		}
	}
#if defined(__ANDROID__)
	// Vulkan library is loaded dynamically on Android
	bool libLoaded = loadVulkanLibrary();
	assert(libLoaded);
#elif defined(_DIRECT2DISPLAY)

#elif defined(__linux__)
	initxcbConnection();
#endif

	if (enabledFeaturesFn != nullptr)
	{
		this->enabledFeatures = enabledFeaturesFn();
	}

#if defined(_WIN32)
	// Enable console if validation is active
	// Debug message callback will output to it
	if (enableValidation)
	{
		setupConsole("VulkanExample");
	}
#endif

#if !defined(__ANDROID__)
	// Android Vulkan initialization is handled in APP_CMD_INIT_WINDOW event
	initVulkan(enableValidation);
#endif
}

VkTexture::~VkTexture()
{
	// Clean up used Vulkan resources 
	// Note : Inherited destructor cleans up resources stored in base class

	destroyTextureImage(mTexture);

	vkDestroyPipeline(gVulkanDevice->mLogicalDevice, mPipelines.solid, nullptr);

	vkDestroyPipelineLayout(gVulkanDevice->mLogicalDevice, mPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(gVulkanDevice->mLogicalDevice, mDescriptorSetLayout, nullptr);

	mVertexBuffer.destroy();
	mIndexBuffer.destroy();
	mUniformBufferVS.destroy();

	// Clean up Vulkan resources
	gSwapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(gVulkanDevice->mLogicalDevice, descriptorPool, nullptr);
	}
	if (setupCmdBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, 1, &setupCmdBuffer);

	}
	destroyCommandBuffers();
	vkDestroyRenderPass(gVulkanDevice->mLogicalDevice, mRenderPass, nullptr);
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(gVulkanDevice->mLogicalDevice, mFrameBuffers[i], nullptr);
	}

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(gVulkanDevice->mLogicalDevice, shaderModule, nullptr);
	}
	vkDestroyImageView(gVulkanDevice->mLogicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(gVulkanDevice->mLogicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, depthStencil.mem, nullptr);

	vkDestroyPipelineCache(gVulkanDevice->mLogicalDevice, pipelineCache, nullptr);

	if (textureLoader)
	{
		delete textureLoader;
	}

	vkDestroyCommandPool(gVulkanDevice->mLogicalDevice, mCmdPool, nullptr);

	vkDestroySemaphore(gVulkanDevice->mLogicalDevice, semaphores.presentComplete, nullptr);
	vkDestroySemaphore(gVulkanDevice->mLogicalDevice, semaphores.renderComplete, nullptr);
	vkDestroySemaphore(gVulkanDevice->mLogicalDevice, semaphores.textOverlayComplete, nullptr);

	if (mEnableTextOverlay)
	{
		delete mTextOverlay;
	}

	delete gVulkanDevice;

	if (mEnableValidation)
	{
		vkDebug::freeDebugCallback(mInstance);
	}
	vkDestroyInstance(mInstance, nullptr);

}


// Create an image memory barrier for changing the layout of
// an image and put it into an active command buffer
void VkTexture::setImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange)
{
	// Create an image barrier object
	VkImageMemoryBarrier imageMemoryBarrier = vkTools::imageMemoryBarrier();;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	// Only sets masks for layouts used in this example
	// For a more complete version that can be used with other layouts see vkTools::setImageLayout

	// Source layouts (old)
	switch (oldImageLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		// Only valid as initial layout, memory contents are not preserved
		// Can be accessed directly, no source dependency required
		imageMemoryBarrier.srcAccessMask = 0;
		break;
	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Only valid as initial layout for linear images, preserves memory contents
		// Make sure host writes to the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Old layout is transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;
	}

	// Target layouts (new)
	switch (newImageLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Transfer source (copy, blit)
		// Make sure any reads from the image have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Transfer destination (copy, blit)
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Shader read (sampler, input attachment)
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	}

	// Put barrier on top of pipeline
	VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(
		cmdBuffer,
		srcStageFlags,
		destStageFlags,
		VK_FLAGS_NONE,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
}

void VkTexture::loadTexture(std::string fileName, VkFormat format, bool forceLinearTiling)
{
#if defined(__ANDROID__)
	// Textures are stored inside the apk on Android (compressed)
	// So they need to be loaded via the asset manager
	AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, fileName.c_str(), AASSET_MODE_STREAMING);
	assert(asset);
	size_t size = AAsset_getLength(asset);
	assert(size > 0);

	void *textureData = malloc(size);
	AAsset_read(asset, textureData, size);
	AAsset_close(asset);

	gli::texture2D tex2D(gli::load((const char*)textureData, size));
#else
	gli::texture2D tex2D(gli::load(fileName));
#endif

	assert(!tex2D.empty());

	VkFormatProperties formatProperties;

	mTexture.width = static_cast<uint32_t>(tex2D[0].dimensions().x);
	mTexture.height = static_cast<uint32_t>(tex2D[0].dimensions().y);
	mTexture.mipLevels = static_cast<uint32_t>(tex2D.levels());

	// Get device properites for the requested texture format
	vkGetPhysicalDeviceFormatProperties(gVulkanDevice->mPhysicalDevice, format, &formatProperties);

	// Only use linear tiling if requested (and supported by the device)
	// Support for linear tiling is mostly limited, so prefer to use
	// optimal tiling instead
	// On most implementations linear tiling will only support a very
	// limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
	VkBool32 useStaging = true;

	// Only use linear tiling if forced
	if (forceLinearTiling)
	{
		// Don't use linear if format is not supported for (linear) shader sampling
		useStaging = !(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
	}

	VkMemoryAllocateInfo memAllocInfo = vkTools::memoryAllocateInfo();
	VkMemoryRequirements memReqs = {};

	if (useStaging)
	{
		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vkTools::bufferCreateInfo();
		bufferCreateInfo.size = tex2D.size();
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = gVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, tex2D.data(), tex2D.size());
		vkUnmapMemory(gVulkanDevice->mLogicalDevice, stagingMemory);

		// Setup buffer copy regions for each mip level
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		uint32_t offset = 0;

		for (uint32_t i = 0; i < mTexture.mipLevels; i++)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = i;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex2D[i].dimensions().x);
			bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex2D[i].dimensions().y);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);

			offset += static_cast<uint32_t>(tex2D[i].size());
		}

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vkTools::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = mTexture.mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		// Set initial layout of the image to undefined
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { mTexture.width, mTexture.height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		VK_CHECK_RESULT(vkCreateImage(gVulkanDevice->mLogicalDevice, &imageCreateInfo, nullptr, &mTexture.image));

		vkGetImageMemoryRequirements(gVulkanDevice->mLogicalDevice, mTexture.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = gVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAllocInfo, nullptr, &mTexture.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(gVulkanDevice->mLogicalDevice, mTexture.image, mTexture.deviceMemory, 0));

		VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Image barrier for optimal image

		// The sub resource range describes the regions of the image we will be transition
		VkImageSubresourceRange subresourceRange = {};
		// Image only contains color data
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		// Start at first mip level
		subresourceRange.baseMipLevel = 0;
		// We will transition on all mip levels
		subresourceRange.levelCount = mTexture.mipLevels;
		// The 2D texture only has one layer
		subresourceRange.layerCount = 1;

		// Optimal image will be used as destination for the copy, so we must transfer from our
		// initial undefined image layout to the transfer destination layout
		setImageLayout(
			copyCmd,
			mTexture.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy mip levels from staging buffer
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			mTexture.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data());

		// Change texture image layout to shader read after all mip levels have been copied
		mTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		setImageLayout(
			copyCmd,
			mTexture.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			mTexture.imageLayout,
			subresourceRange);

		flushCommandBuffer(copyCmd, mQueue, true);

		// Clean up staging resources
		vkFreeMemory(gVulkanDevice->mLogicalDevice, stagingMemory, nullptr);
		vkDestroyBuffer(gVulkanDevice->mLogicalDevice, stagingBuffer, nullptr);
	}
	else
	{
		// Prefer using optimal tiling, as linear tiling 
		// may support only a small set of features 
		// depending on implementation (e.g. no mip maps, only one layer, etc.)

		VkImage mappableImage;
		VkDeviceMemory mappableMemory;

		// Load mip map level 0 to linear tiling image
		VkImageCreateInfo imageCreateInfo = vkTools::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		imageCreateInfo.extent = { mTexture.width, mTexture.height, 1 };
		VK_CHECK_RESULT(vkCreateImage(gVulkanDevice->mLogicalDevice, &imageCreateInfo, nullptr, &mappableImage));

		// Get memory requirements for this image 
		// like size and alignment
		vkGetImageMemoryRequirements(gVulkanDevice->mLogicalDevice, mappableImage, &memReqs);
		// Set memory allocation size to required memory size
		memAllocInfo.allocationSize = memReqs.size;

		// Get memory type that can be mapped to host memory
		memAllocInfo.memoryTypeIndex = gVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Allocate host memory
		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAllocInfo, nullptr, &mappableMemory));

		// Bind allocated image for use
		VK_CHECK_RESULT(vkBindImageMemory(gVulkanDevice->mLogicalDevice, mappableImage, mappableMemory, 0));

		// Get sub resource layout
		// Mip map count, array layer, etc.
		VkImageSubresource subRes = {};
		subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkSubresourceLayout subResLayout;
		void *data;

		// Get sub resources layout 
		// Includes row pitch, size offsets, etc.
		vkGetImageSubresourceLayout(gVulkanDevice->mLogicalDevice, mappableImage, &subRes, &subResLayout);

		// Map image memory
		VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mappableMemory, 0, memReqs.size, 0, &data));

		// Copy image data into memory
		memcpy(data, tex2D[subRes.mipLevel].data(), tex2D[subRes.mipLevel].size());

		vkUnmapMemory(gVulkanDevice->mLogicalDevice, mappableMemory);

		// Linear tiled images don't need to be staged
		// and can be directly used as textures
		mTexture.image = mappableImage;
		mTexture.deviceMemory = mappableMemory;
		mTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Setup image memory barrier transfer image to shader read layout

		// The sub resource range describes the regions of the image we will be transition
		VkImageSubresourceRange subresourceRange = {};
		// Image only contains color data
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		// Start at first mip level
		subresourceRange.baseMipLevel = 0;
		// Only one mip level, most implementations won't support more for linear tiled images
		subresourceRange.levelCount = 1;
		// The 2D texture only has one layer
		subresourceRange.layerCount = 1;

		setImageLayout(
			copyCmd,
			mTexture.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_PREINITIALIZED,
			mTexture.imageLayout,
			subresourceRange);

		flushCommandBuffer(copyCmd, mQueue, true);
	}

	// Create sampler
	// In Vulkan textures are accessed by samplers
	// This separates all the sampling information from the 
	// texture data
	// This means you could have multiple sampler objects
	// for the same texture with different settings
	// Similar to the samplers available with OpenGL 3.3
	VkSamplerCreateInfo samplerCreateInfo = vkTools::samplerCreateInfo();
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	// Set max level-of-detail to mip level count of the texture
	samplerCreateInfo.maxLod = (useStaging) ? (float)mTexture.mipLevels : 0.0f;
	// Enable anisotropic filtering
	// This feature is optional, so we must check if it's supported on the device
	if (gVulkanDevice->mFeatures.samplerAnisotropy)
	{
		// Use max. level of anisotropy for this example
		samplerCreateInfo.maxAnisotropy = gVulkanDevice->mProperties.limits.maxSamplerAnisotropy;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
	}
	else
	{
		// The device does not support anisotropic filtering
		samplerCreateInfo.maxAnisotropy = 1.0;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
	}
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(gVulkanDevice->mLogicalDevice, &samplerCreateInfo, nullptr, &mTexture.sampler));

	// Create image view
	// Textures are not directly accessed by the shaders and
	// are abstracted by image views containing additional
	// information and sub resource ranges
	VkImageViewCreateInfo viewCreateInfo = vkTools::imageViewCreateInfo();
	viewCreateInfo.image = VK_NULL_HANDLE;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	// The subresource range describes the set of mip levels (and array layers) that can be accessed through this image view
	// It's possible to create multiple image views for a single image referring to different (and/or overlapping) ranges of the image
	viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;
	// Linear tiling usually won't support mip maps
	// Only set mip map count if optimal tiling is used
	viewCreateInfo.subresourceRange.levelCount = (useStaging) ? mTexture.mipLevels : 1;
	viewCreateInfo.image = mTexture.image;
	VK_CHECK_RESULT(vkCreateImageView(gVulkanDevice->mLogicalDevice, &viewCreateInfo, nullptr, &mTexture.imageView));

	// Fill image descriptor image info that can be used during the descriptor set setup
	mTexture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	mTexture.descriptor.imageView = mTexture.imageView;
	mTexture.descriptor.sampler = mTexture.sampler;
}

// Free all Vulkan resources used a texture object
void VkTexture::destroyTextureImage(Texture texture)
{
	vkDestroyImageView(gVulkanDevice->mLogicalDevice, texture.imageView, nullptr);
	vkDestroyImage(gVulkanDevice->mLogicalDevice, texture.image, nullptr);
	vkDestroySampler(gVulkanDevice->mLogicalDevice, texture.sampler, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, texture.deviceMemory, nullptr);
}

void VkTexture::buildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = vkTools::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = defaultClearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vkTools::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = mRenderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (int32_t i = 0; i < mDrawCmdBuffers.size(); ++i)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = mFrameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(mDrawCmdBuffers[i], &cmdBufInfo));

		vkCmdBeginRenderPass(mDrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkTools::viewport((float)width, (float)height, 0.0f, 1.0f);
		vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkTools::rect2D(width, height, 0, 0);
		vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, NULL);
		vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.solid);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &mVertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(mDrawCmdBuffers[i], mIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(mDrawCmdBuffers[i], mIndexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(mDrawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
	}
}

void VkTexture::draw()
{
	prepareFrame();

	// Command buffer to be sumitted to the queue
	mSubmitInfo.commandBufferCount = 1;
	mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[gSwapChain.mCurrentBuffer];

	// Submit to queue
	VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

	submitFrame();
}

void VkTexture::generateQuad()
{
	// Setup vertices for a single uv-mapped quad made from two triangles
	std::vector<Vertex> vertices =
	{
		{ { 1.0f,  1.0f, 0.0f },{ 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
		{ { -1.0f,  1.0f, 0.0f },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
		{ { -1.0f, -1.0f, 0.0f },{ 0.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } },
		{ { 1.0f, -1.0f, 0.0f },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
	};

	// Setup indices
	std::vector<uint32_t> indices = { 0,1,2, 2,3,0 };
	mIndexCount = static_cast<uint32_t>(indices.size());

	// Create buffers
	// For the sake of simplicity we won't stage the vertex data to the gpu memory
	// Vertex buffer
	VK_CHECK_RESULT(gVulkanDevice->createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&mVertexBuffer,
		vertices.size() * sizeof(Vertex),
		vertices.data()));
	// Index buffer
	VK_CHECK_RESULT(gVulkanDevice->createBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&mIndexBuffer,
		indices.size() * sizeof(uint32_t),
		indices.data()));
}

void VkTexture::setupVertexDescriptions()
{
	// Binding description
	mVertices.bindingDescriptions.resize(1);
	mVertices.bindingDescriptions[0] =
		vkTools::vertexInputBindingDescription(
			VERTEX_BUFFER_BIND_ID,
			sizeof(Vertex),
			VK_VERTEX_INPUT_RATE_VERTEX);

	// Attribute descriptions
	// Describes memory layout and shader positions
	mVertices.attributeDescriptions.resize(3);
	// Location 0 : Position
	mVertices.attributeDescriptions[0] =
		vkTools::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			0,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Vertex, pos));
	// Location 1 : Texture coordinates
	mVertices.attributeDescriptions[1] =
		vkTools::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			1,
			VK_FORMAT_R32G32_SFLOAT,
			offsetof(Vertex, uv));
	// Location 1 : Vertex normal
	mVertices.attributeDescriptions[2] =
		vkTools::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			2,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Vertex, normal));

	mVertices.inputState = vkTools::pipelineVertexInputStateCreateInfo();
	mVertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(mVertices.bindingDescriptions.size());
	mVertices.inputState.pVertexBindingDescriptions = mVertices.bindingDescriptions.data();
	mVertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(mVertices.attributeDescriptions.size());
	mVertices.inputState.pVertexAttributeDescriptions = mVertices.attributeDescriptions.data();
}

void VkTexture::setupDescriptorPool()
{
	// Example uses one ubo and one image sampler
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
		vkTools::descriptorPoolCreateInfo(
			static_cast<uint32_t>(poolSizes.size()),
			poolSizes.data(),
			2);

	VK_CHECK_RESULT(vkCreateDescriptorPool(gVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void VkTexture::setupDescriptorSetLayout()
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
	{
		// Binding 0 : Vertex shader uniform buffer
		vkTools::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT,
			0),
		// Binding 1 : Fragment shader image sampler
		vkTools::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			1)
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout =
		vkTools::descriptorSetLayoutCreateInfo(
			setLayoutBindings.data(),
			static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(gVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vkTools::pipelineLayoutCreateInfo(
			&mDescriptorSetLayout,
			1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(gVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
}

void VkTexture::setupDescriptorSet()
{
	VkDescriptorSetAllocateInfo allocInfo =
		vkTools::descriptorSetAllocateInfo(
			descriptorPool,
			&mDescriptorSetLayout,
			1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(gVulkanDevice->mLogicalDevice, &allocInfo, &mDescriptorSet));

	std::vector<VkWriteDescriptorSet> writeDescriptorSets =
	{
		// Binding 0 : Vertex shader uniform buffer
		vkTools::writeDescriptorSet(
			mDescriptorSet,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			0,
			&mUniformBufferVS.descriptor),
		// Binding 1 : Fragment shader texture sampler
		vkTools::writeDescriptorSet(
			mDescriptorSet,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			&mTexture.descriptor)
	};

	vkUpdateDescriptorSets(gVulkanDevice->mLogicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void VkTexture::preparePipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vkTools::pipelineInputAssemblyStateCreateInfo(
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			0,
			VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vkTools::pipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_NONE,
			VK_FRONT_FACE_COUNTER_CLOCKWISE,
			0);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
		vkTools::pipelineColorBlendAttachmentState(
			0xf,
			VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendState =
		vkTools::pipelineColorBlendStateCreateInfo(
			1,
			&blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		vkTools::pipelineDepthStencilStateCreateInfo(
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewportState =
		vkTools::pipelineViewportStateCreateInfo(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisampleState =
		vkTools::pipelineMultisampleStateCreateInfo(
			VK_SAMPLE_COUNT_1_BIT,
			0);

	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState =
		vkTools::pipelineDynamicStateCreateInfo(
			dynamicStateEnables.data(),
			static_cast<uint32_t>(dynamicStateEnables.size()),
			0);

	// Load shaders
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	shaderStages[0] = loadShader(getAssetPath() + "shaders/texture/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/texture/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		vkTools::pipelineCreateInfo(
			mPipelineLayout,
			mRenderPass,
			0);

	pipelineCreateInfo.pVertexInputState = &mVertices.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(gVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipelines.solid));
}

// Prepare and initialize uniform buffer containing shader uniforms
void VkTexture::prepareUniformBuffers()
{
	// Vertex shader uniform buffer block
	VK_CHECK_RESULT(gVulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&mUniformBufferVS,
		sizeof(mUboVS),
		&mUboVS));

	updateUniformBuffers();
}

void VkTexture::updateUniformBuffers()
{
	Matrix viewMatrix, matTmp;
	Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), (float)width / (float)height, 0.001f, 256.0f, &mUboVS.projection);
	viewMatrix.translate(0.0f, 0.0f, mZoom);
	// Vertex shader
	//mUboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.001f, 256.0f);
	//glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, mZoom));

	matTmp.translate(cameraPos);
	mUboVS.model = viewMatrix *matTmp;
	mUboVS.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
	mUboVS.model.rotateY(MATH_DEG_TO_RAD(mRotation.y));
	mUboVS.model.rotateZ(MATH_DEG_TO_RAD(mRotation.z));

	//mUboVS.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
	//mUboVS.model = glm::rotate(mUboVS.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	//mUboVS.model = glm::rotate(mUboVS.model, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	//mUboVS.model = glm::rotate(mUboVS.model, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	mUboVS.viewPos = Vector4(0.0f, 0.0f, -mZoom, 0.0f);

	VK_CHECK_RESULT(mUniformBufferVS.map());
	memcpy(mUniformBufferVS.mapped, &mUboVS, sizeof(mUboVS));
	mUniformBufferVS.unmap();
}


void VkTexture::render()
{
	if (!prepared)
		return;
	draw();
}

void VkTexture::viewChanged()
{
	updateUniformBuffers();
}

void VkTexture::changeLodBias(float delta)
{
	mUboVS.lodBias += delta;
	if (mUboVS.lodBias < 0.0f)
	{
		mUboVS.lodBias = 0.0f;
	}
	if (mUboVS.lodBias > mTexture.mipLevels)
	{
		mUboVS.lodBias = (float)mTexture.mipLevels;
	}
	updateUniformBuffers();
	updateTextOverlay();
}

void VkTexture::keyPressed(uint32_t keyCode)
{
	switch (keyCode)
	{
	case Keyboard::KEY_KPADD:
	case GAMEPAD_BUTTON_R1:
		changeLodBias(0.1f);
		break;
	case Keyboard::KEY_KPSUB:
	case GAMEPAD_BUTTON_L1:
		changeLodBias(-0.1f);
		break;
	}
}

void VkTexture::getOverlayText(VulkanTextOverlay *textOverlay)
{
	std::stringstream ss;
	ss << std::setprecision(2) << std::fixed << mUboVS.lodBias;
#if defined(__ANDROID__)
	textOverlay->addText("LOD bias: " + ss.str() + " (Buttons L1/R1 to change)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#else
	textOverlay->addText("LOD bias: " + ss.str() + " (numpad +/- to change)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
}

void VkTexture::initVulkan(bool enableValidation)
{
	VkResult err;

	// Vulkan instance
	err = createInstance(enableValidation);
	if (err)
	{
		vkTools::exitFatal("Could not create Vulkan instance : \n" + vkTools::errorString(err), "Fatal error");
	}

#if defined(__ANDROID__)
	loadVulkanFunctions(mInstance);
#endif

	// If requested, we enable the default validation layers for debugging
	if (enableValidation)
	{
		// The report flags determine what type of messages for the layers will be displayed
		// For validating (debugging) an appplication the error and warning bits should suffice
		VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT; // | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		// Additional flags include performance info, loader and layer debug messages, etc.
		vkDebug::setupDebugging(mInstance, debugReportFlags, VK_NULL_HANDLE);
	}

	// Physical device
	uint32_t gpuCount = 0;
	// Get number of available physical devices
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr));
	assert(gpuCount > 0);
	// Enumerate devices
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	err = vkEnumeratePhysicalDevices(mInstance, &gpuCount, physicalDevices.data());
	if (err)
	{
		vkTools::exitFatal("Could not enumerate phyiscal devices : \n" + vkTools::errorString(err), "Fatal error");
	}

	// Note :
	// This example will always use the first physical device reported,
	// change the vector index if you have multiple Vulkan devices installed
	// and want to use another one
	//mPhysicalDevice = physicalDevices[0];

	// Vulkan device creation
	// This is handled by a separate class that gets a logical device representation
	// and encapsulates functions related to a device
	gVulkanDevice = new VkCoreDevice(physicalDevices[0]);
	VK_CHECK_RESULT(gVulkanDevice->createLogicalDevice(enabledFeatures));

	// Get a graphics queue from the device
	vkGetDeviceQueue(gVulkanDevice->mLogicalDevice, gVulkanDevice->queueFamilyIndices.graphics, 0, &mQueue);

	// Find a suitable depth format
	VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(gVulkanDevice->mPhysicalDevice, &mDepthFormat);
	assert(validDepthFormat);

	gSwapChain.connect(mInstance, gVulkanDevice->mPhysicalDevice, gVulkanDevice->mLogicalDevice);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkTools::semaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
	VK_CHECK_RESULT(vkCreateSemaphore(gVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	VK_CHECK_RESULT(vkCreateSemaphore(gVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
	// Will be inserted after the render complete semaphore if the text overlay is enabled
	VK_CHECK_RESULT(vkCreateSemaphore(gVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &semaphores.textOverlayComplete));

	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	mSubmitInfo = vkTools::submitInfo();
	mSubmitInfo.pWaitDstStageMask = &submitPipelineStages;
	mSubmitInfo.waitSemaphoreCount = 1;
	mSubmitInfo.pWaitSemaphores = &semaphores.presentComplete;
	mSubmitInfo.signalSemaphoreCount = 1;
	mSubmitInfo.pSignalSemaphores = &semaphores.renderComplete;
}

// Win32 : Sets up a console window and redirects standard output to it
void VkTexture::setupConsole(std::string title)
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	FILE *stream;
	freopen_s(&stream, "CONOUT$", "w+", stdout);
	SetConsoleTitle(TEXT(title.c_str()));
}

HWND VkTexture::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
{
	this->mWindowInstance = hinstance;

	bool fullscreen = false;
	for (auto arg : args)
	{
		if (arg == std::string("-fullscreen"))
		{
			fullscreen = true;
		}
	}

	WNDCLASSEX wndClass;

	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = wndproc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hinstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = name.c_str();
	wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

	if (!RegisterClassEx(&wndClass))
	{
		std::cout << "Could not register window class!\n";
		fflush(stdout);
		exit(1);
	}

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	if (fullscreen)
	{
		DEVMODE dmScreenSettings;
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth = screenWidth;
		dmScreenSettings.dmPelsHeight = screenHeight;
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		if ((width != screenWidth) && (height != screenHeight))
		{
			if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
				{
					fullscreen = FALSE;
				}
				else
				{
					return FALSE;
				}
			}
		}

	}

	DWORD dwExStyle;
	DWORD dwStyle;

	if (fullscreen)
	{
		dwExStyle = WS_EX_APPWINDOW;
		dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	}
	else
	{
		dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	}

	RECT windowRect;
	windowRect.left = 0L;
	windowRect.top = 0L;
	windowRect.right = fullscreen ? (long)screenWidth : (long)width;
	windowRect.bottom = fullscreen ? (long)screenHeight : (long)height;

	AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

	std::string windowTitle = getWindowTitle();
	mHwndWinow = CreateWindowEx(0,
		name.c_str(),
		windowTitle.c_str(),
		dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0,
		0,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		NULL,
		NULL,
		hinstance,
		NULL);

	if (!fullscreen)
	{
		// Center on screen
		uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
		uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
		SetWindowPos(mHwndWinow, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}

	if (!mHwndWinow)
	{
		printf("Could not create window!\n");
		fflush(stdout);
		return 0;
		exit(1);
	}

	ShowWindow(mHwndWinow, SW_SHOW);
	SetForegroundWindow(mHwndWinow);
	SetFocus(mHwndWinow);

	return mHwndWinow;
}

void VkTexture::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		prepared = false;
		DestroyWindow(hWnd);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
		ValidateRect(mHwndWinow, NULL);
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case Keyboard::KEY_P:
			paused = !paused;
			break;
		case Keyboard::KEY_F1:
			if (mEnableTextOverlay)
			{
				mTextOverlay->mVisible = !mTextOverlay->mVisible;
			}
			break;
		case Keyboard::KEY_ESCAPE:
			PostQuitMessage(0);
			break;
		}

		if (mCamera.firstperson)
		{
			switch (wParam)
			{
			case Keyboard::KEY_W:
				mCamera.keys.up = true;
				break;
			case Keyboard::KEY_S:
				mCamera.keys.down = true;
				break;
			case Keyboard::KEY_A:
				mCamera.keys.left = true;
				break;
			case Keyboard::KEY_D:
				mCamera.keys.right = true;
				break;
			}
		}

		keyPressed((uint32_t)wParam);
		break;
	case WM_KEYUP:
		if (mCamera.firstperson)
		{
			switch (wParam)
			{
			case Keyboard::KEY_W:
				mCamera.keys.up = false;
				break;
			case Keyboard::KEY_S:
				mCamera.keys.down = false;
				break;
			case Keyboard::KEY_A:
				mCamera.keys.left = false;
				break;
			case Keyboard::KEY_D:
				mCamera.keys.right = false;
				break;
			}
		}
		break;
	case WM_RBUTTONDOWN:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
		mousePos.x = (float)LOWORD(lParam);
		mousePos.y = (float)HIWORD(lParam);
		break;
	case WM_MOUSEWHEEL:
	{
		short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		mZoom += (float)wheelDelta * 0.005f * zoomSpeed;
		mCamera.translate(Vector3(0.0f, 0.0f, (float)wheelDelta * 0.005f * zoomSpeed));
		viewUpdated = true;
		break;
	}
	case WM_MOUSEMOVE:
		if (wParam & MK_RBUTTON)
		{
			int32_t posx = LOWORD(lParam);
			int32_t posy = HIWORD(lParam);
			mZoom += (mousePos.y - (float)posy) * .005f * zoomSpeed;
			mCamera.translate(Vector3(-0.0f, 0.0f, (mousePos.y - (float)posy) * .005f * zoomSpeed));
			mousePos = Vector2((float)posx, (float)posy);
			viewUpdated = true;
		}
		if (wParam & MK_LBUTTON)
		{
			int32_t posx = LOWORD(lParam);
			int32_t posy = HIWORD(lParam);
			mRotation.x += (mousePos.y - (float)posy) * 1.25f * rotationSpeed;
			mRotation.y -= (mousePos.x - (float)posx) * 1.25f * rotationSpeed;
			mCamera.rotate(Vector3((mousePos.y - (float)posy) * mCamera.rotationSpeed, -(mousePos.x - (float)posx) * mCamera.rotationSpeed, 0.0f));
			mousePos = Vector2((float)posx, (float)posy);
			viewUpdated = true;
		}
		if (wParam & MK_MBUTTON)
		{
			int32_t posx = LOWORD(lParam);
			int32_t posy = HIWORD(lParam);
			cameraPos.x -= (mousePos.x - (float)posx) * 0.01f;
			cameraPos.y -= (mousePos.y - (float)posy) * 0.01f;
			mCamera.translate(Vector3(-(mousePos.x - (float)posx) * 0.01f, -(mousePos.y - (float)posy) * 0.01f, 0.0f));
			viewUpdated = true;
			mousePos.x = (float)posx;
			mousePos.y = (float)posy;
		}
		break;
	case WM_SIZE:
		if ((prepared) && (wParam != SIZE_MINIMIZED))
		{
			if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
			{
				destWidth = LOWORD(lParam);
				destHeight = HIWORD(lParam);
				windowResize();
			}
		}
		break;
	case WM_ENTERSIZEMOVE:
		resizing = true;
		break;
	case WM_EXITSIZEMOVE:
		resizing = false;
		break;
	}
}


void VkTexture::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = gSwapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(gVulkanDevice->mLogicalDevice, &cmdPoolInfo, nullptr, &mCmdPool));
}

void VkTexture::setupDepthStencil()
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = NULL;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = mDepthFormat;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageCreateInfo.flags = 0;

	VkMemoryAllocateInfo memAllocateInfo = {};
	memAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocateInfo.pNext = NULL;
	memAllocateInfo.allocationSize = 0;
	memAllocateInfo.memoryTypeIndex = 0;

	VkImageViewCreateInfo depthStencilView = {};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.pNext = NULL;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = mDepthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;

	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(gVulkanDevice->mLogicalDevice, &imageCreateInfo, nullptr, &depthStencil.image));
	vkGetImageMemoryRequirements(gVulkanDevice->mLogicalDevice, depthStencil.image, &memReqs);
	memAllocateInfo.allocationSize = memReqs.size;
	memAllocateInfo.memoryTypeIndex = gVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAllocateInfo, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(gVulkanDevice->mLogicalDevice, depthStencil.image, depthStencil.mem, 0));

	depthStencilView.image = depthStencil.image;
	VK_CHECK_RESULT(vkCreateImageView(gVulkanDevice->mLogicalDevice, &depthStencilView, nullptr, &depthStencil.view));
}

void VkTexture::setupFrameBuffer()
{
	VkImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depthStencil.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = mRenderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	mFrameBuffers.resize(gSwapChain.mImageCount);
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		attachments[0] = gSwapChain.buffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(gVulkanDevice->mLogicalDevice, &frameBufferCreateInfo, nullptr, &mFrameBuffers[i]));
	}
}

void VkTexture::setupRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = mColorformat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = mDepthFormat;
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

	VK_CHECK_RESULT(vkCreateRenderPass(gVulkanDevice->mLogicalDevice, &renderPassInfo, nullptr, &mRenderPass));
}

void VkTexture::windowResize()
{
	if (!prepared)
	{
		return;
	}
	prepared = false;

	// Recreate swap chain
	width = destWidth;
	height = destHeight;
	createSetupCommandBuffer();
	setupSwapChain();

	// Recreate the frame buffers

	vkDestroyImageView(gVulkanDevice->mLogicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(gVulkanDevice->mLogicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, depthStencil.mem, nullptr);
	setupDepthStencil();
	
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(gVulkanDevice->mLogicalDevice, mFrameBuffers[i], nullptr);
	}
	setupFrameBuffer();

	flushSetupCommandBuffer();

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkQueueWaitIdle(mQueue);
	vkDeviceWaitIdle(gVulkanDevice->mLogicalDevice);

	if (mEnableTextOverlay)
	{
		mTextOverlay->reallocateCommandBuffers();
		updateTextOverlay();
	}

	mCamera.updateAspectRatio((float)width / (float)height);

	// Notify derived class
	windowResized();
	viewChanged();

	prepared = true;
}

void VkTexture::windowResized()
{
	// Can be overriden in derived class
}

void VkTexture::initSwapchain()
{
#if defined(_WIN32)
	gSwapChain.initSurface(mWindowInstance, mHwndWinow);
#elif defined(__ANDROID__)	
	gSwapChain.initSurface(androidApp->window);
#elif defined(_DIRECT2DISPLAY)
	gSwapChain.initSurface(width, height);
#elif defined(__linux__)
	gSwapChain.initSurface(connection, mHwndWinow);
#endif
}

void VkTexture::setupSwapChain()
{
	gSwapChain.create(&width, &height, enableVSync);
}
