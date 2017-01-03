#pragma once

#include <vulkan/vulkan.h>
#include <gli/gli.hpp>

#include "VkCoreDevice.hpp"

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

namespace vkTools 
{
	struct VulkanTexture
	{
		VkSampler sampler;
		VkImage image;
		VkImageLayout imageLayout;
		VkDeviceMemory deviceMemory;
		VkImageView view;
		uint32_t width;
		uint32_t height;
		uint32_t mipLevels;
		uint32_t layerCount;
		VkDescriptorImageInfo descriptor;
	};

	/**
	* @brief A simple Vulkan texture uploader for getting images into GPU memory
	*/
	class VulkanTextureLoader
	{
	private:
		VkCoreDevice *vulkanDevice;
		VkQueue queue;
		VkCommandBuffer cmdBuffer;
		VkCommandPool cmdPool;
	public:
#if defined(__ANDROID__)
		AAssetManager* assetManager = nullptr;
#endif

		VulkanTextureLoader(VkCoreDevice *vulkanDevice, VkQueue queue, VkCommandPool cmdPool);

		~VulkanTextureLoader();

		void loadTexture(std::string filename, VkFormat format, VulkanTexture *texture, bool forceLinear = false, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT);

		void loadCubemap(std::string filename, VkFormat format, VulkanTexture *texture, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT);

		void loadTextureArray(std::string filename, VkFormat format, VulkanTexture *texture, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT);

		void createTexture(void* buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t width, uint32_t height, VulkanTexture *texture, VkFilter filter = VK_FILTER_LINEAR, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT);

		void destroyTexture(VulkanTexture texture);

	};
};
