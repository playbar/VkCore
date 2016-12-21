#include "VulkanBase.h"


VkResult VulkanBase::createInstance(bool enableValidation)
{
	this->mEnableValidation = enableValidation;

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = name.c_str();
	appInfo.pEngineName = name.c_str();
	appInfo.apiVersion = VK_API_VERSION_1_0;

	std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
	//std::vector<const char*> enabledExtensions = { };

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

std::string VulkanBase::getWindowTitle()
{
	std::string device(gVulkanDevice->mProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device;
	windowTitle += " - " + std::to_string(frameCounter) + " fps";
	return windowTitle;
}

uint32_t VulkanBase::getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties)
{
	// Iterate over all memory types available for the device used in this example
	for (uint32_t i = 0; i < gVulkanDevice->mMemoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((gVulkanDevice->mMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		typeBits >>= 1;
	}

	throw "Could not find a suitable memory type!";
}

// Get a new command buffer from the command pool
// If begin is true, the command buffer is also started so we can start adding commands
VkCommandBuffer VulkanBase::getCommandBuffer(bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = mCmdPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;

	VK_CHECK_RESULT(vkAllocateCommandBuffers(gVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

// End the command buffer and submit it to the queue
// Uses a fence to ensure command buffer has finished executing before deleting it
void VulkanBase::flushCommandBuffer(VkCommandBuffer commandBuffer)
{
	assert(commandBuffer != VK_NULL_HANDLE);

	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;
	VkFence fence;
	VK_CHECK_RESULT(vkCreateFence(gVulkanDevice->mLogicalDevice, &fenceCreateInfo, nullptr, &fence));

	// Submit to the queue
	VK_CHECK_RESULT(vkQueueSubmit(gVulkanDevice->mQueue, 1, &submitInfo, fence));
	// Wait for the fence to signal that command buffer has finished executing
	VK_CHECK_RESULT(vkWaitForFences(gVulkanDevice->mLogicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

	vkDestroyFence(gVulkanDevice->mLogicalDevice, fence, nullptr);
	vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, 1, &commandBuffer);
}

// Build separate command buffers for every framebuffer image
// Unlike in OpenGL all rendering commands are recorded once into command buffers that are then resubmitted to the queue
// This allows to generate work upfront and from multiple threads, one of the biggest advantages of Vulkan
void VulkanBase::buildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = nullptr;

	// Set clear values for all framebuffer attachments with loadOp set to clear
	// We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
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

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(mDrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)width;
		viewport.height = (float)height;
		viewport.minDepth = (float) 0.0f;
		viewport.maxDepth = (float) 1.0f;
		vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

		// Bind descriptor sets describing shader binding points
		vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);

		// Bind the rendering pipeline
		// The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
		vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

		// Bind triangle vertex buffer (contains position and colors)
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &mVertices.buffer, offsets);

		// Bind triangle index buffer
		vkCmdBindIndexBuffer(mDrawCmdBuffers[i], mIndices.buffer, 0, VK_INDEX_TYPE_UINT32);

		// Draw indexed triangle
		vkCmdDrawIndexed(mDrawCmdBuffers[i], mIndices.count, 1, 0, 0, 1);

		vkCmdEndRenderPass(mDrawCmdBuffers[i]);

		// Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to 
		// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

		VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
	}
}

void VulkanBase::draw()
{
	// Get next image in the swap chain (back/front buffer)
	VK_CHECK_RESULT(gSwapChain.acquireNextImage(gVulkanDevice->presentCompleteSemaphore ));

	// Use a fence to wait until the command buffer has finished execution before using it again
	int curIndex = gSwapChain.mCurrentBuffer;
	VK_CHECK_RESULT(vkWaitForFences(gVulkanDevice->mLogicalDevice, 1, &gVulkanDevice->mWaitFences[curIndex], VK_TRUE, UINT64_MAX));
	VK_CHECK_RESULT(vkResetFences(gVulkanDevice->mLogicalDevice, 1, &gVulkanDevice->mWaitFences[curIndex]));

	// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
	VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// The submit info structure specifices a command buffer queue submission batch
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
	submitInfo.pWaitSemaphores = &gVulkanDevice->presentCompleteSemaphore;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
	submitInfo.waitSemaphoreCount = 1;												// One wait semaphore																				
	submitInfo.pSignalSemaphores = &gVulkanDevice->renderCompleteSemaphore;						// Semaphore(s) to be signaled when command buffers have completed
	submitInfo.signalSemaphoreCount = 1;											// One signal semaphore
	submitInfo.pCommandBuffers = &mDrawCmdBuffers[curIndex];					// Command buffers(s) to execute in this batch (submission)
	submitInfo.commandBufferCount = 1;												// One command buffer

	VK_CHECK_RESULT(vkQueueSubmit(gVulkanDevice->mQueue, 1, &submitInfo, gVulkanDevice->mWaitFences[curIndex]));

	VK_CHECK_RESULT(gSwapChain.queuePresent(gVulkanDevice->mQueue, gVulkanDevice->renderCompleteSemaphore));

	VK_CHECK_RESULT(vkQueueWaitIdle(gVulkanDevice->mQueue));
}

void VulkanBase::updateUniformBuffers()
{
	// Update matrices
	float aspect = (float)width / (float)height;
	vkcore::Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), 1.0f, 0.1f, 256.0f, &mUboVS.projectionMatrix);
	Matrix::createTranslation(0.0f, 0.0f, mZoom, &mUboVS.viewMatrix);
	//mUboVS.viewMatrix.translate(0.0f, 0.0f, mZoom, &mUboVS.viewMatrix);
	char szTmp[256] = {};
	sprintf(szTmp, "zoom=%f", mZoom);
	OutputDebugString(szTmp);

	Matrix::createRotationX(MATH_DEG_TO_RAD(mRotation.x), &mUboVS.modelMatrix);
	mUboVS.modelMatrix.rotateY(MATH_DEG_TO_RAD(mRotation.y));
	mUboVS.modelMatrix.rotateZ(MATH_DEG_TO_RAD(mRotation.z));

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory, 0, sizeof(mUboVS), 0, (void **)&pData));
	memcpy(pData, &mUboVS, sizeof(mUboVS));
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory);
}

