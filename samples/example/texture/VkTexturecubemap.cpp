#include "VkTexturecubemap.h"

std::vector<const char*> VkTexturecubemap::args;


VkTexturecubemap::VkTexturecubemap(bool enableValidation, PFN_GetEnabledFeatures enabledFeaturesFn)
{
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
	mZoom = -4.0f;
	rotationSpeed = 0.25f;
	mRotation = { -7.25f, -120.0f, 0.0f };
	mEnableTextOverlay = true;
	title = "VkCore";
}

VkTexturecubemap::~VkTexturecubemap()
{
	// Clean up texture resources
	vkDestroyImageView(mVulkanDevice->mLogicalDevice, cubeMap.view, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalDevice, cubeMap.image, nullptr);
	vkDestroySampler(mVulkanDevice->mLogicalDevice, cubeMap.sampler, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalDevice, cubeMap.deviceMemory, nullptr);

	vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.skybox, nullptr);
	vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.reflect, nullptr);

	vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayout, nullptr);

	for (size_t i = 0; i < meshes.objects.size(); i++)
	{
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.objects[i]);
	}
	vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.skybox);

	vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.objectVS);
	vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.skyboxVS);

	// Clean up Vulkan resources
	gSwapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(mVulkanDevice->mLogicalDevice, descriptorPool, nullptr);
	}
	if (setupCmdBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &setupCmdBuffer);

	}
	destroyCommandBuffers();
	vkDestroyRenderPass(mVulkanDevice->mLogicalDevice, mRenderPass, nullptr);
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(mVulkanDevice->mLogicalDevice, mFrameBuffers[i], nullptr);
	}

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(mVulkanDevice->mLogicalDevice, shaderModule, nullptr);
	}
	vkDestroyImageView(mVulkanDevice->mLogicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalDevice, depthStencil.mem, nullptr);

	vkDestroyPipelineCache(mVulkanDevice->mLogicalDevice, pipelineCache, nullptr);

	if (textureLoader)
	{
		delete textureLoader;
	}

	vkDestroyCommandPool(mVulkanDevice->mLogicalDevice, mCmdPool, nullptr);

	vkDestroySemaphore(mVulkanDevice->mLogicalDevice, semaphores.presentComplete, nullptr);
	vkDestroySemaphore(mVulkanDevice->mLogicalDevice, semaphores.renderComplete, nullptr);
	vkDestroySemaphore(mVulkanDevice->mLogicalDevice, semaphores.textOverlayComplete, nullptr);

	if (mEnableTextOverlay)
	{
		delete mTextOverlay;
	}

	delete mVulkanDevice;

	if (mEnableValidation)
	{
		vkDebug::freeDebugCallback(mInstance);
	}

	vkDestroyInstance(mInstance, nullptr);

#if defined(_DIRECT2DISPLAY)

#elif defined(__linux)
#if defined(__ANDROID__)
	// todo : android cleanup (if required)
#else
	xcb_destroy_window(connection, mHwndWinow);
	xcb_disconnect(connection);
#endif
#endif
}


