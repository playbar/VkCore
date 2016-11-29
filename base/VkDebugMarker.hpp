#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include "define.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "VulkanBase.h"


#define ENABLE_VALIDATION false

// Offscreen properties
#define OFFSCREEN_DIM 256
#define OFFSCREEN_FORMAT VK_FORMAT_R8G8B8A8_UNORM
#define OFFSCREEN_FILTER VK_FILTER_LINEAR;


namespace DebugMarker
{
	bool active = false;

	PFN_vkDebugMarkerSetObjectTagEXT pfnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
	PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert = VK_NULL_HANDLE;

	// Get function pointers for the debug report extensions from the device
	void setup(VkDevice device)
	{
		pfnDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT");
		pfnDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT");
		pfnCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT");
		pfnCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT");
		pfnCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT");

		// Set flag if at least one function pointer is present
		active = (pfnDebugMarkerSetObjectName != VK_NULL_HANDLE);
	}

	// Sets the debug name of an object
	// All Objects in Vulkan are represented by their 64-bit handles which are passed into this function
	// along with the object type
	void setObjectName(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char *name)
	{
		// Check for valid function pointer (may not be present if not running in a debugging application)
		if (pfnDebugMarkerSetObjectName)
		{
			VkDebugMarkerObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = objectType;
			nameInfo.object = object;
			nameInfo.pObjectName = name;
			pfnDebugMarkerSetObjectName(device, &nameInfo);
		}
	}

	// Set the tag for an object
	void setObjectTag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag)
	{
		// Check for valid function pointer (may not be present if not running in a debugging application)
		if (pfnDebugMarkerSetObjectTag)
		{
			VkDebugMarkerObjectTagInfoEXT tagInfo = {};
			tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
			tagInfo.objectType = objectType;
			tagInfo.object = object;
			tagInfo.tagName = name;
			tagInfo.tagSize = tagSize;
			tagInfo.pTag = tag;
			pfnDebugMarkerSetObjectTag(device, &tagInfo);
		}
	}

	// Start a new debug marker region
	void beginRegion(VkCommandBuffer cmdbuffer, const char* pMarkerName, glm::vec4 color)
	{
		// Check for valid function pointer (may not be present if not running in a debugging application)
		if (pfnCmdDebugMarkerBegin)
		{
			VkDebugMarkerMarkerInfoEXT markerInfo = {};
			markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
			markerInfo.pMarkerName = pMarkerName;
			pfnCmdDebugMarkerBegin(cmdbuffer, &markerInfo);
		}
	}

	// Insert a new debug marker into the command buffer
	void insert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color)
	{
		// Check for valid function pointer (may not be present if not running in a debugging application)
		if (pfnCmdDebugMarkerInsert)
		{
			VkDebugMarkerMarkerInfoEXT markerInfo = {};
			markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
			markerInfo.pMarkerName = markerName.c_str();
			pfnCmdDebugMarkerInsert(cmdbuffer, &markerInfo);
		}
	}

	// End the current debug marker region
	void endRegion(VkCommandBuffer cmdBuffer)
	{
		// Check for valid function (may not be present if not runnin in a debugging application)
		if (pfnCmdDebugMarkerEnd)
		{
			pfnCmdDebugMarkerEnd(cmdBuffer);
		}
	}
};

struct BugMarScene {
	struct {
		VkBuffer buf;
		VkDeviceMemory mem;
	} vertices;
	struct {
		VkBuffer buf;
		VkDeviceMemory mem;
	} indices;

	// Store mesh offsets for vertex and indexbuffers
	struct Mesh
	{
		uint32_t indexStart;
		uint32_t indexCount;
		std::string name;
	};
	std::vector<Mesh> meshes;

	void draw(VkCommandBuffer cmdBuffer)
	{
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(cmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &vertices.buf, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, indices.buf, 0, VK_INDEX_TYPE_UINT32);
		for (auto mesh : meshes)
		{
			// Add debug marker for mesh name
			DebugMarker::insert(cmdBuffer, "Draw \"" + mesh.name + "\"", glm::vec4(0.0f));
			vkCmdDrawIndexed(cmdBuffer, mesh.indexCount, 1, mesh.indexStart, 0, 0);
		}
	}
};