// Prepare vertex and index buffers for an indexed triangle
// Also uploads them to device local memory using staging and initializes vertex input and attribute binding to match the vertex shader
void VulkanBase::prepareVertices(bool useStagingBuffers)
{
	// A note on memory management in Vulkan in general:
	//	This is a very complex topic and while it's fine for an example application to to small individual memory allocations that is not
	//	what should be done a real-world application, where you should allocate large chunkgs of memory at once isntead.

	// Setup vertices
	std::vector<Vertex> vertexBuffer =
	{
		{ { 0.9f,  1.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },
		{ { -1.0f,  1.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },
		{ { -1.0f, -1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
		//{ { 0.9f, -1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f } }
	};
	//std::vector<Vertex> vertexBuffer =
	//{
	//	{ { -200.0f, -200.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },
	//	{ { -100.0f, -200.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },
	//	{ { -100.0f, -100.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } },
	//	{ { -200.0f, -100.0f, 0.0f },{ 1.0f, 1.0f, 1.0f } }
	//};

	uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

	// Setup indices
	std::vector<uint32_t> indexBuffer = { 1, 0, 2 };
	mIndices.count = static_cast<uint32_t>(indexBuffer.size());
	uint32_t indexBufferSize = mIndices.count * sizeof(uint32_t);

	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	void *data;

	if (useStagingBuffers)
	{
		struct StagingBuffer {
			VkDeviceMemory memory;
			VkBuffer buffer;
		};

		struct {
			StagingBuffer vertices;
			StagingBuffer indices;
		} stagingBuffers;

		// Vertex buffer
		VkBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = vertexBufferSize;
		// Buffer is used as the copy source
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		// Create a host-visible buffer to copy the vertex data to (staging buffer)
		VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer));
		vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, stagingBuffers.vertices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		// Request a host visible memory type that can be used to copy our data do
		// Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &stagingBuffers.vertices.memory));
		// Map and copy
		VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, stagingBuffers.vertices.memory, 0, memAlloc.allocationSize, 0, &data));
		memcpy(data, vertexBuffer.data(), vertexBufferSize);
		vkUnmapMemory(gVulkanDevice->mLogicalDevice, stagingBuffers.vertices.memory);
		VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0));

		// Create a device local buffer to which the (host local) vertex data will be copied and which will be used for rendering
		vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &vertexBufferInfo, nullptr, &mVertices.buffer));
		vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mVertices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mVertices.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mVertices.buffer, mVertices.memory, 0));

		// Index buffer
		VkBufferCreateInfo indexbufferInfo = {};
		indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexbufferInfo.size = indexBufferSize;
		indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		// Copy index data to a buffer visible to the host (staging buffer)
		VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &indexbufferInfo, nullptr, &stagingBuffers.indices.buffer));
		vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, stagingBuffers.indices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &stagingBuffers.indices.memory));
		VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data));
		memcpy(data, indexBuffer.data(), indexBufferSize);
		vkUnmapMemory(gVulkanDevice->mLogicalDevice, stagingBuffers.indices.memory);
		VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0));

		// Create destination buffer with device only visibility
		indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &indexbufferInfo, nullptr, &mIndices.buffer));
		vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mIndices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mIndices.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mIndices.buffer, mIndices.memory, 0));

		VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
		cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufferBeginInfo.pNext = nullptr;

		VkCommandBuffer copyCmd = getCommandBuffer(true);

		VkBufferCopy copyRegion = {};

		// Vertex buffer
		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(copyCmd, stagingBuffers.vertices.buffer, mVertices.buffer, 1, &copyRegion);
		// Index buffer
		copyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(copyCmd, stagingBuffers.indices.buffer, mIndices.buffer, 1, &copyRegion);

		// Flushing the command buffer will also submit it to the queue and uses a fence to ensure that all commands have been executed before returning
		flushCommandBuffer(copyCmd);

		// Destroy staging buffers
		// Note: Staging buffer must not be deleted before the copies have been submitted and executed
		vkDestroyBuffer(gVulkanDevice->mLogicalDevice, stagingBuffers.vertices.buffer, nullptr);
		vkFreeMemory(gVulkanDevice->mLogicalDevice, stagingBuffers.vertices.memory, nullptr);
		vkDestroyBuffer(gVulkanDevice->mLogicalDevice, stagingBuffers.indices.buffer, nullptr);
		vkFreeMemory(gVulkanDevice->mLogicalDevice, stagingBuffers.indices.memory, nullptr);
	}
	else
	{
		// Don't use staging
		// Create host-visible buffers only and use these for rendering. This is not advised and will usually result in lower rendering performance

		// Vertex buffer
		VkBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = vertexBufferSize;
		vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		// Copy vertex data to a buffer visible to the host
		VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &vertexBufferInfo, nullptr, &mVertices.buffer));
		vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mVertices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mVertices.memory));
		VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mVertices.memory, 0, memAlloc.allocationSize, 0, &data));
		memcpy(data, vertexBuffer.data(), vertexBufferSize);
		vkUnmapMemory(gVulkanDevice->mLogicalDevice, mVertices.memory);
		VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mVertices.buffer, mVertices.memory, 0));

		// Index buffer
		VkBufferCreateInfo indexbufferInfo = {};
		indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexbufferInfo.size = indexBufferSize;
		indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		// Copy index data to a buffer visible to the host
		VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &indexbufferInfo, nullptr, &mIndices.buffer));
		vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mIndices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mIndices.memory));
		VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mIndices.memory, 0, indexBufferSize, 0, &data));
		memcpy(data, indexBuffer.data(), indexBufferSize);
		vkUnmapMemory(gVulkanDevice->mLogicalDevice, mIndices.memory);
		VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mIndices.buffer, mIndices.memory, 0));
	}

	// Vertex input binding
	mVertices.inputBinding.binding = VERTEX_BUFFER_BIND_ID;
	mVertices.inputBinding.stride = sizeof(Vertex);
	mVertices.inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// Inpute attribute binding describe shader attribute locations and memory layouts
	// These match the following shader layout (see triangle.vert):
	//	layout (location = 0) in vec3 inPos;
	//	layout (location = 1) in vec3 inColor;
	mVertices.inputAttributes.resize(2);
	// Attribute location 0: Position
	mVertices.inputAttributes[0].binding = VERTEX_BUFFER_BIND_ID;
	mVertices.inputAttributes[0].location = 0;
	mVertices.inputAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	mVertices.inputAttributes[0].offset = offsetof(Vertex, position);
	// Attribute location 1: Color
	mVertices.inputAttributes[1].binding = VERTEX_BUFFER_BIND_ID;
	mVertices.inputAttributes[1].location = 1;
	mVertices.inputAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	mVertices.inputAttributes[1].offset = offsetof(Vertex, color);

	// Assign to the vertex input state used for pipeline creation
	mVertices.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	mVertices.inputState.pNext = nullptr;
	mVertices.inputState.flags = VK_FLAGS_NONE;
	mVertices.inputState.vertexBindingDescriptionCount = 1;
	mVertices.inputState.pVertexBindingDescriptions = &mVertices.inputBinding;
	mVertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(mVertices.inputAttributes.size());
	mVertices.inputState.pVertexAttributeDescriptions = mVertices.inputAttributes.data();
}

