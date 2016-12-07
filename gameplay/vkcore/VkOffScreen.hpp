#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include "define.h"

#include <vulkan/vulkan.h>
#include "VulkanBase.h"

#define ENABLE_VALIDATION false

// Offscreen frame buffer properties
#define FB_DIM 512
#define FB_COLOR_FORMAT VK_FORMAT_R8G8B8A8_UNORM


class VkOffScreen : public VulkanBase
{
	// Vertex layout for this example
	std::vector<vkMeshLoader::VertexLayout> vertexLayout =
	{
		vkMeshLoader::VERTEX_LAYOUT_POSITION,
		vkMeshLoader::VERTEX_LAYOUT_UV,
		vkMeshLoader::VERTEX_LAYOUT_COLOR,
		vkMeshLoader::VERTEX_LAYOUT_NORMAL
	};
public:
	bool debugDisplay = false;

	struct {
		vkTools::VulkanTexture colorMap;
	} textures;

	struct {
		vkMeshLoader::MeshBuffer example;
		vkMeshLoader::MeshBuffer quad;
		vkMeshLoader::MeshBuffer plane;
	} meshes;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct {
		vkTools::UniformData vsShared;
		vkTools::UniformData vsMirror;
		vkTools::UniformData vsOffScreen;
		vkTools::UniformData vsDebugQuad;
	} uniformData;

	struct UBO {
		Matrix projection;
		Matrix model;
		Vector4 lightPos = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	};

	struct {
		UBO vsShared;
	} ubos;

	struct {
		VkPipeline debug;
		VkPipeline shaded;
		VkPipeline shadedOffscreen;
		VkPipeline mirror;
	} pipelines;

	struct {
		VkPipelineLayout textured;
		VkPipelineLayout shaded;
	} pipelineLayouts;

	struct {
		VkDescriptorSet offscreen;
		VkDescriptorSet mirror;
		VkDescriptorSet model;
		VkDescriptorSet debugQuad;
	} descriptorSets;