void VkTexturecubemap::loadCubemap(std::string filename, VkFormat format, bool forceLinearTiling)
{
#if defined(__ANDROID__)
	// Textures are stored inside the apk on Android (compressed)
	// So they need to be loaded via the asset manager
	AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
	assert(asset);
	size_t size = AAsset_getLength(asset);
	assert(size > 0);

	void *textureData = malloc(size);
	AAsset_read(asset, textureData, size);
	AAsset_close(asset);

	gli::textureCube texCube(gli::load((const char*)textureData, size));
#else
	gli::textureCube texCube(gli::load(filename));
#endif

	assert(!texCube.empty());

	cubeMap.width = texCube.dimensions().x;
	cubeMap.height = texCube.dimensions().y;
	cubeMap.mipLevels = texCube.levels();

	VkMemoryAllocateInfo memAllocInfo = vkTools::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = vkTools::bufferCreateInfo();
	bufferCreateInfo.size = texCube.size();
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, stagingBuffer, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	// Get memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	uint8_t *data;
	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
	memcpy(data, texCube.data(), texCube.size());
	vkUnmapMemory(mVulkanDevice->mLogicalDevice, stagingMemory);

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo = vkTools::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = cubeMap.mipLevels;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent = { cubeMap.width, cubeMap.height, 1 };
	imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	// Cube faces count as array layers in Vulkan
	imageCreateInfo.arrayLayers = 6;
	// This flag is required for cube map images
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalDevice, &imageCreateInfo, nullptr, &cubeMap.image));

	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalDevice, cubeMap.image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAllocInfo, nullptr, &cubeMap.deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalDevice, cubeMap.image, cubeMap.deviceMemory, 0));

	VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Setup buffer copy regions for each face including all of it's miplevels
	std::vector<VkBufferImageCopy> bufferCopyRegions;
	uint32_t offset = 0;

	for (uint32_t face = 0; face < 6; face++)
	{
		for (uint32_t level = 0; level < cubeMap.mipLevels; level++)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = level;
			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = texCube[face][level].dimensions().x;
			bufferCopyRegion.imageExtent.height = texCube[face][level].dimensions().y;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);

			// Increase offset into staging buffer for next level / face
			offset += texCube[face][level].size();
		}
	}

	// Image barrier for optimal image (target)
	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = cubeMap.mipLevels;
	subresourceRange.layerCount = 6;

	vkTools::setImageLayout(
		copyCmd,
		cubeMap.image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// Copy the cube map faces from the staging buffer to the optimal tiled image
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		cubeMap.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data()
	);

	// Change texture image layout to shader read after all faces have been copied
	cubeMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkTools::setImageLayout(
		copyCmd,
		cubeMap.image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		cubeMap.imageLayout,
		subresourceRange);

	flushCommandBuffer(copyCmd, mQueue, true);

	// Create sampler
	VkSamplerCreateInfo sampler = vkTools::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = cubeMap.mipLevels;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sampler.maxAnisotropy = 1.0f;
	if (mVulkanDevice->mFeatures.samplerAnisotropy)
	{
		sampler.maxAnisotropy = mVulkanDevice->mProperties.limits.maxSamplerAnisotropy;
		sampler.anisotropyEnable = VK_TRUE;
	}
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->mLogicalDevice, &sampler, nullptr, &cubeMap.sampler));

	// Create image view
	VkImageViewCreateInfo view = vkTools::imageViewCreateInfo();
	// Cube map view type
	view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	view.format = format;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	// 6 array layers (faces)
	view.subresourceRange.layerCount = 6;
	// Set number of mip levels
	view.subresourceRange.levelCount = cubeMap.mipLevels;
	view.image = cubeMap.image;
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalDevice, &view, nullptr, &cubeMap.view));

	// Clean up staging resources
	vkFreeMemory(mVulkanDevice->mLogicalDevice, stagingMemory, nullptr);
	vkDestroyBuffer(mVulkanDevice->mLogicalDevice, stagingBuffer, nullptr);
}

void VkTexturecubemap::reBuildCommandBuffers()
{
	if (!checkCommandBuffers())
	{
		destroyCommandBuffers();
		createCommandBuffers();
	}
	buildCommandBuffers();
}

void VkTexturecubemap::buildCommandBuffers()
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

		VkDeviceSize offsets[1] = { 0 };

		// Skybox
		if (displaySkybox)
		{
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.skybox, 0, NULL);
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.skybox.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.skybox.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.skybox.indexCount, 1, 0, 0, 0);
		}

		// 3D object
		vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.object, 0, NULL);
		vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.objects[meshes.objectIndex].vertices.buf, offsets);
		vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.objects[meshes.objectIndex].indices.buf, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.reflect);
		vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.objects[meshes.objectIndex].indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(mDrawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
	}
}