void VulkanBase::setupDescriptorPool()
{
	// We need to tell the API the number of max. requested descriptors per type
	VkDescriptorPoolSize typeCounts[1];
	// This example only uses one descriptor type (uniform buffer) and only requests one descriptor of this type
	typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	typeCounts[0].descriptorCount = 1;
	// For additional types you need to add new entries in the type count list
	// E.g. for two combined image samplers :
	// typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// typeCounts[1].descriptorCount = 2;

	// Create the global descriptor pool
	// All descriptors used in this example are allocated from this pool
	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = nullptr;
	descriptorPoolInfo.poolSizeCount = 1;
	descriptorPoolInfo.pPoolSizes = typeCounts;
	// Set the max. number of descriptor sets that can be requested from this pool (requesting beyond this limit will result in an error)
	descriptorPoolInfo.maxSets = 1;

	VK_CHECK_RESULT(vkCreateDescriptorPool(gVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void VulkanBase::setupDescriptorSetLayout()
{
	// Setup layout of descriptors used in this example
	// Basically connects the different shader stages to descriptors for binding uniform buffers, image samplers, etc.
	// So every shader binding should map to one descriptor set layout binding

	// Binding 0: Uniform buffer (Vertex shader)
	VkDescriptorSetLayoutBinding layoutBinding = {};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
	descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayout.pNext = nullptr;
	descriptorLayout.bindingCount = 1;
	descriptorLayout.pBindings = &layoutBinding;

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(gVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));

	// Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
	// In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
	pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pPipelineLayoutCreateInfo.pNext = nullptr;
	pPipelineLayoutCreateInfo.setLayoutCount = 1;
	pPipelineLayoutCreateInfo.pSetLayouts = &mDescriptorSetLayout;

	VK_CHECK_RESULT(vkCreatePipelineLayout(gVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
}

void VulkanBase::setupDescriptorSet()
{
	// Allocate a new descriptor set from the global descriptor pool
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &mDescriptorSetLayout;

	VK_CHECK_RESULT(vkAllocateDescriptorSets(gVulkanDevice->mLogicalDevice, &allocInfo, &mDescriptorSet));

	// Update the descriptor set determining the shader binding points
	// For every binding point used in a shader there needs to be one
	// descriptor set matching that binding point

	VkWriteDescriptorSet writeDescriptorSet = {};

	// Binding 0 : Uniform buffer
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = mDescriptorSet;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSet.pBufferInfo = &mUniformDataVS.descriptor;
	// Binds this uniform buffer to binding point 0
	writeDescriptorSet.dstBinding = 0;

	vkUpdateDescriptorSets(gVulkanDevice->mLogicalDevice, 1, &writeDescriptorSet, 0, nullptr);
}

// Create the depth (and stencil) buffer attachments used by our framebuffers
// Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
void VulkanBase::setupDepthStencil()
{
	// Create an optimal image used as the depth stencil attachment
	VkImageCreateInfo image = {};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = mDepthFormat;
	// Use example's height and width
	image.extent = { width, height, 1 };
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK_RESULT(vkCreateImage(gVulkanDevice->mLogicalDevice, &image, nullptr, &depthStencil.image));

	// Allocate memory for the image (device local) and bind it to our image
	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(gVulkanDevice->mLogicalDevice, depthStencil.image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(gVulkanDevice->mLogicalDevice, depthStencil.image, depthStencil.mem, 0));

	// Create a view for the depth stencil image
	// Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
	// This allows for multiple views of one image with differing ranges (e.g. for different layers)
	VkImageViewCreateInfo depthStencilView = {};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = mDepthFormat;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;
	depthStencilView.image = depthStencil.image;
	VK_CHECK_RESULT(vkCreateImageView(gVulkanDevice->mLogicalDevice, &depthStencilView, nullptr, &depthStencil.view));

}

// Create a frame buffer for each swap chain image
// Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
void VulkanBase::setupFrameBuffer()
{
	std::array<VkImageView, 2> attachments;
	attachments[1] = depthStencil.view;														

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	// All frame buffers use the same renderpass setup
	frameBufferCreateInfo.renderPass = mRenderPass;
	frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	frameBufferCreateInfo.pAttachments = attachments.data();
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	frameBufferCreateInfo.layers = 1;

	// Create a frame buffer for every image in the swapchain
	mFrameBuffers.resize(gSwapChain.mImageCount);
	for (size_t i = 0; i < mFrameBuffers.size(); i++)
	{
		attachments[0] = gSwapChain.buffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(gVulkanDevice->mLogicalDevice, &frameBufferCreateInfo, nullptr, &mFrameBuffers[i]));
	}
}

// Render pass setup
// Render passes are a new concept in Vulkan. They describe the attachments used during rendering and may contain multiple subpasses with attachment dependencies 
// This allows the driver to know up-front what the rendering will look like and is a good opportunity to optimize especially on tile-based renderers (with multiple subpasses)
// Using sub pass dependencies also adds implicit layout transitions for the attachment used, so we don't need to add explicit image memory barriers to transform them
// Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
void VulkanBase::setupRenderPass()
{
	// This example will use a single render pass with one subpass

	// Descriptors for the attachments used by this renderpass
	std::array<VkAttachmentDescription, 2> attachments = {};

	// Color attachment
	attachments[0].format = mColorformat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;									// We don't use multi sampling in this example
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear this attachment at the start of the render pass
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;							// Keep it's contents after the render pass is finished (for displaying it)
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// We don't use stencil, so don't care for load
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// Same for store
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;					// Layout to which the attachment is transitioned when the render pass is finished
																					// As we want to present the color buffer to the swapchain, we transition to PRESENT_KHR	
																					// Depth attachment
	attachments[1].format = mDepthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear depth at start of first subpass
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;						// We don't need depth after render pass has finished (DONT_CARE may result in better performance)
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// No stencil
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// No Stencil
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;	// Transition to depth/stencil attachment

																					// Setup attachment references
	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;													// Attachment 0 is color
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;				// Attachment layout used as color during the subpass

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;													// Attachment 1 is color
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;		// Attachment used as depth/stemcil used during the subpass

																					// Setup a single subpass reference
	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;									// Subpass uses one color attachment
	subpassDescription.pColorAttachments = &colorReference;							// Reference to the color attachment in slot 0
	subpassDescription.pDepthStencilAttachment = &depthReference;					// Reference to the depth attachment in slot 1
	subpassDescription.inputAttachmentCount = 0;									// Input attachments can be used to sample from contents of a previous subpass
	subpassDescription.pInputAttachments = nullptr;									// (Input attachments not used by this example)
	subpassDescription.preserveAttachmentCount = 0;									// Preserved attachments can be used to loop (and preserve) attachments through subpasses
	subpassDescription.pPreserveAttachments = nullptr;								// (Preserve attachments not used by this example)
	subpassDescription.pResolveAttachments = nullptr;								// Resolve attachments are resolved at the end of a sub pass and can be used for e.g. multi sampling

	std::array<VkSubpassDependency, 2> dependencies;

	// First dependency at the start of the renderpass
	// Does the transition from final to initial layout 
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;								// Producer of the dependency 
	dependencies[0].dstSubpass = 0;													// Consumer is our single subpass that will wait for the execution depdendency
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// Second dependency at the end the renderpass
	// Does the transition from the initial to the final layout
	dependencies[1].srcSubpass = 0;													// Producer of the dependency is our single subpass
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;								// Consumer are all commands outside of the renderpass
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// Create the actual renderpass
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());		// Number of attachments used by this render pass
	renderPassInfo.pAttachments = attachments.data();								// Descriptions of the attachments used by the render pass
	renderPassInfo.subpassCount = 1;												// We only use one subpass in this example
	renderPassInfo.pSubpasses = &subpassDescription;								// Description of that subpass
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());	// Number of subpass dependencies
	renderPassInfo.pDependencies = dependencies.data();								// Subpass dependencies used by the render pass

	VK_CHECK_RESULT(vkCreateRenderPass(gVulkanDevice->mLogicalDevice, &renderPassInfo, nullptr, &mRenderPass));
}

void VulkanBase::preparePipelines()
{
	// Create the graphics pipeline used in this example
	// Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
	// A pipeline is then stored and hashed on the GPU making pipeline changes very fast
	// Note: There are still a few dynamic states that are not directly part of the pipeline (but the info that they are used is)

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	// The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
	pipelineCreateInfo.layout = mPipelineLayout;
	// Renderpass this pipeline is attached to
	pipelineCreateInfo.renderPass = mRenderPass;

	// Construct the differnent states making up the pipeline

	// Input assembly state describes how primitives are assembled
	// This pipeline will assemble vertex data as a triangle lists (though we only use one triangle)
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	// Rasterization state
	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.lineWidth = 1.0f;

	// Color blend state describes how blend factors are calculated (if used)
	// We need one blend attachment state per color attachment (even if blending is not used
	VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
	blendAttachmentState[0].colorWriteMask = 0xf;
	blendAttachmentState[0].blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = blendAttachmentState;

	// Viewport state sets the number of viewports and scissor used in this pipeline
	// Note: This is actually overriden by the dynamic states (see below)
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// Enable dynamic states
	// Most states are baked into the pipeline, but there are still a few dynamic states that can be changed within a command buffer
	// To be able to change these we need do specify which dynamic states will be changed using this pipeline. Their actual states are set later on in the command buffer.
	// For this example we will set the viewport and scissor using dynamic states
	std::vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables.data();
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

	// Depth and stencil state containing depth and stencil compare and test operations
	// We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilState.stencilTestEnable = VK_FALSE;
	depthStencilState.front = depthStencilState.back;

	// Multi sampling state
	// This example does not make use fo multi sampling (for anti-aliasing), the state must still be set and passed to the pipeline
	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.pSampleMask = nullptr;

	// Load shaders
	// Vulkan loads it's shaders from an immediate binary representation called SPIR-V
	// Shaders are compiled offline from e.g. GLSL using the reference glslang compiler
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
	shaderStages[0] = loadShader(getAssetPath() + "shaders/triangle.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/triangle.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	// Assign the pipeline states to the pipeline creation info structure
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &mVertices.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.renderPass = mRenderPass;
	pipelineCreateInfo.pDynamicState = &dynamicState;

	// Create rendering pipeline using the specified states
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(gVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeline));
}

void VulkanBase::prepareUniformBuffers()
{
	// Prepare and initialize a uniform buffer block containing shader uniforms
	// Single uniforms like in OpenGL are no longer present in Vulkan. All Shader uniforms are passed via uniform buffer blocks
	VkMemoryRequirements memReqs;

	// Vertex shader uniform buffer block
	VkBufferCreateInfo bufferInfo = {};
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(mUboVS);
	// This buffer will be used as a uniform buffer
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// Create a new buffer
	VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &bufferInfo, nullptr, &mUniformDataVS.buffer));
	// Get memory requirements including size, alignment and memory type 
	vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, &memReqs);
	allocInfo.allocationSize = memReqs.size;
	// Get the memory type index that supports host visibile memory access
	// Most implementations offer multiple memory types and selecting the correct one to allocate memory from is crucial
	// We also want the buffer to be host coherent so we don't have to flush (or sync after every update.
	// Note: This may affect performance so you might not want to do this in a real world application that updates buffers on a regular base
	allocInfo.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	// Allocate memory for the uniform buffer
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &allocInfo, nullptr, &(mUniformDataVS.memory)));
	// Bind memory to buffer
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, mUniformDataVS.memory, 0));

	// Store information in the uniform's descriptor that is used by the descriptor set
	mUniformDataVS.descriptor.buffer = mUniformDataVS.buffer;
	mUniformDataVS.descriptor.offset = 0;
	mUniformDataVS.descriptor.range = sizeof(mUboVS);

	updateUniformBuffers();
}

