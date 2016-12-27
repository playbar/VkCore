#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h> 
#include <vector>
#include <random>

#include "define.h"

#include <vulkan/vulkan.h>
#include "VulkanBase.h"

#define INSTANCE_BUFFER_BIND_ID 1
#define ENABLE_VALIDATION false
#define INSTANCE_COUNT 2048


class VkInstancing : public VulkanBase
{
	// Vertex layout for this example
	std::vector<vkMeshLoader::VertexLayout> vertexLayout =
	{
		vkMeshLoader::VERTEX_LAYOUT_POSITION,
		vkMeshLoader::VERTEX_LAYOUT_NORMAL,
		vkMeshLoader::VERTEX_LAYOUT_UV,
		vkMeshLoader::VERTEX_LAYOUT_COLOR
	};

public:
	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct {
		vkMeshLoader::MeshBuffer example;
	} meshes;

	struct {
		vkTools::VulkanTexture colorMap;
	} textures;

	// Per-instance data block
	struct InstanceData {
		Vector3 pos;
		Vector3 rot;
		float scale;
		uint32_t texIndex;
	};

	// Contains the instanced data
	struct {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		size_t size = 0;
		VkDescriptorBufferInfo descriptor;
	} instanceBuffer;

	struct {
		Matrix projection;
		Matrix view;
		float time = 0.0f;
	} uboVS;

	struct {
		vkTools::UniformData vsScene;
	} uniformData;

	struct {
		VkPipeline solid;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	VkInstancing() : VulkanBase(ENABLE_VALIDATION)
	{
		mZoom = -12.0f;
		rotationSpeed = 0.25f;
		mEnableTextOverlay = true;
		title = "Vulkan Example - Instanced mesh rendering";
		srand(time(NULL));
	}