void VkTexturecubemap::loadMeshes()
{
	// Skybox
	loadMesh(getAssetPath() + "models/cube.obj", &meshes.skybox, vertexLayout, 0.05f);
	// Objects
	meshes.objects.resize(3);
	loadMesh(getAssetPath() + "models/sphere.obj", &meshes.objects[0], vertexLayout, 0.05f);
	loadMesh(getAssetPath() + "models/teapot.dae", &meshes.objects[1], vertexLayout, 0.05f);
	loadMesh(getAssetPath() + "models/torusknot.obj", &meshes.objects[2], vertexLayout, 0.05f);
}

void VkTexturecubemap::setupVertexDescriptions()
{
	// Binding description
	vertices.bindingDescriptions.resize(1);
	vertices.bindingDescriptions[0] =
		vkTools::vertexInputBindingDescription(
			VERTEX_BUFFER_BIND_ID,
			vkMeshLoader::vertexSize(vertexLayout),
			VK_VERTEX_INPUT_RATE_VERTEX);

	// Attribute descriptions
	// Describes memory layout and shader positions
	vertices.attributeDescriptions.resize(3);
	// Location 0 : Position
	vertices.attributeDescriptions[0] =
		vkTools::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			0,
			VK_FORMAT_R32G32B32_SFLOAT,
			0);
	// Location 1 : Normal
	vertices.attributeDescriptions[1] =
		vkTools::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			1,
			VK_FORMAT_R32G32B32_SFLOAT,
			sizeof(float) * 3);
	// Location 2 : Texture coordinates
	vertices.attributeDescriptions[2] =
		vkTools::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			2,
			VK_FORMAT_R32G32_SFLOAT,
			sizeof(float) * 5);

	vertices.inputState = vkTools::pipelineVertexInputStateCreateInfo();
	vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
	vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
	vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
	vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
}

void VkTexturecubemap::setupDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
		vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
		vkTools::descriptorPoolCreateInfo(
			poolSizes.size(),
			poolSizes.data(),
			2);

	VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void VkTexturecubemap::setupDescriptorSetLayout()
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
			setLayoutBindings.size());

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vkTools::pipelineLayoutCreateInfo(
			&descriptorSetLayout,
			1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void VkTexturecubemap::setupDescriptorSets()
{
	// Image descriptor for the cube map texture
	VkDescriptorImageInfo cubeMapDescriptor =
		vkTools::descriptorImageInfo(
			cubeMap.sampler,
			cubeMap.view,
			VK_IMAGE_LAYOUT_GENERAL);

	VkDescriptorSetAllocateInfo allocInfo =
		vkTools::descriptorSetAllocateInfo(
			descriptorPool,
			&descriptorSetLayout,
			1);

	// 3D object descriptor set
	VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.object));

	std::vector<VkWriteDescriptorSet> writeDescriptorSets =
	{
		// Binding 0 : Vertex shader uniform buffer
		vkTools::writeDescriptorSet(
			descriptorSets.object,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			0,
			&uniformData.objectVS.descriptor),
		// Binding 1 : Fragment shader cubemap sampler
		vkTools::writeDescriptorSet(
			descriptorSets.object,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			&cubeMapDescriptor)
	};
	vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

	// Sky box descriptor set
	VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.skybox));

	writeDescriptorSets =
	{
		// Binding 0 : Vertex shader uniform buffer
		vkTools::writeDescriptorSet(
			descriptorSets.skybox,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			0,
			&uniformData.skyboxVS.descriptor),
		// Binding 1 : Fragment shader cubemap sampler
		vkTools::writeDescriptorSet(
			descriptorSets.skybox,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			&cubeMapDescriptor)
	};
	vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
}

