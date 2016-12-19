#pragma once

#include <exception>
#include <assert.h>
#include "vulkan/vulkan.h"
#include "vulkantools.h"
#include "vulkanbuffer.hpp"

class VkCoreDevice
{
public:
	VkPhysicalDevice mPhysicalDevice;
	VkDevice mLogicalDevice;
	VkQueue mQueue;
	VkPhysicalDeviceProperties mProperties;
	VkPhysicalDeviceFeatures mFeatures;
	VkPhysicalDeviceMemoryProperties mMemoryProperties;
	std::vector<VkQueueFamilyProperties> mVecQueueFamilyProperties;

	/** @brief Default command pool for the graphics queue family index */
	VkCommandPool mCommandPool = VK_NULL_HANDLE;

	VkSemaphore presentCompleteSemaphore;
	VkSemaphore renderCompleteSemaphore;
	std::vector<VkFence> mWaitFences;

	/** @brief Set to true when the debug marker extension is detected */
	bool mEnableDebugMarkers = false;

	/** @brief Contains queue family indices */
	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;

	VkCoreDevice(VkPhysicalDevice phyDevice);

	~VkCoreDevice();

	uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr);

	uint32_t getQueueFamiliyIndex(VkQueueFlagBits queueFlags);

	VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

	VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
		VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);

	// Create a buffer, fill it with data (if != NULL) and bind buffer memory
	VkBool32 createBuffer(
		VkBufferUsageFlags usageFlags,
		VkMemoryPropertyFlags memoryPropertyFlags,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory);

	// This version always uses HOST_VISIBLE memory
	VkBool32 createBuffer(
		VkBufferUsageFlags usage,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory);

	// Overload that assigns buffer info to descriptor
	VkBool32 createBuffer(
		VkBufferUsageFlags usage,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory,
		VkDescriptorBufferInfo *descriptor);

	// Overload to pass memory property flags
	VkBool32 createBuffer(
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags memoryPropertyFlags,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory,
		VkDescriptorBufferInfo *descriptor);

	VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
		vk::Buffer *buffer, VkDeviceSize size, void *data = nullptr);

	void copyBuffer(vk::Buffer *src, vk::Buffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr);

	VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);

	void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

	void prepareSynchronizationPrimitives();

	const std::string getAssetPath();

};


extern VkCoreDevice *gVulkanDevice;

