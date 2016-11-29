#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <random>
#include "define.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "VulkanBase.h"


#define ENABLE_VALIDATION false
#if defined(__ANDROID__)
// Lower particle count on Android for performance reasons
#define PARTICLE_COUNT 128 * 1024
#else
#define PARTICLE_COUNT 256 * 1024
#endif

class VkComputeParticles : public VulkanBase
{
public:
	float timer = 0.0f;
	float animStart = 20.0f;
	bool animate = true;

	struct {
		vkTools::VulkanTexture particle;
		vkTools::VulkanTexture gradient;
	} textures;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	// Resources for the graphics part of the example
	struct {
		VkDescriptorSetLayout descriptorSetLayout;	// Particle system rendering shader binding layout
		VkDescriptorSet descriptorSet;				// Particle system rendering shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of the graphics pipeline
		VkPipeline pipeline;						// Particle rendering pipeline
	} graphics;

	// Resources for the compute part of the example
	struct {
		vk::Buffer storageBuffer;					// (Shader) storage buffer object containing the particles
		vk::Buffer uniformBuffer;					// Uniform buffer object containing particle system parameters
		VkQueue queue;								// Separate queue for compute commands (queue family may differ from the one used for graphics)
		VkCommandPool commandPool;					// Use a separate command pool (queue family may differ from the one used for graphics)
		VkCommandBuffer commandBuffer;				// Command buffer storing the dispatch commands and barriers
		VkFence fence;								// Synchronization fence to avoid rewriting compute CB if still in use
		VkDescriptorSetLayout descriptorSetLayout;	// Compute shader binding layout
		VkDescriptorSet descriptorSet;				// Compute shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of the compute pipeline
		VkPipeline pipeline;						// Compute pipeline for updating particle positions
		struct computeUBO {							// Compute shader uniform block object
			float deltaT;							//		Frame delta time
			float destX;							//		x position of the attractor
			float destY;							//		y position of the attractor
			int32_t particleCount = PARTICLE_COUNT;
		} ubo;
	} compute;

	// SSBO particle declaration
	struct Particle {
		glm::vec2 pos;								// Particle position
		glm::vec2 vel;								// Particle velocity
		glm::vec4 gradientPos;						// Texture coordiantes for the gradient ramp map
	};

	VkComputeParticles() : VulkanBase(ENABLE_VALIDATION)
	{
		enableTextOverlay = true;
		title = "Vulkan Example - Compute shader particle system";
	}

	~VkComputeParticles()
	{
		// Graphics
		vkDestroyPipeline(mDevice, graphics.pipeline, nullptr);
		vkDestroyPipelineLayout(mDevice, graphics.pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mDevice, graphics.descriptorSetLayout, nullptr);

		// Compute
		compute.storageBuffer.destroy();
		compute.uniformBuffer.destroy();
		vkDestroyPipelineLayout(mDevice, compute.pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mDevice, compute.descriptorSetLayout, nullptr);
		vkDestroyPipeline(mDevice, compute.pipeline, nullptr);
		vkDestroyFence(mDevice, compute.fence, nullptr);
		vkDestroyCommandPool(mDevice, compute.commandPool, nullptr);

		textureLoader->destroyTexture(textures.particle);
		textureLoader->destroyTexture(textures.gradient);
	}