	~VkInstancing()
	{
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.solid, nullptr);
		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayout, nullptr);
		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, instanceBuffer.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, instanceBuffer.memory, nullptr);
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.example);
		vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.vsScene);
		textureLoader->destroyTexture(textures.colorMap);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vkTools::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = mRenderPass;
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

			VkViewport viewport = vkTools::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vkTools::rect2D(width, height, 0, 0);
			vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solid);

			VkDeviceSize offsets[1] = { 0 };
			// Binding point 0 : Mesh vertex buffer
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.example.vertices.buf, offsets);
			// Binding point 1 : Instance data buffer
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer.buffer, offsets);

			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.example.indices.buf, 0, VK_INDEX_TYPE_UINT32);

			// Render instances
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.example.indexCount, INSTANCE_COUNT, 0, 0, 0);

			vkCmdEndRenderPass(mDrawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
		}
	}

	void loadMeshes()
	{
		loadMesh(getAssetPath() + "models/rock01.dae", &meshes.example, vertexLayout, 0.1f);
	}

	void loadTextures()
	{
		textureLoader->loadTextureArray(
			getAssetPath() + "textures/texturearray_rocks_bc3.ktx",
			VK_FORMAT_BC3_UNORM_BLOCK,
			&textures.colorMap);
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(2);

		// Mesh vertex buffer (description) at binding point 0
		vertices.bindingDescriptions[0] =
			vkTools::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				vkMeshLoader::vertexSize(vertexLayout),
				// Input rate for the data passed to shader
				// Step for each vertex rendered
				VK_VERTEX_INPUT_RATE_VERTEX);

		vertices.bindingDescriptions[1] =
			vkTools::vertexInputBindingDescription(
				INSTANCE_BUFFER_BIND_ID,
				sizeof(InstanceData),
				// Input rate for the data passed to shader
				// Step for each instance rendered
				VK_VERTEX_INPUT_RATE_INSTANCE);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.clear();

		// Per-Vertex attributes
		// Location 0 : Position
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				0)
		);
		// Location 1 : Normal
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 3)
		);
		// Location 2 : Texture coordinates
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				VK_FORMAT_R32G32_SFLOAT,
				sizeof(float) * 6)
		);
		// Location 3 : Color
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				3,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 8)
		);

		// Instanced attributes
		// Location 4 : Position
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID,
				5,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 3)
		);
		// Location 5 : Rotation
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID,
				4,
				VK_FORMAT_R32G32B32_SFLOAT,
				0)
		);
		// Location 6 : Scale
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID,
				6,
				VK_FORMAT_R32_SFLOAT,
				sizeof(float) * 6)
		);
		// Location 7 : Texture array layer index
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID,
				7,
				VK_FORMAT_R32_SINT,
				sizeof(float) * 7)
		);


		vertices.inputState = vkTools::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		// Example uses one ubo 
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vkTools::descriptorPoolCreateInfo(
				poolSizes.size(),
				poolSizes.data(),
				2);

		VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_VERTEX_BIT,
				0),
			// Binding 1 : Fragment shader combined sampler
			vkTools::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				1),
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vkTools::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				setLayoutBindings.size());

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vkTools::pipelineLayoutCreateInfo(
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo =
			vkTools::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSet));

		VkDescriptorImageInfo texDescriptor =
			vkTools::descriptorImageInfo(
				textures.colorMap.sampler,
				textures.colorMap.view,
				VK_IMAGE_LAYOUT_GENERAL);

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformData.vsScene.descriptor),
			// Binding 1 : Color map 
			vkTools::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&texDescriptor)
		};

		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vkTools::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				0,
				VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vkTools::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_BACK_BIT,
				VK_FRONT_FACE_CLOCKWISE,
				0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vkTools::pipelineColorBlendAttachmentState(
				0xf,
				VK_FALSE);

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vkTools::pipelineColorBlendStateCreateInfo(
				1,
				&blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vkTools::pipelineDepthStencilStateCreateInfo(
				VK_TRUE,
				VK_TRUE,
				VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
			vkTools::pipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vkTools::pipelineMultisampleStateCreateInfo(
				VK_SAMPLE_COUNT_1_BIT,
				0);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vkTools::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				dynamicStateEnables.size(),
				0);

		// Instacing pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/instancing/instancing.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/instancing/instancing.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vkTools::pipelineCreateInfo(
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

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid));
	}

	float rnd(float range)
	{
		return range * (rand() / double(RAND_MAX));
	}

	void prepareInstanceData()
	{
		std::vector<InstanceData> instanceData;
		instanceData.resize(INSTANCE_COUNT);

		std::mt19937 rndGenerator(time(NULL));
		std::uniform_real_distribution<double> uniformDist(0.0, 1.0);

		for (auto i = 0; i < INSTANCE_COUNT; i++)
		{
			instanceData[i].rot = Vector3(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator));
			float theta = 2 * M_PI * uniformDist(rndGenerator);
			float phi = acos(1 - 2 * uniformDist(rndGenerator));
			instanceData[i].pos = Vector3(sin(phi) * cos(theta), sin(theta) * uniformDist(rndGenerator) / 1500.0f, cos(phi)) * 7.5f;
			instanceData[i].scale = 1.0f + uniformDist(rndGenerator) * 2.0f;
			instanceData[i].texIndex = rnd(textures.colorMap.layerCount);
		}

		instanceBuffer.size = instanceData.size() * sizeof(InstanceData);

		// Staging
		// Instanced data is static, copy to device local memory 
		// This results in better performance

		struct {
			VkDeviceMemory memory;
			VkBuffer buffer;
		} stagingBuffer;

		VulkanBase::createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			instanceBuffer.size,
			instanceData.data(),
			&stagingBuffer.buffer,
			&stagingBuffer.memory);

		VulkanBase::createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			instanceBuffer.size,
			nullptr,
			&instanceBuffer.buffer,
			&instanceBuffer.memory);

		// Copy to staging buffer
		VkCommandBuffer copyCmd = VulkanBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};
		copyRegion.size = instanceBuffer.size;
		vkCmdCopyBuffer(
			copyCmd,
			stagingBuffer.buffer,
			instanceBuffer.buffer,
			1,
			&copyRegion);

		VulkanBase::flushCommandBuffer(copyCmd, mQueue, true);

		instanceBuffer.descriptor.range = instanceBuffer.size;
		instanceBuffer.descriptor.buffer = instanceBuffer.buffer;
		instanceBuffer.descriptor.offset = 0;

		// Destroy staging resources
		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, stagingBuffer.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, stagingBuffer.memory, nullptr);
	}

	void prepareUniformBuffers()
	{
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uboVS),
			nullptr,
			&uniformData.vsScene.buffer,
			&uniformData.vsScene.memory,
			&uniformData.vsScene.descriptor);

		// Map for host access
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsScene.memory, 0, sizeof(uboVS), 0, (void **)&uniformData.vsScene.mapped));

		updateUniformBuffer(true);
	}

	void updateUniformBuffer(bool viewChanged)
	{
		if (viewChanged)
		{
			Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), (float)width / (float)height, 0.001f, 256.0f, &uboVS.projection);
			Matrix::createTranslation(cameraPos + Vector3(0.0f, 0.0f, mZoom), &uboVS.view);
			uboVS.view.rotateX(MATH_DEG_TO_RAD(mRotation.x));
			uboVS.view.rotateY(MATH_DEG_TO_RAD(mRotation.y));
			uboVS.view.rotateZ(MATH_DEG_TO_RAD(mRotation.z));
			//uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.001f, 256.0f);
			//uboVS.view = glm::translate(glm::mat4(), cameraPos + glm::vec3(0.0f, 0.0f, mZoom));
			//uboVS.view = glm::rotate(uboVS.view, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			//uboVS.view = glm::rotate(uboVS.view, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
			//uboVS.view = glm::rotate(uboVS.view, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		}

		if (!paused)
		{
			uboVS.time += frameTimer * 0.05f;
		}

		memcpy(uniformData.vsScene.mapped, &uboVS, sizeof(uboVS));
	}

	void draw()
	{
		VulkanBase::prepareFrame();

		// Command buffer to be sumitted to the queue
		mSubmitInfo.commandBufferCount = 1;
		mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[gSwapChain.mCurrentBuffer];

		// Submit to queue
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		VulkanBase::submitFrame();
	}

	void prepare()
	{
		VulkanBase::prepare();
		loadTextures();
		loadMeshes();
		prepareInstanceData();
		setupVertexDescriptions();
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
		{
			return;
		}
		draw();
		if (!paused)
		{
			updateUniformBuffer(false);
		}
	}

	virtual void viewChanged()
	{
		updateUniformBuffer(true);
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
		textOverlay->addText("Rendering " + std::to_string(INSTANCE_COUNT) + " instances", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
	}
};
