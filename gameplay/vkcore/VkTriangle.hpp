#pragma once

#include "define.h"


#define ENABLE_VALIDATION true
#define USE_STAGING true

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "VulkanBase.h"
#include "VDeleter.hpp"

#include <iostream> 
#include <iomanip>

class VkTriangle : public VulkanBase
{
	struct Vertex
	{
		float position[3];
		float color[3];
	};

public:
	// Vertex buffer and attributes
	struct
	{
		VkDeviceMemory memory;															// Handle to the device memory for this buffer
		VkBuffer buffer;																// Handle to the Vulkan buffer object that the memory is bound to
		VkPipelineVertexInputStateCreateInfo inputState;
		VkVertexInputBindingDescription inputBinding;
		std::vector<VkVertexInputAttributeDescription> inputAttributes;
	} mVertices;


	// Index buffer
	struct
	{
		VkDeviceMemory memory;
		VkBuffer buffer;
		uint32_t count;
	} mIndices;


	// For simplicity we use the same uniform block layout as in the shader:
	//	layout(set = 0, binding = 0) uniform UBO
	//	{
	//		mat4 projectionMatrix;
	//		mat4 modelMatrix;
	//		mat4 viewMatrix;
	//	} ubo;
	//
	// This way we can just memcopy the ubo data to the ubo
	// Note: You should use data types that align with the GPU in order to avoid manual padding (vec4, mat4)
	struct
	{
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	} mUboVS;

	// Uniform block object
	struct
	{
		VkDeviceMemory memory;
		VkBuffer buffer;
		VkDescriptorBufferInfo descriptor;
	}  mUniformDataVS;

	// The pipeline layout is used by a pipline to access the descriptor sets 
	// It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
	// A pipeline layout can be shared among multiple pipelines as long as their interfaces match
	VkPipelineLayout mPipelineLayout;

	// Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
	// While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
	// So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
	// Even though this adds a new dimension of planing ahead, it's a great opportunity for performance optimizations by the driver
	VkPipeline mPipeline;

	// The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
	// Like the pipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
	VkDescriptorSetLayout mDescriptorSetLayout;

	// The descriptor set stores the resources bound to the binding points in a shader
	// It connects the binding points of the different shaders with the buffers and images used for those bindings
	VkDescriptorSet mDescriptorSet;


	// Synchronization primitives
	// Synchronization is an important concept of Vulkan that OpenGL mostly hid away. Getting this right is crucial to using Vulkan.

	// Semaphores
	// Used to coordinate operations within the graphics queue and ensure correct command ordering
	VkSemaphore presentCompleteSemaphore;
	VkSemaphore renderCompleteSemaphore;

	// Fences
	// Used to check the completion of queue operations (e.g. command buffer execution)
	std::vector<VkFence> mWaitFences;

	VkTriangle() : VulkanBase(ENABLE_VALIDATION)
	{
		width = 800;
		height = 600;
		mZoom = -10.0f;
		title = "VkCore";
	}