class VkDebugMarker : public VulkanBase
{
	// Vertex layout used in this example
	struct Vertex 
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
	};
public:
	bool wireframe = true;
	bool glow = true;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	BugMarScene scene, sceneGlow;

	struct {
		vkTools::UniformData vsScene;
	} uniformData;

	struct {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(0.0f, 5.0f, 15.0f, 1.0f);
	} uboVS;

	struct {
		VkPipeline toonshading;
		VkPipeline color;
		VkPipeline wireframe;
		VkPipeline postprocess;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	struct {
		VkDescriptorSet scene;
		VkDescriptorSet fullscreen;
	} descriptorSets;

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

	// Random tag data
	struct {
		const char name[17] = "debug marker tag";
	} demoTag;

	VkDebugMarker() : VulkanBase(ENABLE_VALIDATION)
	{
		zoom = -8.5f;
		zoomSpeed = 2.5f;
		rotationSpeed = 0.5f;
		rotation = { -4.35f, 16.25f, 0.0f };
		cameraPos = { 0.1f, 1.1f, 0.0f };
		mEnableTextOverlay = true;
		title = "Vulkan Example - VK_EXT_debug_marker";
	}

	~VkDebugMarker()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.toonshading, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.color, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.wireframe, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.postprocess, nullptr);

		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayout, nullptr);

		// Destroy and free mesh resources 
		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, scene.vertices.buf, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, scene.vertices.mem, nullptr);
		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, scene.indices.buf, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, scene.indices.mem, nullptr);
		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, sceneGlow.vertices.buf, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, sceneGlow.vertices.mem, nullptr);
		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, sceneGlow.indices.buf, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, sceneGlow.indices.mem, nullptr);

		vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.vsScene);

		// Offscreen
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

		vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &offscreenPass.commandBuffer);
		vkDestroySemaphore(mVulkanDevice->mLogicalDevice, offscreenPass.semaphore, nullptr);
	}

	// Prepare a texture target and framebuffer for offscreen rendering
	void prepareOffscreen()
	{
		offscreenPass.width = OFFSCREEN_DIM;
		offscreenPass.height = OFFSCREEN_DIM;

		// Find a suitable depth format
		VkFormat fbDepthFormat;
		VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(mVulkanDevice->mPhysicalDevice, &fbDepthFormat);
		assert(validDepthFormat);

		// Color attachment
		VkImageCreateInfo image = vkTools::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = OFFSCREEN_FORMAT;
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
		colorImageView.format = OFFSCREEN_FORMAT;
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
		samplerInfo.magFilter = OFFSCREEN_FILTER;
		samplerInfo.minFilter = OFFSCREEN_FILTER;
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
		attchmentDescriptions[0].format = OFFSCREEN_FORMAT;
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

		// Name some objects for debugging
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)offscreenPass.color.image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, "Off-screen color framebuffer");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)offscreenPass.depth.image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, "Off-screen depth framebuffer");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)offscreenPass.sampler, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, "Off-screen framebuffer default sampler");
	}

	// Command buffer for rendering color only scene for glow
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

		// Start a new debug marker region
		DebugMarker::beginRegion(offscreenPass.commandBuffer, "Off-screen scene rendering", glm::vec4(1.0f, 0.78f, 0.05f, 1.0f));

		VkViewport viewport = vkTools::initializers::viewport((float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
		vkCmdSetViewport(offscreenPass.commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = vkTools::initializers::rect2D(offscreenPass.width, offscreenPass.height, 0, 0);
		vkCmdSetScissor(offscreenPass.commandBuffer, 0, 1, &scissor);

		vkCmdBeginRenderPass(offscreenPass.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindDescriptorSets(offscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.scene, 0, NULL);
		vkCmdBindPipeline(offscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.color);

		// Draw glow scene
		sceneGlow.draw(offscreenPass.commandBuffer);

		vkCmdEndRenderPass(offscreenPass.commandBuffer);

		DebugMarker::endRegion(offscreenPass.commandBuffer);

		VK_CHECK_RESULT(vkEndCommandBuffer(offscreenPass.commandBuffer));
	}

	// Load a model file as separate meshes into a scene
	void loadModel(std::string filename, BugMarScene *scene)
	{
		VulkanMeshLoader *meshLoader = new VulkanMeshLoader(mVulkanDevice);
#if defined(__ANDROID__)
		meshLoader->assetManager = androidApp->activity->assetManager;
#endif
		meshLoader->LoadMesh(filename);

		scene->meshes.resize(meshLoader->m_Entries.size());

		// Generate vertex buffer
		float scale = 1.0f;
		std::vector<Vertex> vertexBuffer;
		// Iterate through all meshes in the file
		// and extract the vertex information used in this demo
		for (uint32_t m = 0; m < meshLoader->m_Entries.size(); m++)
		{
			for (uint32_t i = 0; i < meshLoader->m_Entries[m].Vertices.size(); i++)
			{
				Vertex vertex;

				vertex.pos = meshLoader->m_Entries[m].Vertices[i].m_pos * scale;
				vertex.normal = meshLoader->m_Entries[m].Vertices[i].m_normal;
				vertex.uv = meshLoader->m_Entries[m].Vertices[i].m_tex;
				vertex.color = meshLoader->m_Entries[m].Vertices[i].m_color;

				vertexBuffer.push_back(vertex);
			}
		}
		uint32_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);

		// Generate index buffer from loaded mesh file
		std::vector<uint32_t> indexBuffer;
		for (uint32_t m = 0; m < meshLoader->m_Entries.size(); m++)
		{
			uint32_t indexBase = indexBuffer.size();
			for (uint32_t i = 0; i < meshLoader->m_Entries[m].Indices.size(); i++)
			{
				indexBuffer.push_back(meshLoader->m_Entries[m].Indices[i] + indexBase);
			}
			scene->meshes[m].indexStart = indexBase;
			scene->meshes[m].indexCount = meshLoader->m_Entries[m].Indices.size();
		}
		uint32_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);

		// Static mesh should always be device local

		bool useStaging = true;

		if (useStaging)
		{
			struct {
				VkBuffer buffer;
				VkDeviceMemory memory;
			} vertexStaging, indexStaging;

			// Create staging buffers
			// Vertex data
			createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				vertexBufferSize,
				vertexBuffer.data(),
				&vertexStaging.buffer,
				&vertexStaging.memory);
			// Index data
			createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				indexBufferSize,
				indexBuffer.data(),
				&indexStaging.buffer,
				&indexStaging.memory);

			// Create device local buffers
			// Vertex buffer
			createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vertexBufferSize,
				nullptr,
				&scene->vertices.buf,
				&scene->vertices.mem);
			// Index buffer
			createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				indexBufferSize,
				nullptr,
				&scene->indices.buf,
				&scene->indices.mem);

			// Copy from staging buffers
			VkCommandBuffer copyCmd = VulkanBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkBufferCopy copyRegion = {};

			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(
				copyCmd,
				vertexStaging.buffer,
				scene->vertices.buf,
				1,
				&copyRegion);

			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(
				copyCmd,
				indexStaging.buffer,
				scene->indices.buf,
				1,
				&copyRegion);

			VulkanBase::flushCommandBuffer(copyCmd, mQueue, true);

			vkDestroyBuffer(mVulkanDevice->mLogicalDevice, vertexStaging.buffer, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalDevice, vertexStaging.memory, nullptr);
			vkDestroyBuffer(mVulkanDevice->mLogicalDevice, indexStaging.buffer, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalDevice, indexStaging.memory, nullptr);
		}
		else
		{
			// Vertex buffer
			createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				vertexBufferSize,
				vertexBuffer.data(),
				&scene->vertices.buf,
				&scene->vertices.mem);
			// Index buffer
			createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				indexBufferSize,
				indexBuffer.data(),
				&scene->indices.buf,
				&scene->indices.mem);
		}

		delete(meshLoader);
	}

	void loadScene()
	{
		loadModel(getAssetPath() + "models/treasure_smooth.dae", &scene);
		loadModel(getAssetPath() + "models/treasure_glow.dae", &sceneGlow);

		// Name the meshes
		// ASSIMP does not load mesh names from the COLLADA file used in this example
		// so we need to set them manually
		// These names are used in command buffer creation for setting debug markers
		// Scene
		std::vector<std::string> names = { "hill", "rocks", "cave", "tree", "mushroom stems", "blue mushroom caps", "red mushroom caps", "grass blades", "chest box", "chest fittings" };
		for (size_t i = 0; i < names.size(); i++)
		{
			scene.meshes[i].name = names[i];
			sceneGlow.meshes[i].name = names[i];
		}

		// Name the buffers for debugging
		// Scene
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)scene.vertices.buf, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "Scene vertex buffer");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)scene.indices.buf, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "Scene index buffer");
		// Glow
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)sceneGlow.vertices.buf, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "Glow vertex buffer");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)sceneGlow.indices.buf, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "Glow index buffer");
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
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(mDrawCmdBuffers[i], &cmdBufInfo));

			// Start a new debug marker region
			DebugMarker::beginRegion(mDrawCmdBuffers[i], "Render scene", glm::vec4(0.5f, 0.76f, 0.34f, 1.0f));

			vkCmdBeginRenderPass(mDrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vkTools::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vkTools::initializers::rect2D(wireframe ? width / 2 : width, height, 0, 0);
			vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.scene, 0, NULL);

			// Solid rendering

			// Start a new debug marker region
			DebugMarker::beginRegion(mDrawCmdBuffers[i], "Toon shading draw", glm::vec4(0.78f, 0.74f, 0.9f, 1.0f));

			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.toonshading);
			scene.draw(mDrawCmdBuffers[i]);

			DebugMarker::endRegion(mDrawCmdBuffers[i]);

			// Wireframe rendering
			if (wireframe)
			{
				// Insert debug marker
				DebugMarker::beginRegion(mDrawCmdBuffers[i], "Wireframe draw", glm::vec4(0.53f, 0.78f, 0.91f, 1.0f));

				scissor.offset.x = width / 2;
				vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireframe);
				scene.draw(mDrawCmdBuffers[i]);

				DebugMarker::endRegion(mDrawCmdBuffers[i]);

				scissor.offset.x = 0;
				scissor.extent.width = width;
				vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);
			}

			// Post processing
			if (glow)
			{
				DebugMarker::beginRegion(mDrawCmdBuffers[i], "Apply post processing", glm::vec4(0.93f, 0.89f, 0.69f, 1.0f));

				vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.postprocess);
				// Full screen quad is generated by the vertex shaders, so we reuse four vertices (for four invocations) from current vertex buffer
				vkCmdDraw(mDrawCmdBuffers[i], 4, 1, 0, 0);

				DebugMarker::endRegion(mDrawCmdBuffers[i]);
			}


			vkCmdEndRenderPass(mDrawCmdBuffers[i]);

			// End current debug marker region
			DebugMarker::endRegion(mDrawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
		}
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
			vkTools::initializers::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				sizeof(Vertex),
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(4);
		// Location 0 : Position
		vertices.attributeDescriptions[0] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				0);
		// Location 1 : Normal
		vertices.attributeDescriptions[1] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 3);
		// Location 2 : Texture coordinates
		vertices.attributeDescriptions[2] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				VK_FORMAT_R32G32_SFLOAT,
				sizeof(float) * 6);
		// Location 3 : Color
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
		// Example uses one ubo and one combined image sampler
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vkTools::initializers::descriptorPoolCreateInfo(
				poolSizes.size(),
				poolSizes.data(),
				1);

		VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
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
			// Binding 1 : Fragment shader combined sampler
			vkTools::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				1),
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vkTools::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				setLayoutBindings.size());

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vkTools::initializers::pipelineLayoutCreateInfo(
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Name for debugging
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Shared pipeline layout");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)descriptorSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, "Shared descriptor set layout");
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo =
			vkTools::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.scene));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.scene,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformData.vsScene.descriptor),
			// Binding 1 : Color map 
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.scene,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&offscreenPass.descriptor)
		};

		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
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
				VK_CULL_MODE_BACK_BIT,
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

		// Phong lighting pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/debugmarker/toon.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/debugmarker/toon.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vkTools::initializers::pipelineCreateInfo(
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

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.toonshading));

		// Color only pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/debugmarker/colorpass.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/debugmarker/colorpass.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCreateInfo.renderPass = offscreenPass.renderPass;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.color));

		// Wire frame rendering pipeline
		rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationState.lineWidth = 1.0f;
		pipelineCreateInfo.renderPass = mRenderPass;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wireframe));

		// Post processing effect
		shaderStages[0] = loadShader(getAssetPath() + "shaders/debugmarker/postprocess.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/debugmarker/postprocess.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		depthStencilState.depthTestEnable = VK_FALSE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		blendAttachmentState.colorWriteMask = 0xF;
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.postprocess));

		// Name shader moduels for debugging
		// Shader module count starts at 2 when text overlay in base class is enabled
		uint32_t moduleIndex = mEnableTextOverlay ? 2 : 0;
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)shaderModules[moduleIndex + 0], VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, "Toon shading vertex shader");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)shaderModules[moduleIndex + 1], VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, "Toon shading fragment shader");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)shaderModules[moduleIndex + 2], VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, "Color-only vertex shader");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)shaderModules[moduleIndex + 3], VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, "Color-only fragment shader");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)shaderModules[moduleIndex + 4], VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, "Postprocess vertex shader");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)shaderModules[moduleIndex + 5], VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, "Postprocess fragment shader");

		// Name pipelines for debugging
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)pipelines.toonshading, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Toon shading pipeline");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)pipelines.color, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Color only pipeline");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)pipelines.wireframe, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Wireframe rendering pipeline");
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)pipelines.postprocess, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Post processing pipeline");
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Vertex shader uniform buffer block
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uboVS),
			&uboVS,
			&uniformData.vsScene.buffer,
			&uniformData.vsScene.memory,
			&uniformData.vsScene.descriptor);

		// Name uniform buffer for debugging
		DebugMarker::setObjectName(mVulkanDevice->mLogicalDevice, (uint64_t)uniformData.vsScene.buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, "Scene uniform buffer block");
		// Add some random tag
		DebugMarker::setObjectTag(mVulkanDevice->mLogicalDevice, (uint64_t)uniformData.vsScene.buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, 0, sizeof(demoTag), &demoTag);

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
		glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uboVS.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsScene.memory, 0, sizeof(uboVS), 0, (void **)&pData));
		memcpy(pData, &uboVS, sizeof(uboVS));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsScene.memory);
	}

	void draw()
	{
		VulkanBase::prepareFrame();

		// Offscreen rendering
		if (glow)
		{
			// Wait for swap chain presentation to finish
			mSubmitInfo.pWaitSemaphores = &semaphores.presentComplete;
			// Signal ready with offscreen semaphore
			mSubmitInfo.pSignalSemaphores = &offscreenPass.semaphore;

			// Submit work
			mSubmitInfo.commandBufferCount = 1;
			mSubmitInfo.pCommandBuffers = &offscreenPass.commandBuffer;
			VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));
		}

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
		DebugMarker::setup(mVulkanDevice->mLogicalDevice);
		loadScene();
		prepareOffscreen();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		buildOffscreenCommandBuffer();
		updateTextOverlay();
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

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case 0x57:
		case GAMEPAD_BUTTON_X:
			wireframe = !wireframe;
			reBuildCommandBuffers();
			break;
		case 0x47:
		case GAMEPAD_BUTTON_A:
			glow = !glow;
			reBuildCommandBuffers();
			break;
		}
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
		if (DebugMarker::active)
		{
			textOverlay->addText("VK_EXT_debug_marker active", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		}
		else
		{
			textOverlay->addText("VK_EXT_debug_marker not present", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		}
	}
};