void VkTexturecubemap::preparePipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vkTools::pipelineInputAssemblyStateCreateInfo(
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			0,
			VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vkTools::pipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
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
			VK_FALSE,
			VK_FALSE,
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
			dynamicStateEnables.size(),
			0);

	// Skybox pipeline (background cube)
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	shaderStages[0] = loadShader(getAssetPath() + "shaders/cubemap/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/cubemap/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		vkTools::pipelineCreateInfo(
			pipelineLayout,
			mRenderPass,
			0);

	pipelineCreateInfo.pVertexInputState = &vertices.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = shaderStages.size();
	pipelineCreateInfo.pStages = shaderStages.data();

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skybox));

	// Cube map reflect pipeline
	shaderStages[0] = loadShader(getAssetPath() + "shaders/cubemap/reflect.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/cubemap/reflect.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	// Enable depth test and write
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthTestEnable = VK_TRUE;
	// Flip cull mode
	rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.reflect));
}

// Prepare and initialize uniform buffer containing shader uniforms
void VkTexturecubemap::prepareUniformBuffers()
{
	// 3D objact 
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(uboVS),
		nullptr,
		&uniformData.objectVS.buffer,
		&uniformData.objectVS.memory,
		&uniformData.objectVS.descriptor);

	// Skybox
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(uboVS),
		nullptr,
		&uniformData.skyboxVS.buffer,
		&uniformData.skyboxVS.memory,
		&uniformData.skyboxVS.descriptor);

	updateUniformBuffers();
}

void VkTexturecubemap::updateUniformBuffers()
{
	// 3D object
	//glm::mat4 viewMatrix = glm::mat4();
	//uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.001f, 256.0f);
	//viewMatrix = glm::translate(viewMatrix, glm::vec3(0.0f, 0.0f, mZoom));

	//uboVS.model = glm::mat4();
	//uboVS.model = viewMatrix * glm::translate(uboVS.model, cameraPos);
	//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	Matrix viewMatrix, matTmp;
	Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), (float)width / (float)height, 0.001f, 256.0f, &uboVS.projection);
	viewMatrix.translate(0, 0, mZoom);
	matTmp.translate(cameraPos);
	uboVS.model = viewMatrix * matTmp;
	uboVS.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
	uboVS.model.rotateY(MATH_DEG_TO_RAD(mRotation.y));
	uboVS.model.rotateZ(MATH_DEG_TO_RAD(mRotation.z));

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.objectVS.memory, 0, sizeof(uboVS), 0, (void **)&pData));
	memcpy(pData, &uboVS, sizeof(uboVS));
	vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformData.objectVS.memory);

	// Skybox
	//viewMatrix = glm::mat4();
	//uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.001f, 256.0f);
	//uboVS.model = glm::mat4();
	//uboVS.model = viewMatrix * glm::translate(uboVS.model, glm::vec3(0, 0, 0));
	//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), (float)width / (float)height, 0.001f, 256.0f, &uboVS.projection);
	Matrix::createTranslation(Vector3(0, 0, 0), &uboVS.model);
	uboVS.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
	uboVS.model.rotateY(MATH_DEG_TO_RAD(mRotation.y));
	uboVS.model.rotateZ(MATH_DEG_TO_RAD(mRotation.z));

	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.skyboxVS.memory, 0, sizeof(uboVS), 0, (void **)&pData));
	memcpy(pData, &uboVS, sizeof(uboVS));
	vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformData.skyboxVS.memory);
}

