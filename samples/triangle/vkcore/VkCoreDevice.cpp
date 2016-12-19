#include "VkCoreDevice.hpp"
#include "vulkanswapchain.hpp"

VkCoreDevice *gVulkanDevice = NULL;

VkCoreDevice::VkCoreDevice(VkPhysicalDevice phyDevice)
{
	assert(phyDevice);
	this->mPhysicalDevice = phyDevice;

	// Store Properties features, limits and properties of the physical device for later use
	// Device properties also contain limits and sparse properties
	vkGetPhysicalDeviceProperties(phyDevice, &mProperties);
	// Features should be checked by the examples before using them
	vkGetPhysicalDeviceFeatures(phyDevice, &mFeatures);
	// Memory properties are used regularly for creating all kinds of buffer
	vkGetPhysicalDeviceMemoryProperties(phyDevice, &mMemoryProperties);
	// Queue family properties, used for setting up requested queues upon device creation
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueFamilyCount, nullptr);
	assert(queueFamilyCount > 0);
	mVecQueueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueFamilyCount, mVecQueueFamilyProperties.data());
}


VkCoreDevice::~VkCoreDevice()
{
	vkDestroySemaphore(mLogicalDevice, presentCompleteSemaphore, nullptr);
	vkDestroySemaphore(mLogicalDevice, renderCompleteSemaphore, nullptr);

	for (auto& fence : mWaitFences)
	{
		vkDestroyFence(mLogicalDevice, fence, nullptr);
	}

	if (mCommandPool)
	{
		vkDestroyCommandPool(mLogicalDevice, mCommandPool, nullptr);
	}
	if (mLogicalDevice)
	{
		vkDestroyDevice(mLogicalDevice, nullptr);
	}
}

// Create the Vulkan synchronization primitives used in this example
void VkCoreDevice::prepareSynchronizationPrimitives()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;

	VK_CHECK_RESULT(vkCreateSemaphore(gVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore));
	VK_CHECK_RESULT(vkCreateSemaphore(gVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &renderCompleteSemaphore));

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	mWaitFences.resize(gSwapChain.buffers.size());
	for (auto& fence : mWaitFences)
	{
		VK_CHECK_RESULT(vkCreateFence(gVulkanDevice->mLogicalDevice, &fenceCreateInfo, nullptr, &fence));
	}
}


uint32_t VkCoreDevice::getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound)
{
	for (uint32_t i = 0; i < mMemoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((mMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				if (memTypeFound)
				{
					*memTypeFound = true;
				}
				return i;
			}
		}
		typeBits >>= 1;
	}

#if defined(__ANDROID__)
	//todo : Exceptions are disabled by default on Android (need to add LOCAL_CPP_FEATURES += exceptions to Android.mk), so for now just return zero
	if (memTypeFound)
	{
		*memTypeFound = false;
	}
	return 0;
#else
	if (memTypeFound)
	{
		*memTypeFound = false;
		return 0;
	}
	else
	{
		throw std::runtime_error("Could not find a matching memory type");
	}
#endif
}

