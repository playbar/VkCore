#pragma once

#include <stdlib.h>
#include <string>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#endif

#include <vulkan/vulkan.h>
#include "vulkantools.h"

#ifdef __ANDROID__
#include "vulkanandroid.h"
#endif

// Macro to get a procedure address based on a vulkan instance
#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                        \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetInstanceProcAddr(inst, "vk"#entrypoint)); \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);                                                        \
	}                                                                   \
}

// Macro to get a procedure address based on a vulkan device
#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                           \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetDeviceProcAddr(dev, "vk"#entrypoint));   \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);                                                        \
	}                                                                   \
}

typedef struct _SwapChainBuffers 
{
	VkImage image;
	VkImageView view;
} SwapChainBuffer;

class VulkanSwapChain
{	
private: 
	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;
	// Active frame buffer index
	
	// Function pointers
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR; 
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
	PFN_vkQueuePresentKHR fpQueuePresentKHR;
public:
	uint32_t mCurrentBuffer;
	VkFormat colorFormat;
	VkColorSpaceKHR colorSpace;
	/** @brief Handle to the current swap chain, required for recreation */
	VkSwapchainKHR mSwapChainKHR = VK_NULL_HANDLE;	
	uint32_t mImageCount = 0;
	std::vector<VkImage> images;
	std::vector<SwapChainBuffer> buffers;
	// Index of the deteced graphics and presenting device queue
	/** @brief Queue family index of the deteced graphics and presenting device queue */
	uint32_t queueNodeIndex = UINT32_MAX;

	void initSurface(void* platformHandle, void* platformWindow);

	void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

	void create(uint32_t *width, uint32_t *height, bool vsync = false);

	VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore);

	VkResult queuePresent(VkQueue queue, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

	void cleanup();

};

extern VulkanSwapChain gSwapChain;