void VkTexturecubemap::draw()
{
	prepareFrame();

	mSubmitInfo.commandBufferCount = 1;
	mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[gSwapChain.mCurrentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

	submitFrame();
}

void VkTexturecubemap::render()
{
	if (!prepared)
		return;
	draw();
}

void VkTexturecubemap::viewChanged()
{
	updateUniformBuffers();
}

void VkTexturecubemap::toggleSkyBox()
{
	displaySkybox = !displaySkybox;
	reBuildCommandBuffers();
}

void VkTexturecubemap::toggleObject()
{
	meshes.objectIndex++;
	if (meshes.objectIndex >= static_cast<uint32_t>(meshes.objects.size()))
	{
		meshes.objectIndex = 0;
	}
	reBuildCommandBuffers();
}

void VkTexturecubemap::changeLodBias(float delta)
{
	uboVS.lodBias += delta;
	if (uboVS.lodBias < 0.0f)
	{
		uboVS.lodBias = 0.0f;
	}
	if (uboVS.lodBias > cubeMap.mipLevels)
	{
		uboVS.lodBias = cubeMap.mipLevels;
	}
	updateUniformBuffers();
	updateTextOverlay();
}

void VkTexturecubemap::keyPressed(uint32_t keyCode)
{
	switch (keyCode)
	{
	case Keyboard::KEY_S:
	case GAMEPAD_BUTTON_A:
		toggleSkyBox();
		break;
	case Keyboard::KEY_SPACE:
	case GAMEPAD_BUTTON_X:
		toggleObject();
		break;
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

void VkTexturecubemap::getOverlayText(VulkanTextOverlay *textOverlay)
{
	std::stringstream ss;
	ss << std::setprecision(2) << std::fixed << uboVS.lodBias;
	textOverlay->addText("Press \"s\" to toggle skybox", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
	textOverlay->addText("Press \"space\" to toggle object", 5.0f, 100.0f, VulkanTextOverlay::alignLeft);
	textOverlay->addText("LOD bias: " + ss.str() + " (numpad +/- to change)", 5.0f, 115.0f, VulkanTextOverlay::alignLeft);
}


VkResult VkTexturecubemap::createInstance(bool enableValidation)
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

std::string VkTexturecubemap::getWindowTitle()
{
	std::string device(mVulkanDevice->mProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device;
	if (!mEnableTextOverlay)
	{
		windowTitle += " - " + std::to_string(frameCounter) + " fps";
	}
	return windowTitle;
}

const std::string VkTexturecubemap::getAssetPath()
{
#if defined(__ANDROID__)
	return "";
#else
	return "./../data/";
#endif
}

bool VkTexturecubemap::checkCommandBuffers()
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

void VkTexturecubemap::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	mDrawCmdBuffers.resize(gSwapChain.mImageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::commandBufferAllocateInfo(
			mCmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			static_cast<uint32_t>(mDrawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, mDrawCmdBuffers.data()));
}

void VkTexturecubemap::destroyCommandBuffers()
{
	vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, static_cast<uint32_t>(mDrawCmdBuffers.size()), mDrawCmdBuffers.data());
}

void VkTexturecubemap::createSetupCommandBuffer()
{
	if (setupCmdBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &setupCmdBuffer);
		setupCmdBuffer = VK_NULL_HANDLE; // todo : check if still necessary
	}

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::commandBufferAllocateInfo(
			mCmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, &setupCmdBuffer));

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VK_CHECK_RESULT(vkBeginCommandBuffer(setupCmdBuffer, &cmdBufInfo));
}

void VkTexturecubemap::flushSetupCommandBuffer()
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

	vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &setupCmdBuffer);
	setupCmdBuffer = VK_NULL_HANDLE;
}

VkCommandBuffer VkTexturecubemap::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::commandBufferAllocateInfo(
			mCmdPool,
			level,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

void VkTexturecubemap::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
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
		vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &commandBuffer);
	}
}