void VulkanBase::render()
{
	if (!prepared)
		return;
	draw();
}

void VulkanBase::viewChanged()
{
	// This function is called by the base example class each time the view is changed by user input
	updateUniformBuffers();
}

const std::string VulkanBase::getAssetPath()
{
#if defined(__ANDROID__)
	return "";
#else
	return "./../data/";
#endif
}

bool VulkanBase::checkCommandBuffers()
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

void VulkanBase::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	mDrawCmdBuffers.resize(gSwapChain.mImageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::initializers::commandBufferAllocateInfo(
			mCmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			static_cast<uint32_t>(mDrawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(gVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, mDrawCmdBuffers.data()));
}

void VulkanBase::destroyCommandBuffers()
{
	vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, static_cast<uint32_t>(mDrawCmdBuffers.size()), mDrawCmdBuffers.data());
}

VkCommandBuffer VulkanBase::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::initializers::commandBufferAllocateInfo(
			mCmdPool,
			level,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(gVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

void VulkanBase::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
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

void VulkanBase::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(gVulkanDevice->mLogicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanBase::prepare()
{
	if (gVulkanDevice->mEnableDebugMarkers)
	{
		vkDebug::DebugMarker::setup(gVulkanDevice->mLogicalDevice);
	}
	setupSwapChain();
	createCommandPool();	
	createCommandBuffers();
	setupDepthStencil();
	setupRenderPass();
	createPipelineCache();
	setupFrameBuffer();
	
	gVulkanDevice->prepareSynchronizationPrimitives();

	prepareVertices(USE_STAGING);
	prepareUniformBuffers();
	setupDescriptorSetLayout();
	preparePipelines();
	setupDescriptorPool();
	setupDescriptorSet();
	buildCommandBuffers();
	prepared = true;

}

VkPipelineShaderStageCreateInfo VulkanBase::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
#if defined(__ANDROID__)
	shaderStage.module = vkTools::loadShader(androidApp->activity->assetManager, fileName.c_str(), gVulkanDevice->mLogicalDevice, stage);
#else
	//shaderStage.module = vkTools::loadShader(fileName.c_str(), gVulkanDevice->mLogicalDevice, stage);
	shaderStage.module = vkTools::loadShaderGLSL(fileName.c_str(), gVulkanDevice->mLogicalDevice, stage);
	
#endif
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != NULL);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

void VulkanBase::loadMesh(std::string filename, vkMeshLoader::MeshBuffer * meshBuffer, std::vector<vkMeshLoader::VertexLayout> vertexLayout, float scale)
{
	vkMeshLoader::MeshCreateInfo meshCreateInfo;
	meshCreateInfo.scale = glm::vec3(scale);
	meshCreateInfo.center = glm::vec3(0.0f);
	meshCreateInfo.uvscale = glm::vec2(1.0f);
	loadMesh(filename, meshBuffer, vertexLayout, &meshCreateInfo);
}

void VulkanBase::loadMesh(std::string filename, vkMeshLoader::MeshBuffer * meshBuffer, std::vector<vkMeshLoader::VertexLayout> vertexLayout, vkMeshLoader::MeshCreateInfo *meshCreateInfo)
{
	VulkanMeshLoader *mesh = new VulkanMeshLoader(gVulkanDevice);

#if defined(__ANDROID__)
	mesh->assetManager = androidApp->activity->assetManager;
#endif

	mesh->LoadMesh(filename);
	assert(mesh->m_Entries.size() > 0);

	VkCommandBuffer copyCmd = VulkanBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	mesh->createBuffers(
		meshBuffer,
		vertexLayout,
		meshCreateInfo,
		true,
		copyCmd,
		gVulkanDevice->mQueue);

	vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, 1, &copyCmd);

	meshBuffer->dim = mesh->dim.size;

	delete(mesh);
}

void VulkanBase::renderLoop()
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
			std::string windowTitle = getWindowTitle();
			SetWindowText(mHwndWinow, windowTitle.c_str());
			lastFPS = roundf(1.0f / frameTimer);
			fpsTimer = 0.0f;
			frameCounter = 0;
		}
	}

	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(gVulkanDevice->mLogicalDevice);
}

