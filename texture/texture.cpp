/*
* Vulkan Example - Texture loading (and display) example (including mip maps)
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "VulkanBase.h"
#include "vulkandevice.hpp"
#include "vulkanbuffer.hpp"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

// Vertex layout for this example
struct Vertex {
	float pos[3];
	float uv[2];
	float normal[3];
};

class VulkanExample : public VulkanBase
{
public:
	// Contains all Vulkan objects that are required to store and use a texture
	// Note that this repository contains a texture loader (vulkantextureloader.h)
	// that encapsulates texture loading functionality in a class that is used
	// in subsequent demos
	struct Texture
	{
		VkSampler sampler;
		VkImage image;
		VkImageLayout imageLayout;
		VkDeviceMemory deviceMemory;
		VkImageView imageView;
		VkDescriptorImageInfo descriptor;
		uint32_t width, height;
		uint32_t mipLevels;
	} mTexture;

	struct Vertices
	{
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} mVertices;

	vk::Buffer mVertexBuffer;
	vk::Buffer mIndexBuffer;
	uint32_t mIndexCount;

	vk::Buffer mUniformBufferVS;

	struct
	{
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 viewPos;
		float lodBias = 0.0f;
	} mUboVS;

	struct {
		VkPipeline solid;
	} mPipelines;

	VkPipelineLayout mPipelineLayout;
	VkDescriptorSet mDescriptorSet;
	VkDescriptorSetLayout mDescriptorSetLayout;

	VulkanExample() : VulkanBase(ENABLE_VALIDATION)
	{
		zoom = -2.5f;
		rotation = { 0.0f, 15.0f, 0.0f };
		title = "Vulkan Example - Texturing";
		enableTextOverlay = true;
	}

	~VulkanExample()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class

		destroyTextureImage(mTexture);

		vkDestroyPipeline(mDevice, mPipelines.solid, nullptr);

		vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);

		mVertexBuffer.destroy();
		mIndexBuffer.destroy();
		mUniformBufferVS.destroy();
	}

	// Create an image memory barrier for changing the layout of
	// an image and put it into an active command buffer
	void setImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange)
	{
		// Create an image barrier object
		VkImageMemoryBarrier imageMemoryBarrier = vkTools::initializers::imageMemoryBarrier();;
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

	void loadTexture(std::string fileName, VkFormat format, bool forceLinearTiling)
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
		vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &formatProperties);

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

		VkMemoryAllocateInfo memAllocInfo = vkTools::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs = {};

		if (useStaging)
		{
			// Create a host-visible staging buffer that contains the raw image data
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;

			VkBufferCreateInfo bufferCreateInfo = vkTools::initializers::bufferCreateInfo();
			bufferCreateInfo.size = tex2D.size();
			// This buffer is used as a transfer source for the buffer copy
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			
			VK_CHECK_RESULT(vkCreateBuffer(mDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

			// Get memory requirements for the staging buffer (alignment, memory type bits)
			vkGetBufferMemoryRequirements(mDevice, stagingBuffer, &memReqs);

			memAllocInfo.allocationSize = memReqs.size;
			// Get memory type index for a host visible buffer
			memAllocInfo.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			VK_CHECK_RESULT(vkAllocateMemory(mDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(mDevice, stagingBuffer, stagingMemory, 0));

			// Copy texture data into staging buffer
			uint8_t *data;
			VK_CHECK_RESULT(vkMapMemory(mDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
			memcpy(data, tex2D.data(), tex2D.size());
			vkUnmapMemory(mDevice, stagingMemory);

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
			VkImageCreateInfo imageCreateInfo = vkTools::initializers::imageCreateInfo();
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

			VK_CHECK_RESULT(vkCreateImage(mDevice, &imageCreateInfo, nullptr, &mTexture.image));

			vkGetImageMemoryRequirements(mDevice, mTexture.image, &memReqs);

			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VK_CHECK_RESULT(vkAllocateMemory(mDevice, &memAllocInfo, nullptr, &mTexture.deviceMemory));
			VK_CHECK_RESULT(vkBindImageMemory(mDevice, mTexture.image, mTexture.deviceMemory, 0));

			VkCommandBuffer copyCmd = VulkanBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

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

			VulkanBase::flushCommandBuffer(copyCmd, mQueue, true);

			// Clean up staging resources
			vkFreeMemory(mDevice, stagingMemory, nullptr);
			vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
		}
		else
		{
			// Prefer using optimal tiling, as linear tiling 
			// may support only a small set of features 
			// depending on implementation (e.g. no mip maps, only one layer, etc.)

			VkImage mappableImage;
			VkDeviceMemory mappableMemory;

			// Load mip map level 0 to linear tiling image
			VkImageCreateInfo imageCreateInfo = vkTools::initializers::imageCreateInfo();
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
			VK_CHECK_RESULT(vkCreateImage(mDevice, &imageCreateInfo, nullptr, &mappableImage));

			// Get memory requirements for this image 
			// like size and alignment
			vkGetImageMemoryRequirements(mDevice, mappableImage, &memReqs);
			// Set memory allocation size to required memory size
			memAllocInfo.allocationSize = memReqs.size;

			// Get memory type that can be mapped to host memory
			memAllocInfo.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			// Allocate host memory
			VK_CHECK_RESULT(vkAllocateMemory(mDevice, &memAllocInfo, nullptr, &mappableMemory));

			// Bind allocated image for use
			VK_CHECK_RESULT(vkBindImageMemory(mDevice, mappableImage, mappableMemory, 0));

			// Get sub resource layout
			// Mip map count, array layer, etc.
			VkImageSubresource subRes = {};
			subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			VkSubresourceLayout subResLayout;
			void *data;

			// Get sub resources layout 
			// Includes row pitch, size offsets, etc.
			vkGetImageSubresourceLayout(mDevice, mappableImage, &subRes, &subResLayout);

			// Map image memory
			VK_CHECK_RESULT(vkMapMemory(mDevice, mappableMemory, 0, memReqs.size, 0, &data));

			// Copy image data into memory
			memcpy(data, tex2D[subRes.mipLevel].data(), tex2D[subRes.mipLevel].size());

			vkUnmapMemory(mDevice, mappableMemory);

			// Linear tiled images don't need to be staged
			// and can be directly used as textures
			mTexture.image = mappableImage;
			mTexture.deviceMemory = mappableMemory;
			mTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			
			VkCommandBuffer copyCmd = VulkanBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

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

			VulkanBase::flushCommandBuffer(copyCmd, mQueue, true);
		}

		// Create sampler
		// In Vulkan textures are accessed by samplers
		// This separates all the sampling information from the 
		// texture data
		// This means you could have multiple sampler objects
		// for the same texture with different settings
		// Similar to the samplers available with OpenGL 3.3
		VkSamplerCreateInfo samplerCreateInfo = vkTools::initializers::samplerCreateInfo();
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
		if (mVulkanDevice->mFeatures.samplerAnisotropy)
		{
			// Use max. level of anisotropy for this example
			samplerCreateInfo.maxAnisotropy = mVulkanDevice->mProperties.limits.maxSamplerAnisotropy;
			samplerCreateInfo.anisotropyEnable = VK_TRUE;
		}
		else
		{
			// The device does not support anisotropic filtering
			samplerCreateInfo.maxAnisotropy = 1.0;
			samplerCreateInfo.anisotropyEnable = VK_FALSE;
		}
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(mDevice, &samplerCreateInfo, nullptr, &mTexture.sampler));

		// Create image view
		// Textures are not directly accessed by the shaders and
		// are abstracted by image views containing additional
		// information and sub resource ranges
		VkImageViewCreateInfo viewCreateInfo = vkTools::initializers::imageViewCreateInfo();
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
		VK_CHECK_RESULT(vkCreateImageView(mDevice, &viewCreateInfo, nullptr, &mTexture.imageView));

		// Fill image descriptor image info that can be used during the descriptor set setup
		mTexture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		mTexture.descriptor.imageView = mTexture.imageView;
		mTexture.descriptor.sampler = mTexture.sampler;
	}

	// Free all Vulkan resources used a texture object
	void destroyTextureImage(Texture texture)
	{
		vkDestroyImageView(mDevice, texture.imageView, nullptr);
		vkDestroyImage(mDevice, texture.image, nullptr);
		vkDestroySampler(mDevice, texture.sampler, nullptr);
		vkFreeMemory(mDevice, texture.deviceMemory, nullptr);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vkTools::initializers::renderPassBeginInfo();
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
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(mDrawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(mDrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vkTools::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vkTools::initializers::rect2D(width, height, 0, 0);
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

	void draw()
	{
		VulkanBase::prepareFrame();

		// Command buffer to be sumitted to the queue
		mSubmitInfo.commandBufferCount = 1;
		mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[mCurrentBuffer];

		// Submit to queue
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		VulkanBase::submitFrame();
	}

	void generateQuad()
	{
		// Setup vertices for a single uv-mapped quad made from two triangles
		std::vector<Vertex> vertices =
		{
			{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } },
			{ {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
		};

		// Setup indices
		std::vector<uint32_t> indices = { 0,1,2, 2,3,0 };
		mIndexCount = static_cast<uint32_t>(indices.size());

		// Create buffers
		// For the sake of simplicity we won't stage the vertex data to the gpu memory
		// Vertex buffer
		VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&mVertexBuffer,
			vertices.size() * sizeof(Vertex),
			vertices.data()));
		// Index buffer
		VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&mIndexBuffer,
			indices.size() * sizeof(uint32_t),
			indices.data()));
	}

	void setupVertexDescriptions()
	{
		// Binding description
		mVertices.bindingDescriptions.resize(1);
		mVertices.bindingDescriptions[0] =
			vkTools::initializers::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID, 
				sizeof(Vertex), 
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		// Describes memory layout and shader positions
		mVertices.attributeDescriptions.resize(3);
		// Location 0 : Position
		mVertices.attributeDescriptions[0] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				offsetof(Vertex, pos));			
		// Location 1 : Texture coordinates
		mVertices.attributeDescriptions[1] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32_SFLOAT,
				offsetof(Vertex, uv));
		// Location 1 : Vertex normal
		mVertices.attributeDescriptions[2] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				VK_FORMAT_R32G32B32_SFLOAT,
				offsetof(Vertex, normal));

		mVertices.inputState = vkTools::initializers::pipelineVertexInputStateCreateInfo();
		mVertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(mVertices.bindingDescriptions.size());
		mVertices.inputState.pVertexBindingDescriptions = mVertices.bindingDescriptions.data();
		mVertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(mVertices.attributeDescriptions.size());
		mVertices.inputState.pVertexAttributeDescriptions = mVertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		// Example uses one ubo and one image sampler
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo = 
			vkTools::initializers::descriptorPoolCreateInfo(
				static_cast<uint32_t>(poolSizes.size()),
				poolSizes.data(),
				2);

		VK_CHECK_RESULT(vkCreateDescriptorPool(mDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = 
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
				VK_SHADER_STAGE_VERTEX_BIT, 
				0),
			// Binding 1 : Fragment shader image sampler
			vkTools::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				VK_SHADER_STAGE_FRAGMENT_BIT, 
				1)
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout = 
			vkTools::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				static_cast<uint32_t>(setLayoutBindings.size()));

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vkTools::initializers::pipelineLayoutCreateInfo(
				&mDescriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(mDevice, &pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo = 
			vkTools::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&mDescriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(mDevice, &allocInfo, &mDescriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::initializers::writeDescriptorSet(
				mDescriptorSet, 
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
				0, 
				&mUniformBufferVS.descriptor),
			// Binding 1 : Fragment shader texture sampler
			vkTools::initializers::writeDescriptorSet(
				mDescriptorSet, 
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				1, 
				&mTexture.descriptor)
		};

		vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vkTools::initializers::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				0,
				VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vkTools::initializers::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_NONE,
				VK_FRONT_FACE_COUNTER_CLOCKWISE,
				0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vkTools::initializers::pipelineColorBlendAttachmentState(
				0xf,
				VK_FALSE);

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vkTools::initializers::pipelineColorBlendStateCreateInfo(
				1, 
				&blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vkTools::initializers::pipelineDepthStencilStateCreateInfo(
				VK_TRUE,
				VK_TRUE,
				VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
			vkTools::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vkTools::initializers::pipelineMultisampleStateCreateInfo(
				VK_SAMPLE_COUNT_1_BIT,
				0);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vkTools::initializers::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				static_cast<uint32_t>(dynamicStateEnables.size()),
				0);

		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo,2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/texture/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/texture/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vkTools::initializers::pipelineCreateInfo(
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

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipelines.solid));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Vertex shader uniform buffer block
		VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&mUniformBufferVS,
			sizeof(mUboVS),
			&mUboVS));

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		// Vertex shader
		mUboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.001f, 256.0f);
		glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		mUboVS.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
		mUboVS.model = glm::rotate(mUboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		mUboVS.model = glm::rotate(mUboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		mUboVS.model = glm::rotate(mUboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		mUboVS.viewPos = glm::vec4(0.0f, 0.0f, -zoom, 0.0f);

		VK_CHECK_RESULT(mUniformBufferVS.map());
		memcpy(mUniformBufferVS.mapped, &mUboVS, sizeof(mUboVS));
		mUniformBufferVS.unmap();
	}

	void prepare()
	{
		VulkanBase::prepare();
		generateQuad();
		setupVertexDescriptions();
		prepareUniformBuffers();
		loadTexture(
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

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
	}

	void changeLodBias(float delta)
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

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case KEY_KPADD:
		case GAMEPAD_BUTTON_R1:
			changeLodBias(0.1f);
			break;
		case KEY_KPSUB:
		case GAMEPAD_BUTTON_L1:
			changeLodBias(-0.1f);
			break;
		}
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
		std::stringstream ss;
		ss << std::setprecision(2) << std::fixed << mUboVS.lodBias;
#if defined(__ANDROID__)
		textOverlay->addText("LOD bias: " + ss.str() + " (Buttons L1/R1 to change)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#else
		textOverlay->addText("LOD bias: " + ss.str() + " (numpad +/- to change)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
	}
};

VULKAN_EXAMPLE_MAIN()