void VkTexturecubemap::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(mVulkanDevice->mLogicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VkTexturecubemap::prepare()
{
	if (mVulkanDevice->mEnableDebugMarkers)
	{
		vkDebug::DebugMarker::setup(mVulkanDevice->mLogicalDevice);
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
	textureLoader = new vkTools::VulkanTextureLoader(mVulkanDevice, mQueue, mCmdPool);
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
			mVulkanDevice,
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
	//////////////
	loadMeshes();
	setupVertexDescriptions();
	prepareUniformBuffers();
	loadCubemap(
		getAssetPath() + "textures/cubemap_yokohama.ktx",
		VK_FORMAT_BC3_UNORM_BLOCK,
		false);
	setupDescriptorSetLayout();
	preparePipelines();
	setupDescriptorPool();
	setupDescriptorSets();
	buildCommandBuffers();
	prepared = true;
}

VkPipelineShaderStageCreateInfo VkTexturecubemap::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
#if defined(__ANDROID__)
	shaderStage.module = vkTools::loadShader(androidApp->activity->assetManager, fileName.c_str(), mVulkanDevice->mLogicalDevice, stage);
#else
	shaderStage.module = vkTools::loadShader(fileName.c_str(), mVulkanDevice->mLogicalDevice, stage);
#endif
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != NULL);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

VkPipelineShaderStageCreateInfo VkTexturecubemap::loadShaderGLSL(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
#if defined(__ANDROID__)
	shaderStage.module = vkTools::loadShader(androidApp->activity->assetManager, fileName.c_str(), mVulkanDevice->mLogicalDevice, stage);
#else
	shaderStage.module = vkTools::loadShaderGLSL(fileName.c_str(), mVulkanDevice->mLogicalDevice, stage);
#endif
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != NULL);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

VkBool32 VkTexturecubemap::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory)
{
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc = vkTools::memoryAllocateInfo();
	VkBufferCreateInfo bufferCreateInfo = vkTools::bufferCreateInfo(usageFlags, size);

	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &bufferCreateInfo, nullptr, buffer));

	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, *buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, memory));
	if (data != nullptr)
	{
		void *mapped;
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, *memory, 0, size, 0, &mapped));
		memcpy(mapped, data, size);
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, *memory);
	}
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, *buffer, *memory, 0));

	return true;
}

VkBool32 VkTexturecubemap::createBuffer(VkBufferUsageFlags usage, VkDeviceSize size, void * data, VkBuffer *buffer, VkDeviceMemory *memory)
{
	return createBuffer(usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data, buffer, memory);
}

VkBool32 VkTexturecubemap::createBuffer(VkBufferUsageFlags usage, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor)
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

VkBool32 VkTexturecubemap::createBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor)
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

void VkTexturecubemap::loadMesh(std::string filename, vkMeshLoader::MeshBuffer * meshBuffer, std::vector<vkMeshLoader::VertexLayout> vertexLayout, float scale)
{
	vkMeshLoader::MeshCreateInfo meshCreateInfo;
	meshCreateInfo.scale = glm::vec3(scale);
	meshCreateInfo.center = glm::vec3(0.0f);
	meshCreateInfo.uvscale = glm::vec2(1.0f);
	loadMesh(filename, meshBuffer, vertexLayout, &meshCreateInfo);
}

void VkTexturecubemap::loadMesh(std::string filename, vkMeshLoader::MeshBuffer * meshBuffer, std::vector<vkMeshLoader::VertexLayout> vertexLayout, vkMeshLoader::MeshCreateInfo *meshCreateInfo)
{
	VulkanMeshLoader *mesh = new VulkanMeshLoader(mVulkanDevice);

#if defined(__ANDROID__)
	mesh->assetManager = androidApp->activity->assetManager;
#endif

	mesh->LoadMesh(filename);
	assert(mesh->m_Entries.size() > 0);

	VkCommandBuffer copyCmd = VkTexturecubemap::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	mesh->createBuffers(
		meshBuffer,
		vertexLayout,
		meshCreateInfo,
		true,
		copyCmd,
		mQueue);

	vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &copyCmd);

	meshBuffer->dim = mesh->dim.size;

	delete(mesh);
}

void VkTexturecubemap::renderLoop()
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
	vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);
}