	void loadTextures()
	{
		textureLoader->loadTexture(getAssetPath() + "textures/particle01_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, &textures.particle, false);
		textureLoader->loadTexture(getAssetPath() + "textures/particle_gradient_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, &textures.gradient, false);
	}

	void buildCommandBuffers()
	{
		// Destroy command buffers if already present
		if (!checkCommandBuffers())
		{
			destroyCommandBuffers();
			createCommandBuffers();
		}

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

			// Draw the particle system using the update vertex buffer

			vkCmdBeginRenderPass(mDrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vkTools::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vkTools::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1, &graphics.descriptorSet, 0, NULL);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &compute.storageBuffer.buffer, offsets);
			vkCmdDraw(mDrawCmdBuffers[i], PARTICLE_COUNT, 1, 0, 0);

			vkCmdEndRenderPass(mDrawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
		}

	}

	void buildComputeCommandBuffer()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

		VK_CHECK_RESULT(vkBeginCommandBuffer(compute.commandBuffer, &cmdBufInfo));

		// Compute particle movement

		// Add memory barrier to ensure that the (graphics) vertex shader has fetched attributes before compute starts to write to the buffer
		VkBufferMemoryBarrier bufferBarrier = vkTools::initializers::bufferMemoryBarrier();
		bufferBarrier.buffer = compute.storageBuffer.buffer;
		bufferBarrier.size = compute.storageBuffer.descriptor.range;
		bufferBarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;						// Vertex shader invocations have finished reading from the buffer
		bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;								// Compute shader wants to write to the buffer
																								// Compute and graphics queue may have different queue families (see VulkanDevice::createLogicalDevice)
																								// For the barrier to work across different queues, we need to set their family indices
		bufferBarrier.srcQueueFamilyIndex = mVulkanDevice->queueFamilyIndices.graphics;			// Required as compute and graphics queue may have different families
		bufferBarrier.dstQueueFamilyIndex = mVulkanDevice->queueFamilyIndices.compute;			// Required as compute and graphics queue may have different families

		vkCmdPipelineBarrier(
			compute.commandBuffer,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);

		vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipeline);
		vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayout, 0, 1, &compute.descriptorSet, 0, 0);

		// Dispatch the compute job
		vkCmdDispatch(compute.commandBuffer, PARTICLE_COUNT / 16, 1, 1);

		// Add memory barrier to ensure that compute shader has finished writing to the buffer
		// Without this the (rendering) vertex shader may display incomplete results (partial data from last frame) 
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;								// Compute shader has finished writes to the buffer
		bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;						// Vertex shader invocations want to read from the buffer
		bufferBarrier.buffer = compute.storageBuffer.buffer;
		bufferBarrier.size = compute.storageBuffer.descriptor.range;
		// Compute and graphics queue may have different queue families (see VulkanDevice::createLogicalDevice)
		// For the barrier to work across different queues, we need to set their family indices
		bufferBarrier.srcQueueFamilyIndex = mVulkanDevice->queueFamilyIndices.compute;			// Required as compute and graphics queue may have different families
		bufferBarrier.dstQueueFamilyIndex = mVulkanDevice->queueFamilyIndices.graphics;			// Required as compute and graphics queue may have different families

		vkCmdPipelineBarrier(
			compute.commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);

		vkEndCommandBuffer(compute.commandBuffer);
	}

	// Setup and fill the compute shader storage buffers containing the particles
	void prepareStorageBuffers()
	{
		std::mt19937 rGenerator;
		std::uniform_real_distribution<float> rDistribution(-1.0f, 1.0f);

		// Initial particle positions
		std::vector<Particle> particleBuffer(PARTICLE_COUNT);
		for (auto& particle : particleBuffer)
		{
			particle.pos = glm::vec2(rDistribution(rGenerator), rDistribution(rGenerator));
			particle.vel = glm::vec2(0.0f);
			particle.gradientPos.x = particle.pos.x / 2.0f;
		}

		VkDeviceSize storageBufferSize = particleBuffer.size() * sizeof(Particle);

		// Staging
		// SSBO won't be changed on the host after upload so copy to device local memory 

		vk::Buffer stagingBuffer;

		mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stagingBuffer,
			storageBufferSize,
			particleBuffer.data());

		mVulkanDevice->createBuffer(
			// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&compute.storageBuffer,
			storageBufferSize);

		// Copy to staging buffer
		VkCommandBuffer copyCmd = VulkanBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRegion = {};
		copyRegion.size = storageBufferSize;
		vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, compute.storageBuffer.buffer, 1, &copyRegion);
		VulkanBase::flushCommandBuffer(copyCmd, mQueue, true);

		stagingBuffer.destroy();

		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
			vkTools::initializers::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				sizeof(Particle),
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(2);
		// Location 0 : Position
		vertices.attributeDescriptions[0] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32_SFLOAT,
				offsetof(Particle, pos));
		// Location 1 : Gradient position
		vertices.attributeDescriptions[1] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32B32A32_SFLOAT,
				offsetof(Particle, gradientPos));

		// Assign to vertex buffer
		vertices.inputState = vkTools::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
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
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		// Binding 0 : Particle color map
		setLayoutBindings.push_back(vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0));
		// Binding 1 : Particle gradient ramp
		setLayoutBindings.push_back(vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			1));

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vkTools::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				static_cast<uint32_t>(setLayoutBindings.size()));

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mDevice, &descriptorLayout, nullptr, &graphics.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
			vkTools::initializers::pipelineLayoutCreateInfo(
				&graphics.descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, nullptr, &graphics.pipelineLayout));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo =
			vkTools::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&graphics.descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(mDevice, &allocInfo, &graphics.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		// Binding 0 : Particle color map
		writeDescriptorSets.push_back(vkTools::initializers::writeDescriptorSet(
			graphics.descriptorSet,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			0,
			&textures.particle.descriptor));
		// Binding 1 : Particle gradient ramp
		writeDescriptorSets.push_back(vkTools::initializers::writeDescriptorSet(
			graphics.descriptorSet,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			&textures.gradient.descriptor));

		vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vkTools::initializers::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
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
				VK_FALSE,
				VK_FALSE,
				VK_COMPARE_OP_ALWAYS);

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

		// Rendering pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/computeparticles/particle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/computeparticles/particle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vkTools::initializers::pipelineCreateInfo(
				graphics.pipelineLayout,
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
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.renderPass = mRenderPass;

		// Additive blending
		blendAttachmentState.colorWriteMask = 0xF;
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &graphics.pipeline));
	}

	void prepareCompute()
	{
		// Create a compute capable device queue
		// The VulkanDevice::createLogicalDevice functions finds a compute capable queue and prefers queue families that only support compute
		// Depending on the implementation this may result in different queue family indices for graphics and computes,
		// requiring proper synchronization (see the memory barriers in buildComputeCommandBuffer)
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.pNext = NULL;
		queueCreateInfo.queueFamilyIndex = mVulkanDevice->queueFamilyIndices.compute;
		queueCreateInfo.queueCount = 1;
		vkGetDeviceQueue(mDevice, mVulkanDevice->queueFamilyIndices.compute, 0, &compute.queue);

		// Create compute pipeline
		// Compute pipelines are created separate from graphics pipelines even if they use the same queue (family index)

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Particle position storage buffer
			vkTools::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				VK_SHADER_STAGE_COMPUTE_BIT,
				0),
			// Binding 1 : Uniform buffer
			vkTools::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_COMPUTE_BIT,
				1),
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vkTools::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				static_cast<uint32_t>(setLayoutBindings.size()));

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mDevice, &descriptorLayout, nullptr, &compute.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vkTools::initializers::pipelineLayoutCreateInfo(
				&compute.descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(mDevice, &pPipelineLayoutCreateInfo, nullptr, &compute.pipelineLayout));

		VkDescriptorSetAllocateInfo allocInfo =
			vkTools::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&compute.descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(mDevice, &allocInfo, &compute.descriptorSet));

		std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets =
		{
			// Binding 0 : Particle position storage buffer
			vkTools::initializers::writeDescriptorSet(
				compute.descriptorSet,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				0,
				&compute.storageBuffer.descriptor),
			// Binding 1 : Uniform buffer
			vkTools::initializers::writeDescriptorSet(
				compute.descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
				&compute.uniformBuffer.descriptor)
		};

		vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, NULL);

		// Create pipeline		
		VkComputePipelineCreateInfo computePipelineCreateInfo = vkTools::initializers::computePipelineCreateInfo(compute.pipelineLayout, 0);
		computePipelineCreateInfo.stage = loadShader(getAssetPath() + "shaders/computeparticles/particle.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(mDevice, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &compute.pipeline));

		// Separate command pool as queue family for compute may be different than graphics
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = mVulkanDevice->queueFamilyIndices.compute;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(mDevice, &cmdPoolInfo, nullptr, &compute.commandPool));

		// Create a command buffer for compute operations
		VkCommandBufferAllocateInfo cmdBufAllocateInfo =
			vkTools::initializers::commandBufferAllocateInfo(
				compute.commandPool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				1);

		VK_CHECK_RESULT(vkAllocateCommandBuffers(mDevice, &cmdBufAllocateInfo, &compute.commandBuffer));

		// Fence for compute CB sync
		VkFenceCreateInfo fenceCreateInfo = vkTools::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
		VK_CHECK_RESULT(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &compute.fence));

		// Build a single command buffer containing the compute dispatch commands
		buildComputeCommandBuffer();
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Compute shader uniform buffer block
		mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&compute.uniformBuffer,
			sizeof(compute.ubo));

		// Map for host access
		VK_CHECK_RESULT(compute.uniformBuffer.map());

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		compute.ubo.deltaT = frameTimer * 2.5f;
		if (animate)
		{
			compute.ubo.destX = sin(glm::radians(timer * 360.0f)) * 0.75f;
			compute.ubo.destY = 0.0f;
		}
		else
		{
			float normalizedMx = (mousePos.x - static_cast<float>(width / 2)) / static_cast<float>(width / 2);
			float normalizedMy = (mousePos.y - static_cast<float>(height / 2)) / static_cast<float>(height / 2);
			compute.ubo.destX = normalizedMx;
			compute.ubo.destY = normalizedMy;
		}

		memcpy(compute.uniformBuffer.mapped, &compute.ubo, sizeof(compute.ubo));
	}

	void draw()
	{
		// Submit graphics commands
		VulkanBase::prepareFrame();

		mSubmitInfo.commandBufferCount = 1;
		mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[mCurrentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		VulkanBase::submitFrame();

		// Submit compute commands
		vkWaitForFences(mDevice, 1, &compute.fence, VK_TRUE, UINT64_MAX);
		vkResetFences(mDevice, 1, &compute.fence);

		VkSubmitInfo computeSubmitInfo = vkTools::initializers::submitInfo();
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &compute.commandBuffer;

		VK_CHECK_RESULT(vkQueueSubmit(compute.queue, 1, &computeSubmitInfo, compute.fence));
	}

	void prepare()
	{
		VulkanBase::prepare();
		loadTextures();
		prepareStorageBuffers();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		prepareCompute();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();

		if (animate)
		{
			if (animStart > 0.0f)
			{
				animStart -= frameTimer * 5.0f;
			}
			else if (animStart <= 0.0f)
			{
				timer += frameTimer * 0.04f;
				if (timer > 1.f)
					timer = 0.f;
			}
		}

		updateUniformBuffers();
	}

	void toggleAnimation()
	{
		animate = !animate;
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case KEY_A:
		case GAMEPAD_BUTTON_A:
			toggleAnimation();
			break;
		}
	}
};