VulkanBase::VulkanBase(bool enableValidation, PFN_GetEnabledFeatures enabledFeaturesFn)
{

	if (enabledFeaturesFn != nullptr)
	{
		this->enabledFeatures = enabledFeaturesFn();
	}

	if (enableValidation)
	{
		setupConsole("VulkanExample");
	}

#if !defined(__ANDROID__)
	// Android Vulkan initialization is handled in APP_CMD_INIT_WINDOW event
	initVulkan(enableValidation);
#endif

	width = 800;
	height = 600;
	mZoom = -10.0f;
	title = "VkCore";

}

VulkanBase::~VulkanBase()
{
	// Clean up used Vulkan resources 
	// Note: Inherited destructor cleans up resources stored in base class
	vkDestroyPipeline(gVulkanDevice->mLogicalDevice, mPipeline, nullptr);

	vkDestroyPipelineLayout(gVulkanDevice->mLogicalDevice, mPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(gVulkanDevice->mLogicalDevice, mDescriptorSetLayout, nullptr);

	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mVertices.buffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mVertices.memory, nullptr);

	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mIndices.buffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mIndices.memory, nullptr);

	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory, nullptr);


	// Clean up Vulkan resources
	gSwapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(gVulkanDevice->mLogicalDevice, descriptorPool, nullptr);
	}
	//if (setupCmdBuffer != VK_NULL_HANDLE)
	//{
	//	vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &setupCmdBuffer);
	//}
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

	vkDestroyCommandPool(gVulkanDevice->mLogicalDevice, mCmdPool, nullptr);

	delete gVulkanDevice;

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