	~VkTriangle()
	{
		// Clean up used Vulkan resources 
		// Note: Inherited destructor cleans up resources stored in base class
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, mPipeline, nullptr);

		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, mPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, mDescriptorSetLayout, nullptr);

		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, mVertices.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, mVertices.memory, nullptr);

		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, mIndices.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, mIndices.memory, nullptr);

		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, mUniformDataVS.memory, nullptr);

		vkDestroySemaphore(mVulkanDevice->mLogicalDevice, presentCompleteSemaphore, nullptr);
		vkDestroySemaphore(mVulkanDevice->mLogicalDevice, renderCompleteSemaphore, nullptr);

		for (auto& fence : mWaitFences)
		{
			vkDestroyFence(mVulkanDevice->mLogicalDevice, fence, nullptr);
		}
	}

	// This function is used to request a device memory type that supports all the property flags we request (e.g. device local, host visibile)
	// Upon success it will return the index of the memory type that fits our requestes memory properties
	// This is necessary as implementations can offer an arbitrary number of memory types with different
	// memory properties. 
	uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties)
	{
		// Iterate over all memory types available for the device used in this example
		for (uint32_t i = 0; i < mVulkanDevice->mMemoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((mVulkanDevice->mMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}
			typeBits >>= 1;
		}

		throw "Could not find a suitable memory type!";
	}

	// Create the Vulkan synchronization primitives used in this example
	void prepareSynchronizationPrimitives()
	{
		// Semaphores (Used for correct command ordering)
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;

		// Semaphore used to ensures that image presentation is complete before starting to submit again
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore));

		// Semaphore used to ensures that all commands submitted have been finished before submitting the image to the queue
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &renderCompleteSemaphore));

		// Fences (Used to check draw command buffer completion)
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		// Create in signaled state so we don't wait on first render of each command buffer
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		mWaitFences.resize(mDrawCmdBuffers.size());
		for (auto& fence : mWaitFences)
		{
			VK_CHECK_RESULT(vkCreateFence(mVulkanDevice->mLogicalDevice, &fenceCreateInfo, nullptr, &fence));
		}
	}

	// Get a new command buffer from the command pool
	// If begin is true, the command buffer is also started so we can start adding commands
	VkCommandBuffer getCommandBuffer(bool begin)
	{
		VkCommandBuffer cmdBuffer;

		VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
		cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufAllocateInfo.commandPool = mCmdPool;
		cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufAllocateInfo.commandBufferCount = 1;

		VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

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
	void flushCommandBuffer(VkCommandBuffer commandBuffer)
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
		VK_CHECK_RESULT(vkCreateFence(mVulkanDevice->mLogicalDevice, &fenceCreateInfo, nullptr, &fence));

		// Submit to the queue
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &submitInfo, fence));
		// Wait for the fence to signal that command buffer has finished executing
		VK_CHECK_RESULT(vkWaitForFences(mVulkanDevice->mLogicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

		vkDestroyFence(mVulkanDevice->mLogicalDevice, fence, nullptr);
		vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, mCmdPool, 1, &commandBuffer);
	}

	// Build separate command buffers for every framebuffer image
	// Unlike in OpenGL all rendering commands are recorded once into command buffers that are then resubmitted to the queue
	// This allows to generate work upfront and from multiple threads, one of the biggest advantages of Vulkan
	void buildCommandBuffers()
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

	void draw()
	{
		// Get next image in the swap chain (back/front buffer)
		VK_CHECK_RESULT(mSwapChain.acquireNextImage(presentCompleteSemaphore, &mCurrentBuffer));

		// Use a fence to wait until the command buffer has finished execution before using it again
		VK_CHECK_RESULT(vkWaitForFences(mVulkanDevice->mLogicalDevice, 1, &mWaitFences[mCurrentBuffer], VK_TRUE, UINT64_MAX));
		VK_CHECK_RESULT(vkResetFences(mVulkanDevice->mLogicalDevice, 1, &mWaitFences[mCurrentBuffer]));

		// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// The submit info structure specifices a command buffer queue submission batch
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
		submitInfo.pWaitSemaphores = &presentCompleteSemaphore;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
		submitInfo.waitSemaphoreCount = 1;												// One wait semaphore																				
		submitInfo.pSignalSemaphores = &renderCompleteSemaphore;						// Semaphore(s) to be signaled when command buffers have completed
		submitInfo.signalSemaphoreCount = 1;											// One signal semaphore
		submitInfo.pCommandBuffers = &mDrawCmdBuffers[mCurrentBuffer];					// Command buffers(s) to execute in this batch (submission)
		submitInfo.commandBufferCount = 1;												// One command buffer

																						// Submit to the graphics queue passing a wait fence
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &submitInfo, mWaitFences[mCurrentBuffer]));

		// Present the current buffer to the swap chain
		// Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
		// This ensures that the image is not presented to the windowing system until all commands have been submitted
		VK_CHECK_RESULT(mSwapChain.queuePresent(mQueue, mCurrentBuffer, renderCompleteSemaphore));
	}

	void updateUniformBuffers()
	{
		// Update matrices
		float aspect = (float)width / (float)height;
		mUboVS.projectionMatrix = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 256.0f);
		//mUboVS.projectionMatrix = glm::mat4();
		mUboVS.viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, mZoom));
		//mUboVS.viewMatrix = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
		//mUboVS.viewMatrix = glm::translate(mUboVS.viewMatrix, glm::vec3(0.0f, 0.0f, mZoom));
		//mUboVS.viewMatrix = glm::mat4();
		//std::cout.precision(4);
		//std::cout << mZoom << std::endl;

		//char szTmp[256] = {};
		//sprintf(szTmp, "zoom=%f", mZoom);
		//OutputDebugString(szTmp);
		mUboVS.modelMatrix = glm::mat4();
		//mUboVS.modelMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -1.7));
		//mUboVS.modelMatrix = glm::rotate(mUboVS.modelMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		mUboVS.modelMatrix = glm::rotate(mUboVS.modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		mUboVS.modelMatrix = glm::rotate(mUboVS.modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		mUboVS.modelMatrix = glm::rotate(mUboVS.modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		//mUboVS.modelMatrix = glm::scale(glm::mat4(), glm::vec3(1.0f / 400.0f, 1/300.0f, 0.0f));
		//mUboVS.modelMatrix = glm::mat4();

		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, mUniformDataVS.memory, 0, sizeof(mUboVS), 0, (void **)&pData));
		memcpy(pData, &mUboVS, sizeof(mUboVS));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, mUniformDataVS.memory);
	}

	// Prepare vertex and index buffers for an indexed triangle
	// Also uploads them to device local memory using staging and initializes vertex input and attribute binding to match the vertex shader
	void prepareVertices(bool useStagingBuffers)
	{
		// A note on memory management in Vulkan in general:
		//	This is a very complex topic and while it's fine for an example application to to small individual memory allocations that is not
		//	what should be done a real-world application, where you should allocate large chunkgs of memory at once isntead.

		// Setup vertices
		std::vector<Vertex> vertexBuffer =
		{
			{ { 0.9f,  1.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },
			{ { -1.0f,  1.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },
			{ { -1.0f, -1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { 0.9f, -1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f } }
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
		std::vector<uint32_t> indexBuffer = { 1, 0, 2, 3 };
		mIndices.count = static_cast<uint32_t>(indexBuffer.size());
		uint32_t indexBufferSize = mIndices.count * sizeof(uint32_t);

		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;

		void *data;

		if (useStagingBuffers)
		{
			// Static data like vertex and index buffer should be stored on the device memory 
			// for optimal (and fastest) access by the GPU
			//
			// To achieve this we use so-called "staging buffers" :
			// - Create a buffer that's visible to the host (and can be mapped)
			// - Copy the data to this buffer
			// - Create another buffer that's local on the device (VRAM) with the same size
			// - Copy the data from the host to the device using a command buffer
			// - Delete the host visible (staging) buffer
			// - Use the device local buffers for rendering

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
			VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer));
			vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, stagingBuffers.vertices.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			// Request a host visible memory type that can be used to copy our data do
			// Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &stagingBuffers.vertices.memory));
			// Map and copy
			VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, stagingBuffers.vertices.memory, 0, memAlloc.allocationSize, 0, &data));
			memcpy(data, vertexBuffer.data(), vertexBufferSize);
			vkUnmapMemory(mVulkanDevice->mLogicalDevice, stagingBuffers.vertices.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0));

			// Create a device local buffer to which the (host local) vertex data will be copied and which will be used for rendering
			vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &vertexBufferInfo, nullptr, &mVertices.buffer));
			vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, mVertices.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mVertices.memory));
			VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, mVertices.buffer, mVertices.memory, 0));

			// Index buffer
			VkBufferCreateInfo indexbufferInfo = {};
			indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			indexbufferInfo.size = indexBufferSize;
			indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			// Copy index data to a buffer visible to the host (staging buffer)
			VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &indexbufferInfo, nullptr, &stagingBuffers.indices.buffer));
			vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, stagingBuffers.indices.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &stagingBuffers.indices.memory));
			VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data));
			memcpy(data, indexBuffer.data(), indexBufferSize);
			vkUnmapMemory(mVulkanDevice->mLogicalDevice, stagingBuffers.indices.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0));

			// Create destination buffer with device only visibility
			indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &indexbufferInfo, nullptr, &mIndices.buffer));
			vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, mIndices.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mIndices.memory));
			VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, mIndices.buffer, mIndices.memory, 0));

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
			vkDestroyBuffer(mVulkanDevice->mLogicalDevice, stagingBuffers.vertices.buffer, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalDevice, stagingBuffers.vertices.memory, nullptr);
			vkDestroyBuffer(mVulkanDevice->mLogicalDevice, stagingBuffers.indices.buffer, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalDevice, stagingBuffers.indices.memory, nullptr);
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
			VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &vertexBufferInfo, nullptr, &mVertices.buffer));
			vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, mVertices.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mVertices.memory));
			VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, mVertices.memory, 0, memAlloc.allocationSize, 0, &data));
			memcpy(data, vertexBuffer.data(), vertexBufferSize);
			vkUnmapMemory(mVulkanDevice->mLogicalDevice, mVertices.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, mVertices.buffer, mVertices.memory, 0));

			// Index buffer
			VkBufferCreateInfo indexbufferInfo = {};
			indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			indexbufferInfo.size = indexBufferSize;
			indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

			// Copy index data to a buffer visible to the host
			VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &indexbufferInfo, nullptr, &mIndices.buffer));
			vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, mIndices.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mIndices.memory));
			VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, mIndices.memory, 0, indexBufferSize, 0, &data));
			memcpy(data, indexBuffer.data(), indexBufferSize);
			vkUnmapMemory(mVulkanDevice->mLogicalDevice, mIndices.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, mIndices.buffer, mIndices.memory, 0));
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

	void setupDescriptorPool()
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

		VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
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

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));

		// Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
		// In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
		pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.pNext = nullptr;
		pPipelineLayoutCreateInfo.setLayoutCount = 1;
		pPipelineLayoutCreateInfo.pSetLayouts = &mDescriptorSetLayout;

		VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
	}

	void setupDescriptorSet()
	{
		// Allocate a new descriptor set from the global descriptor pool
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &mDescriptorSetLayout;

		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &mDescriptorSet));

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

		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, 1, &writeDescriptorSet, 0, nullptr);
	}

	// Create the depth (and stencil) buffer attachments used by our framebuffers
	// Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
	void setupDepthStencil()
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
		VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalDevice, &image, nullptr, &depthStencil.image));

		// Allocate memory for the image (device local) and bind it to our image
		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(mVulkanDevice->mLogicalDevice, depthStencil.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &depthStencil.mem));
		VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalDevice, depthStencil.image, depthStencil.mem, 0));

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
		VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalDevice, &depthStencilView, nullptr, &depthStencil.view));
	}

	// Create a frame buffer for each swap chain image
	// Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
	void setupFrameBuffer()
	{
		// Create a frame buffer for every image in the swapchain
		mFrameBuffers.resize(mSwapChain.mImageCount);
		for (size_t i = 0; i < mFrameBuffers.size(); i++)
		{
			std::array<VkImageView, 2> attachments;
			attachments[0] = mSwapChain.buffers[i].view;									// Color attachment is the view of the swapchain image			
			attachments[1] = depthStencil.view;											// Depth/Stencil attachment is the same for all frame buffers			

			VkFramebufferCreateInfo frameBufferCreateInfo = {};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			// All frame buffers use the same renderpass setup
			frameBufferCreateInfo.renderPass = mRenderPass;
			frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			frameBufferCreateInfo.pAttachments = attachments.data();
			frameBufferCreateInfo.width = width;
			frameBufferCreateInfo.height = height;
			frameBufferCreateInfo.layers = 1;
			// Create the framebuffer
			VK_CHECK_RESULT(vkCreateFramebuffer(mVulkanDevice->mLogicalDevice, &frameBufferCreateInfo, nullptr, &mFrameBuffers[i]));
		}
	}

	// Render pass setup
	// Render passes are a new concept in Vulkan. They describe the attachments used during rendering and may contain multiple subpasses with attachment dependencies 
	// This allows the driver to know up-front what the rendering will look like and is a good opportunity to optimize especially on tile-based renderers (with multiple subpasses)
	// Using sub pass dependencies also adds implicit layout transitions for the attachment used, so we don't need to add explicit image memory barriers to transform them
	// Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
	void setupRenderPass()
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

		VK_CHECK_RESULT(vkCreateRenderPass(mVulkanDevice->mLogicalDevice, &renderPassInfo, nullptr, &mRenderPass));
	}

	void preparePipelines()
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
		shaderStages[0] = loadShader(getAssetPath() + "shaders/triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeline));
	}

	void prepareUniformBuffers()
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
		VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalDevice, &bufferInfo, nullptr, &mUniformDataVS.buffer));
		// Get memory requirements including size, alignment and memory type 
		vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, &memReqs);
		allocInfo.allocationSize = memReqs.size;
		// Get the memory type index that supports host visibile memory access
		// Most implementations offer multiple memory types and selecting the correct one to allocate memory from is crucial
		// We also want the buffer to be host coherent so we don't have to flush (or sync after every update.
		// Note: This may affect performance so you might not want to do this in a real world application that updates buffers on a regular base
		allocInfo.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		// Allocate memory for the uniform buffer
		VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &allocInfo, nullptr, &(mUniformDataVS.memory)));
		// Bind memory to buffer
		VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, mUniformDataVS.memory, 0));

		// Store information in the uniform's descriptor that is used by the descriptor set
		mUniformDataVS.descriptor.buffer = mUniformDataVS.buffer;
		mUniformDataVS.descriptor.offset = 0;
		mUniformDataVS.descriptor.range = sizeof(mUboVS);

		updateUniformBuffers();
	}

	void prepare()
	{
		VulkanBase::prepare();
		prepareSynchronizationPrimitives();
		prepareVertices(USE_STAGING);
		prepareUniformBuffers();
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
		// This function is called by the base example class each time the view is changed by user input
		updateUniformBuffers();
	}
};