void VkTexturecubemap::updateTextOverlay()
{
	if (!mEnableTextOverlay)
		return;

	mTextOverlay->beginTextUpdate();

	mTextOverlay->addText(title, 5.0f, 5.0f, VulkanTextOverlay::alignLeft);

	std::stringstream ss;
	ss << std::fixed << std::setprecision(3) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
	mTextOverlay->addText(ss.str(), 5.0f, 25.0f, VulkanTextOverlay::alignLeft);

	mTextOverlay->addText(mVulkanDevice->mProperties.deviceName, 5.0f, 45.0f, VulkanTextOverlay::alignLeft);

	getOverlayText(mTextOverlay);

	mTextOverlay->endTextUpdate();
}


void VkTexturecubemap::prepareFrame()
{
	// Acquire the next image from the swap chaing
	VK_CHECK_RESULT(gSwapChain.acquireNextImage(semaphores.presentComplete));
}

void VkTexturecubemap::submitFrame()
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

void VkTexturecubemap::initVulkan(bool enableValidation)
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
	mVulkanDevice = new VkCoreDevice(physicalDevices[0]);
	VK_CHECK_RESULT(mVulkanDevice->createLogicalDevice(enabledFeatures));

	// Get a graphics queue from the device
	vkGetDeviceQueue(mVulkanDevice->mLogicalDevice, mVulkanDevice->queueFamilyIndices.graphics, 0, &mQueue);

	// Find a suitable depth format
	VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(mVulkanDevice->mPhysicalDevice, &mDepthFormat);
	assert(validDepthFormat);

	gSwapChain.connect(mInstance, mVulkanDevice->mPhysicalDevice, mVulkanDevice->mLogicalDevice);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkTools::semaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
	VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
	// Will be inserted after the render complete semaphore if the text overlay is enabled
	VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &semaphores.textOverlayComplete));

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
void VkTexturecubemap::setupConsole(std::string title)
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	FILE *stream;
	freopen_s(&stream, "CONOUT$", "w+", stdout);
	SetConsoleTitle(TEXT(title.c_str()));
}

HWND VkTexturecubemap::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
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

void VkTexturecubemap::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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

void VkTexturecubemap::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = gSwapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(mVulkanDevice->mLogicalDevice, &cmdPoolInfo, nullptr, &mCmdPool));
}

void VkTexturecubemap::setupDepthStencil()
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

	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalDevice, &imageCreateInfo, nullptr, &depthStencil.image));
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalDevice, depthStencil.image, &memReqs);
	memAllocateInfo.allocationSize = memReqs.size;
	memAllocateInfo.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAllocateInfo, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalDevice, depthStencil.image, depthStencil.mem, 0));

	depthStencilView.image = depthStencil.image;
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalDevice, &depthStencilView, nullptr, &depthStencil.view));
}

void VkTexturecubemap::setupFrameBuffer()
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
		VK_CHECK_RESULT(vkCreateFramebuffer(mVulkanDevice->mLogicalDevice, &frameBufferCreateInfo, nullptr, &mFrameBuffers[i]));
	}
}

void VkTexturecubemap::setupRenderPass()
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

	VK_CHECK_RESULT(vkCreateRenderPass(mVulkanDevice->mLogicalDevice, &renderPassInfo, nullptr, &mRenderPass));
}

void VkTexturecubemap::windowResize()
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

	vkDestroyImageView(mVulkanDevice->mLogicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalDevice, depthStencil.mem, nullptr);
	setupDepthStencil();

	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(mVulkanDevice->mLogicalDevice, mFrameBuffers[i], nullptr);
	}
	setupFrameBuffer();

	flushSetupCommandBuffer();

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkQueueWaitIdle(mQueue);
	vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);

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

void VkTexturecubemap::windowResized()
{
	// Can be overriden in derived class
}

void VkTexturecubemap::initSwapchain()
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

void VkTexturecubemap::setupSwapChain()
{
	gSwapChain.create(&width, &height, enableVSync);
}