void VulkanBase::initVulkan(bool enableValidation)
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

	// Find a suitable depth format
	VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(gVulkanDevice->mPhysicalDevice, &mDepthFormat);
	assert(validDepthFormat);

	gSwapChain.connect(mInstance, gVulkanDevice->mPhysicalDevice, gVulkanDevice->mLogicalDevice);
	return;
}

// Win32 : Sets up a console window and redirects standard output to it
void VulkanBase::setupConsole(std::string title)
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	FILE *stream;
	freopen_s(&stream, "CONOUT$", "w+", stdout);
	SetConsoleTitle(TEXT(title.c_str()));
}

HWND VulkanBase::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
{
	this->mWindowInstance = hinstance;

	bool fullscreen = false;

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

void VulkanBase::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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


void VulkanBase::keyPressed(uint32_t keyCode)
{
	// Can be overriden in derived class
}

void VulkanBase::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = gSwapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(gVulkanDevice->mLogicalDevice, &cmdPoolInfo, nullptr, &mCmdPool));
}

void VulkanBase::windowResize()
{
	if (!prepared)
	{
		return;
	}
	prepared = false;

	// Recreate swap chain
	width = destWidth;
	height = destHeight;
	//createSetupCommandBuffer();
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

	//flushSetupCommandBuffer();

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkQueueWaitIdle(gVulkanDevice->mQueue);
	vkDeviceWaitIdle(gVulkanDevice->mLogicalDevice);

	mCamera.updateAspectRatio((float)width / (float)height);

	// Notify derived class
	windowResized();
	viewChanged();

	prepared = true;
}

void VulkanBase::windowResized()
{
	// Can be overriden in derived class
}

void VulkanBase::initSwapchain()
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

void VulkanBase::setupSwapChain()
{
	gSwapChain.create(&width, &height, enableVSync);
}