	struct {
		VkDescriptorSetLayout textured;
		VkDescriptorSetLayout shaded;
	} descriptorSetLayouts;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	};
	struct OffscreenPass {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment color, depth;
		VkRenderPass renderPass;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		// Semaphore used to synchronize between offscreen and final scene render pass
		VkSemaphore semaphore = VK_NULL_HANDLE;
	} offscreenPass;

	Vector3 meshPos = Vector3(0.0f, -1.5f, 0.0f);
	Vector3 meshRot = Vector3(0.0f);

	VkOffScreen() : VulkanBase(ENABLE_VALIDATION)
	{
		mZoom = -6.0f;
		mRotation = { -2.5f, 0.0f, 0.0f };
		cameraPos = { 0.0f, 1.0f, 0.0f };
		timerSpeed *= 0.25f;
		mEnableTextOverlay = true;
		title = "Vulkan Example - Offscreen rendering";
	}

	~VkOffScreen()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class

		// Textures
		textureLoader->destroyTexture(textures.colorMap);

		// Frame buffer

		// Color attachment
		vkDestroyImageView(mVulkanDevice->mLogicalDevice, offscreenPass.color.view, nullptr);
		vkDestroyImage(mVulkanDevice->mLogicalDevice, offscreenPass.color.image, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, offscreenPass.color.mem, nullptr);

		// Depth attachment
		vkDestroyImageView(mVulkanDevice->mLogicalDevice, offscreenPass.depth.view, nullptr);
		vkDestroyImage(mVulkanDevice->mLogicalDevice, offscreenPass.depth.image, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, offscreenPass.depth.mem, nullptr);

		vkDestroyRenderPass(mVulkanDevice->mLogicalDevice, offscreenPass.renderPass, nullptr);
		vkDestroySampler(mVulkanDevice->mLogicalDevice, offscreenPass.sampler, nullptr);
		vkDestroyFramebuffer(mVulkanDevice->mLogicalDevice, offscreenPass.frameBuffer, nullptr);

		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.debug, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.shaded, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.shadedOffscreen, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.mirror, nullptr);

		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayouts.textured, nullptr);
		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayouts.shaded, nullptr);

		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayouts.shaded, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayouts.textured, nullptr);

		// Meshes
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.example);
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.quad);
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.plane);

		// Uniform buffers
		vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.vsShared);
		vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.vsMirror);
		vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.vsOffScreen);
		vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.vsDebugQuad);

		vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &offscreenPass.commandBuffer);
		vkDestroySemaphore(mVulkanDevice->mLogicalDevice, offscreenPass.semaphore, nullptr);
	}

	// Setup the offscreen framebuffer for rendering the mirrored scene
	// The color attachment of this framebuffer will then be used to sample from in the fragment shader of the final pass
	void prepareOffscreen()
	{
		offscreenPass.width = FB_DIM;
		offscreenPass.height = FB_DIM;

		// Find a suitable depth format
		VkFormat fbDepthFormat;
		VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(mVulkanDevice->mPhysicalDevice, &fbDepthFormat);
		assert(validDepthFormat);

		// Color attachment
		VkImageCreateInfo image = vkTools::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = FB_COLOR_FORMAT;
		image.extent.width = offscreenPass.width;
		image.extent.height = offscreenPass.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		// We will sample directly from the color attachment
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		VkMemoryAllocateInfo memAlloc = vkTools::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalDevice, &image, nullptr, &offscreenPass.color.image));
		vkGetImageMemoryRequirements(mVulkanDevice->mLogicalDevice, offscreenPass.color.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &offscreenPass.color.mem));
		VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalDevice, offscreenPass.color.image, offscreenPass.color.mem, 0));

		VkImageViewCreateInfo colorImageView = vkTools::initializers::imageViewCreateInfo();
		colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorImageView.format = FB_COLOR_FORMAT;
		colorImageView.subresourceRange = {};
		colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorImageView.subresourceRange.baseMipLevel = 0;
		colorImageView.subresourceRange.levelCount = 1;
		colorImageView.subresourceRange.baseArrayLayer = 0;
		colorImageView.subresourceRange.layerCount = 1;
		colorImageView.image = offscreenPass.color.image;
		VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalDevice, &colorImageView, nullptr, &offscreenPass.color.view));

		// Create sampler to sample from the attachment in the fragment shader
		VkSamplerCreateInfo samplerInfo = vkTools::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.maxAnisotropy = 0;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->mLogicalDevice, &samplerInfo, nullptr, &offscreenPass.sampler));

		// Depth stencil attachment
		image.format = fbDepthFormat;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalDevice, &image, nullptr, &offscreenPass.depth.image));
		vkGetImageMemoryRequirements(mVulkanDevice->mLogicalDevice, offscreenPass.depth.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &offscreenPass.depth.mem));
		VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalDevice, offscreenPass.depth.image, offscreenPass.depth.mem, 0));

		VkImageViewCreateInfo depthStencilView = vkTools::initializers::imageViewCreateInfo();
		depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		depthStencilView.format = fbDepthFormat;
		depthStencilView.flags = 0;
		depthStencilView.subresourceRange = {};
		depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		depthStencilView.subresourceRange.baseMipLevel = 0;
		depthStencilView.subresourceRange.levelCount = 1;
		depthStencilView.subresourceRange.baseArrayLayer = 0;
		depthStencilView.subresourceRange.layerCount = 1;
		depthStencilView.image = offscreenPass.depth.image;
		VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalDevice, &depthStencilView, nullptr, &offscreenPass.depth.view));

		// Create a separate render pass for the offscreen rendering as it may differ from the one used for scene rendering

		std::array<VkAttachmentDescription, 2> attchmentDescriptions = {};
		// Color attachment
		attchmentDescriptions[0].format = FB_COLOR_FORMAT;
		attchmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attchmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attchmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attchmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attchmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// Depth attachment
		attchmentDescriptions[1].format = fbDepthFormat;
		attchmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attchmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attchmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attchmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for layout transitions
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

		// Create the actual renderpass
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attchmentDescriptions.size());
		renderPassInfo.pAttachments = attchmentDescriptions.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(mVulkanDevice->mLogicalDevice, &renderPassInfo, nullptr, &offscreenPass.renderPass));

		VkImageView attachments[2];
		attachments[0] = offscreenPass.color.view;
		attachments[1] = offscreenPass.depth.view;

		VkFramebufferCreateInfo fbufCreateInfo = vkTools::initializers::framebufferCreateInfo();
		fbufCreateInfo.renderPass = offscreenPass.renderPass;
		fbufCreateInfo.attachmentCount = 2;
		fbufCreateInfo.pAttachments = attachments;
		fbufCreateInfo.width = offscreenPass.width;
		fbufCreateInfo.height = offscreenPass.height;
		fbufCreateInfo.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(mVulkanDevice->mLogicalDevice, &fbufCreateInfo, nullptr, &offscreenPass.frameBuffer));

		// Fill a descriptor for later use in a descriptor set 
		offscreenPass.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		offscreenPass.descriptor.imageView = offscreenPass.color.view;
		offscreenPass.descriptor.sampler = offscreenPass.sampler;
	}

	// Sets up the command buffer that renders the scene to the offscreen frame buffer
	void buildOffscreenCommandBuffer()
	{
		if (offscreenPass.commandBuffer == VK_NULL_HANDLE)
		{
			offscreenPass.commandBuffer = VulkanBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		}
		if (offscreenPass.semaphore == VK_NULL_HANDLE)
		{
			// Create a semaphore used to synchronize offscreen rendering and usage
			VkSemaphoreCreateInfo semaphoreCreateInfo = vkTools::initializers::semaphoreCreateInfo();
			VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &offscreenPass.semaphore));
		}

		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vkTools::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = offscreenPass.renderPass;
		renderPassBeginInfo.framebuffer = offscreenPass.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width = offscreenPass.width;
		renderPassBeginInfo.renderArea.extent.height = offscreenPass.height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		VK_CHECK_RESULT(vkBeginCommandBuffer(offscreenPass.commandBuffer, &cmdBufInfo));

		vkCmdBeginRenderPass(offscreenPass.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkTools::initializers::viewport((float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
		vkCmdSetViewport(offscreenPass.commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = vkTools::initializers::rect2D(offscreenPass.width, offscreenPass.height, 0, 0);
		vkCmdSetScissor(offscreenPass.commandBuffer, 0, 1, &scissor);

		VkDeviceSize offsets[1] = { 0 };

		// Mirrored scene
		vkCmdBindDescriptorSets(offscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.shaded, 0, 1, &descriptorSets.offscreen, 0, NULL);
		vkCmdBindPipeline(offscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadedOffscreen);
		vkCmdBindVertexBuffers(offscreenPass.commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &meshes.example.vertices.buf, offsets);
		vkCmdBindIndexBuffer(offscreenPass.commandBuffer, meshes.example.indices.buf, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(offscreenPass.commandBuffer, meshes.example.indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(offscreenPass.commandBuffer);

		VK_CHECK_RESULT(vkEndCommandBuffer(offscreenPass.commandBuffer));
	}

	void reBuildCommandBuffers()
	{
		if (!checkCommandBuffers())
		{
			destroyCommandBuffers();
			createCommandBuffers();
		}
		buildCommandBuffers();
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
			renderPassBeginInfo.framebuffer = mFrameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(mDrawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(mDrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vkTools::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vkTools::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize offsets[1] = { 0 };

			if (debugDisplay)
			{
				vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.textured, 0, 1, &descriptorSets.debugQuad, 0, NULL);
				vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.debug);
				vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.quad.vertices.buf, offsets);
				vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.quad.indices.buf, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.quad.indexCount, 1, 0, 0, 0);
			}

			// Scene
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.debug);

			// Reflection plane
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.textured, 0, 1, &descriptorSets.mirror, 0, NULL);
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.mirror);

			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.plane.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.plane.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.plane.indexCount, 1, 0, 0, 0);

			// Model
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.shaded, 0, 1, &descriptorSets.model, 0, NULL);
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shaded);

			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.example.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.example.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.example.indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(mDrawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		loadMesh(getAssetPath() + "models/plane.obj", &meshes.plane, vertexLayout, 0.5f);
		loadMesh(getAssetPath() + "models/chinesedragon.dae", &meshes.example, vertexLayout, 0.3f);
		textureLoader->loadTexture(getAssetPath() + "textures/darkmetal_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &textures.colorMap);
	}

	void generateQuad()
	{
		// Setup vertices for a single uv-mapped quad
		struct Vertex {
			float pos[3];
			float uv[2];
			float col[3];
			float normal[3];
		};

#define QUAD_COLOR_NORMAL { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }
		std::vector<Vertex> vertexBuffer =
		{
			{ { 1.0f, 1.0f, 0.0f },{ 1.0f, 1.0f }, QUAD_COLOR_NORMAL },
			{ { 0.0f, 1.0f, 0.0f },{ 0.0f, 1.0f }, QUAD_COLOR_NORMAL },
			{ { 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f }, QUAD_COLOR_NORMAL },
			{ { 1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f }, QUAD_COLOR_NORMAL }
		};
#undef QUAD_COLOR_NORMAL

		createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			vertexBuffer.size() * sizeof(Vertex),
			vertexBuffer.data(),
			&meshes.quad.vertices.buf,
			&meshes.quad.vertices.mem);

		// Setup indices
		std::vector<uint32_t> indexBuffer = { 0,1,2, 2,3,0 };
		meshes.quad.indexCount = indexBuffer.size();

		createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			indexBuffer.size() * sizeof(uint32_t),
			indexBuffer.data(),
			&meshes.quad.indices.buf,
			&meshes.quad.indices.mem);
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
			vkTools::initializers::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				vkMeshLoader::vertexSize(vertexLayout),
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		vertices.attributeDescriptions.resize(4);
		// Location 0 : Position
		vertices.attributeDescriptions[0] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				0);
		// Location 1 : Texture coordinates
		vertices.attributeDescriptions[1] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32_SFLOAT,
				sizeof(float) * 3);
		// Location 2 : Color
		vertices.attributeDescriptions[2] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 5);
		// Location 3 : Normal
		vertices.attributeDescriptions[3] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				3,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 8);

		vertices.inputState = vkTools::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6),
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8)
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vkTools::initializers::descriptorPoolCreateInfo(
				poolSizes.size(),
				poolSizes.data(),
				5);

		VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo;

		// Binding 0 : Vertex shader uniform buffer
		setLayoutBindings.push_back(vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT,
			0));
		// Binding 1 : Fragment shader image sampler
		setLayoutBindings.push_back(vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			1));
		// Binding 2 : Fragment shader image sampler
		setLayoutBindings.push_back(vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			2));

		// Shaded layouts (only use first layout binding)
		descriptorLayoutInfo = vkTools::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), 1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalDevice, &descriptorLayoutInfo, nullptr, &descriptorSetLayouts.shaded));

		pipelineLayoutInfo = vkTools::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.shaded, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayouts.shaded));

		// Textured layouts (use all layout bindings)
		descriptorLayoutInfo = vkTools::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalDevice, &descriptorLayoutInfo, nullptr, &descriptorSetLayouts.textured));

		pipelineLayoutInfo = vkTools::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.textured, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayouts.textured));
	}

	void setupDescriptorSet()
	{
		// Mirror plane descriptor set
		VkDescriptorSetAllocateInfo allocInfo =
			vkTools::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayouts.textured,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.mirror));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.mirror,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformData.vsMirror.descriptor),
			// Binding 1 : Fragment shader texture sampler
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.mirror,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&offscreenPass.descriptor),
			// Binding 2 : Fragment shader texture sampler
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.mirror,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				2,
				&textures.colorMap.descriptor)
		};

		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

		// Debug quad
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.debugQuad));

		std::vector<VkWriteDescriptorSet> debugQuadWriteDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.debugQuad,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformData.vsDebugQuad.descriptor),
			// Binding 1 : Fragment shader texture sampler
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.debugQuad,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&offscreenPass.descriptor)
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, debugQuadWriteDescriptorSets.size(), debugQuadWriteDescriptorSets.data(), 0, NULL);

		// Shaded descriptor sets
		allocInfo.pSetLayouts = &descriptorSetLayouts.shaded;

		// Model
		// No texture
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.model));

		std::vector<VkWriteDescriptorSet> modelWriteDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.model,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformData.vsShared.descriptor)
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, modelWriteDescriptorSets.size(), modelWriteDescriptorSets.data(), 0, NULL);

		// Offscreen
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.offscreen));

		std::vector<VkWriteDescriptorSet> offScreenWriteDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.offscreen,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformData.vsOffScreen.descriptor)
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, offScreenWriteDescriptorSets.size(), offScreenWriteDescriptorSets.data(), 0, NULL);
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
				VK_CULL_MODE_FRONT_BIT,
				VK_FRONT_FACE_CLOCKWISE,
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
				dynamicStateEnables.size(),
				0);

		// Solid rendering pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/offscreen/quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/offscreen/quad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vkTools::initializers::pipelineCreateInfo(
				pipelineLayouts.textured,
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

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.debug));

		// Mirror
		shaderStages[0] = loadShader(getAssetPath() + "shaders/offscreen/mirror.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/offscreen/mirror.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.mirror));

		// Flip culling
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

		// Phong shading pipelines
		pipelineCreateInfo.layout = pipelineLayouts.shaded;
		// Scene
		shaderStages[0] = loadShader(getAssetPath() + "shaders/offscreen/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/offscreen/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.shaded));
		// Offscreen
		// Flip culling
		pipelineCreateInfo.renderPass = offscreenPass.renderPass;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.shadedOffscreen));

	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Mesh vertex shader uniform buffer block
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(ubos.vsShared),
			nullptr,
			&uniformData.vsShared.buffer,
			&uniformData.vsShared.memory,
			&uniformData.vsShared.descriptor);

		// Mirror plane vertex shader uniform buffer block
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(ubos.vsShared),
			nullptr,
			&uniformData.vsMirror.buffer,
			&uniformData.vsMirror.memory,
			&uniformData.vsMirror.descriptor);

		// Offscreen vertex shader uniform buffer block 
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(ubos.vsShared),
			nullptr,
			&uniformData.vsOffScreen.buffer,
			&uniformData.vsOffScreen.memory,
			&uniformData.vsOffScreen.descriptor);

		// Debug quad vertex shader uniform buffer block 
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(ubos.vsShared),
			nullptr,
			&uniformData.vsDebugQuad.buffer,
			&uniformData.vsDebugQuad.memory,
			&uniformData.vsDebugQuad.descriptor);

		updateUniformBuffers();
		updateUniformBufferOffscreen();
	}

	void updateUniformBuffers()
	{
		// Mesh
		//ubos.vsShared.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
		//glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, mZoom));
		//ubos.vsShared.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.y + meshRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		//ubos.vsShared.model = glm::translate(ubos.vsShared.model, meshPos);

		Matrix viewMatrix, matTmp;
		Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), (float)width / (float)height, 0.1f, 256.0f, &ubos.vsShared.projection);
		viewMatrix.translate(0, 0, mZoom);
		matTmp.translate(cameraPos);
		ubos.vsShared.model = viewMatrix * matTmp;
		ubos.vsShared.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
		ubos.vsShared.model.rotateY(MATH_DEG_TO_RAD(mRotation.y + meshRot.y));
		ubos.vsShared.model.rotateZ(MATH_DEG_TO_RAD(mRotation.z));
		ubos.vsShared.model.translate(meshPos);

		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsShared.memory, 0, sizeof(ubos.vsShared), 0, (void **)&pData));
		memcpy(pData, &ubos.vsShared, sizeof(ubos.vsShared));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsShared.memory);

		// Mirror
		//ubos.vsShared.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		matTmp.setIdentity();
		matTmp.translate(cameraPos);
		ubos.vsShared.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
		ubos.vsShared.model.rotateY(MATH_DEG_TO_RAD(mRotation.y));
		ubos.vsShared.model.rotateZ(MATH_DEG_TO_RAD(mRotation.z));

		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsMirror.memory, 0, sizeof(ubos.vsShared), 0, (void **)&pData));
		memcpy(pData, &ubos.vsShared, sizeof(ubos.vsShared));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsMirror.memory);

		//// Debug quad
		//ubos.vsShared.projection = glm::ortho(4.0f, 0.0f, 0.0f, 4.0f*(float)height / (float)width, -1.0f, 1.0f);
		//ubos.vsShared.model = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, 0.0f));
		Matrix::createOrthographicOffCenter(4.0f, 0.0f, 0.0f, 4.0f*(float)height / (float)width, -1.0f, 1.0f, &ubos.vsShared.projection);
		Matrix::createTranslation(Vector3(0, 0, 0), &ubos.vsShared.model);

		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsDebugQuad.memory, 0, sizeof(ubos.vsShared), 0, (void **)&pData));
		memcpy(pData, &ubos.vsShared, sizeof(ubos.vsShared));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsDebugQuad.memory);
	}

	void updateUniformBufferOffscreen()
	{
		//ubos.vsShared.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
		//glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, mZoom));
		//ubos.vsShared.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.y + meshRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
		//ubos.vsShared.model = glm::rotate(ubos.vsShared.model, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		//ubos.vsShared.model = glm::scale(ubos.vsShared.model, glm::vec3(1.0f, -1.0f, 1.0f));
		//ubos.vsShared.model = glm::translate(ubos.vsShared.model, meshPos);

		Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), (float)width / (float)height, 0.1f, 256.0f, &ubos.vsShared.projection);
		Matrix viewMatrix, matTmp;
		viewMatrix.translate(0, 0, mZoom);
		matTmp.translate(cameraPos);
		ubos.vsShared.model = viewMatrix * matTmp;
		ubos.vsShared.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
		ubos.vsShared.model.rotateY(MATH_DEG_TO_RAD(mRotation.y + meshRot.y));
		ubos.vsShared.model.rotateZ(MATH_DEG_TO_RAD(mRotation.z));
		ubos.vsShared.model.scale(Vector3(1.0f, -1.0f, 1.0f));
		ubos.vsShared.model.translate(meshPos);


		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsOffScreen.memory, 0, sizeof(ubos.vsShared), 0, (void **)&pData));
		memcpy(pData, &ubos.vsShared, sizeof(ubos.vsShared));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsOffScreen.memory);
	}

	void draw()
	{
		VulkanBase::prepareFrame();

		// The scene render command buffer has to wait for the offscreen
		// rendering to be finished before we can use the framebuffer 
		// color image for sampling during final rendering
		// To ensure this we use a dedicated offscreen synchronization
		// semaphore that will be signaled when offscreen renderin
		// has been finished
		// This is necessary as an implementation may start both
		// command buffers at the same time, there is no guarantee
		// that command buffers will be executed in the order they
		// have been submitted by the application

		// Offscreen rendering

		// Wait for swap chain presentation to finish
		mSubmitInfo.pWaitSemaphores = &semaphores.presentComplete;
		// Signal ready with offscreen semaphore
		mSubmitInfo.pSignalSemaphores = &offscreenPass.semaphore;

		// Submit work
		mSubmitInfo.commandBufferCount = 1;
		mSubmitInfo.pCommandBuffers = &offscreenPass.commandBuffer;
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		// Scene rendering

		// Wait for offscreen semaphore
		mSubmitInfo.pWaitSemaphores = &offscreenPass.semaphore;
		// Signal ready with render complete semaphpre
		mSubmitInfo.pSignalSemaphores = &semaphores.renderComplete;

		// Submit work
		mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[mCurrentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		VulkanBase::submitFrame();
	}

	void prepare()
	{
		VulkanBase::prepare();
		loadAssets();
		generateQuad();
		prepareOffscreen();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		buildOffscreenCommandBuffer();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
		if (!paused)
		{
			meshRot.y += frameTimer * 10.0f;
			updateUniformBuffers();
			updateUniformBufferOffscreen();
		}
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
		updateUniformBufferOffscreen();
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case KEY_D:
		case GAMEPAD_BUTTON_A:
			toggleDebugDisplay();
			break;
		}
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
#if defined(__ANDROID__)
		textOverlay->addText("Press \"Button A\" to display offscreen target", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#else
		textOverlay->addText("Press \"d\" to display offscreen target", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
	}

	void toggleDebugDisplay()
	{
		debugDisplay = !debugDisplay;
		reBuildCommandBuffers();
	}
};