uint32_t VkCoreDevice::getQueueFamiliyIndex(VkQueueFlagBits queueFlags)
{
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if (queueFlags & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(mVecQueueFamilyProperties.size()); i++)
		{
			if ((mVecQueueFamilyProperties[i].queueFlags & queueFlags) && ((mVecQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
				break;
			}
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (queueFlags & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(mVecQueueFamilyProperties.size()); i++)
		{
			if ((mVecQueueFamilyProperties[i].queueFlags & queueFlags) && ((mVecQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((mVecQueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
				break;
			}
		}
	}

	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	for (uint32_t i = 0; i < static_cast<uint32_t>(mVecQueueFamilyProperties.size()); i++)
	{
		if (mVecQueueFamilyProperties[i].queueFlags & queueFlags)
		{
			return i;
			break;
		}
	}

#if defined(__ANDROID__)
	//todo : Exceptions are disabled by default on Android (need to add LOCAL_CPP_FEATURES += exceptions to Android.mk), so for now just return zero
	return 0;
#else
	throw std::runtime_error("Could not find a matching queue family index");
#endif
}


VkResult VkCoreDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, bool useSwapChain, 
	VkQueueFlags requestedQueueTypes)
{
	// Desired queues need to be requested upon logical device creation
	// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
	// requests different queue types

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	const float defaultQueuePriority(0.0f);

	// Graphics queue
	if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		queueFamilyIndices.graphics = getQueueFamiliyIndex(VK_QUEUE_GRAPHICS_BIT);
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		queueCreateInfos.push_back(queueInfo);
	}
	else
	{
		queueFamilyIndices.graphics = VK_NULL_HANDLE;
	}

	// Dedicated compute queue
	if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{
		queueFamilyIndices.compute = getQueueFamiliyIndex(VK_QUEUE_COMPUTE_BIT);
		if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		queueFamilyIndices.compute = queueFamilyIndices.graphics;
	}

	// Dedicated transfer queue
	if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
	{
		queueFamilyIndices.transfer = getQueueFamiliyIndex(VK_QUEUE_TRANSFER_BIT);
		if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		queueFamilyIndices.transfer = queueFamilyIndices.graphics;
	}

	// Create the logical device representation
	std::vector<const char*> deviceExtensions;
	if (useSwapChain)
	{
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

	// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
	if (vkTools::checkDeviceExtensionPresent(mPhysicalDevice, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
	{
		deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
		mEnableDebugMarkers = true;
	}

	if (deviceExtensions.size() > 0)
	{
		deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}

	VkResult result = vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mLogicalDevice);

	if (result == VK_SUCCESS)
	{
		// Create a default command pool for graphics command buffers
		mCommandPool = createCommandPool(queueFamilyIndices.graphics);
	}

	// Get a graphics queue from the device
	vkGetDeviceQueue(mLogicalDevice, queueFamilyIndices.graphics, 0, &mQueue);

	return result;
}


VkResult VkCoreDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
	VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data)
{
	// Create the buffer handle
	VkBufferCreateInfo bufferCreateInfo = vkTools::initializers::bufferCreateInfo(usageFlags, size);
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK_RESULT(vkCreateBuffer(mLogicalDevice, &bufferCreateInfo, nullptr, buffer));

	// Create the memory backing up the buffer handle
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc = vkTools::initializers::memoryAllocateInfo();
	vkGetBufferMemoryRequirements(mLogicalDevice, *buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	// Find a memory type index that fits the properties of the buffer
	memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	VK_CHECK_RESULT(vkAllocateMemory(mLogicalDevice, &memAlloc, nullptr, memory));

	// If a pointer to the buffer data has been passed, map the buffer and copy over the data
	if (data != nullptr)
	{
		void *mapped;
		VK_CHECK_RESULT(vkMapMemory(mLogicalDevice, *memory, 0, size, 0, &mapped));
		memcpy(mapped, data, size);
		vkUnmapMemory(mLogicalDevice, *memory);
	}

	// Attach the memory to the buffer object
	VK_CHECK_RESULT(vkBindBufferMemory(mLogicalDevice, *buffer, *memory, 0));

	return VK_SUCCESS;
}


VkResult VkCoreDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
	vk::Buffer *buffer, VkDeviceSize size, void *data)
{
	buffer->device = mLogicalDevice;

	// Create the buffer handle
	VkBufferCreateInfo bufferCreateInfo = vkTools::initializers::bufferCreateInfo(usageFlags, size);
	VK_CHECK_RESULT(vkCreateBuffer(mLogicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

	// Create the memory backing up the buffer handle
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc = vkTools::initializers::memoryAllocateInfo();
	vkGetBufferMemoryRequirements(mLogicalDevice, buffer->buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	// Find a memory type index that fits the properties of the buffer
	memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	VK_CHECK_RESULT(vkAllocateMemory(mLogicalDevice, &memAlloc, nullptr, &buffer->memory));

	buffer->alignment = memReqs.alignment;
	buffer->size = memAlloc.allocationSize;
	buffer->usageFlags = usageFlags;
	buffer->memoryPropertyFlags = memoryPropertyFlags;

	// If a pointer to the buffer data has been passed, map the buffer and copy over the data
	if (data != nullptr)
	{
		VK_CHECK_RESULT(buffer->map());
		memcpy(buffer->mapped, data, size);
		buffer->unmap();
	}

	// Initialize a default descriptor that covers the whole buffer size
	buffer->setupDescriptor();

	// Attach the memory to the buffer object
	return buffer->bind();
}

void VkCoreDevice::copyBuffer(vk::Buffer *src, vk::Buffer *dst, VkQueue queue, VkBufferCopy *copyRegion)
{
	assert(dst->size <= src->size);
	assert(src->buffer && src->buffer);
	VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy bufferCopy{};
	if (copyRegion == nullptr)
	{
		bufferCopy.size = src->size;
	}
	else
	{
		bufferCopy = *copyRegion;
	}

	vkCmdCopyBuffer(copyCmd, src->buffer, dst->buffer, 1, &bufferCopy);

	flushCommandBuffer(copyCmd, queue);
}


VkBool32 VkCoreDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory)
{
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc = vkTools::initializers::memoryAllocateInfo();
	VkBufferCreateInfo bufferCreateInfo = vkTools::initializers::bufferCreateInfo(usageFlags, size);

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

VkBool32 VkCoreDevice::createBuffer(VkBufferUsageFlags usage, VkDeviceSize size, void * data, VkBuffer *buffer, VkDeviceMemory *memory)
{
	return createBuffer(usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data, buffer, memory);
}

VkBool32 VkCoreDevice::createBuffer(VkBufferUsageFlags usage, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor)
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

VkBool32 VkCoreDevice::createBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor)
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

VkCommandPool VkCoreDevice::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
	cmdPoolInfo.flags = createFlags;
	VkCommandPool cmdPool;
	VK_CHECK_RESULT(vkCreateCommandPool(mLogicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
	return cmdPool;
}

VkCommandBuffer VkCoreDevice::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = vkTools::initializers::commandBufferAllocateInfo(mCommandPool, level, 1);

	VkCommandBuffer cmdBuffer;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mLogicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start recording for the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

void VkCoreDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
	if (commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}

	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = vkTools::initializers::submitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceInfo = vkTools::initializers::fenceCreateInfo(VK_FLAGS_NONE);
	VkFence fence;
	VK_CHECK_RESULT(vkCreateFence(mLogicalDevice, &fenceInfo, nullptr, &fence));

	// Submit to the queue
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
	// Wait for the fence to signal that command buffer has finished executing
	VK_CHECK_RESULT(vkWaitForFences(mLogicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

	vkDestroyFence(mLogicalDevice, fence, nullptr);

	if (free)
	{
		vkFreeCommandBuffers(mLogicalDevice, mCommandPool, 1, &commandBuffer);
	}
}


