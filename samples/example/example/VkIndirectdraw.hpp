#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h> 
#include <vector>
#include <random>
#include "define.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "VulkanBase.h"
#include "vulkanbuffer.hpp"

#define INSTANCE_BUFFER_BIND_ID 1
#define ENABLE_VALIDATION false

// Number of instances per object
#if defined(__ANDROID__)
#define OBJECT_INSTANCE_COUNT 1024
// Circular range of plant distribution
#define PLANT_RADIUS 20.0f
#else
#define OBJECT_INSTANCE_COUNT 2048
// Circular range of plant distribution
#define PLANT_RADIUS 25.0f
#endif


class VkIndirectdraw : public VulkanBase
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
		vkMeshLoader::MeshBuffer plants;
		vkMeshLoader::MeshBuffer ground;
		vkMeshLoader::MeshBuffer skysphere;
	} meshes;

	struct {
		vkTools::VulkanTexture plants;
		vkTools::VulkanTexture ground;
	} textures;

	// Per-instance data block
	struct InstanceData {
		Vector3 pos;
		Vector3 rot;
		float scale;
		uint32_t texIndex;
	};

	// Contains the instanced data
	vk::Buffer instanceBuffer;
	// Contains the indirect drawing commands
	vk::Buffer indirectCommandsBuffer;
	uint32_t indirectDrawCount;

	struct {
		Matrix projection;
		Matrix view;
	} uboVS;

	struct {
		vk::Buffer scene;
	} uniformData;

	struct {
		VkPipeline plants;
		VkPipeline ground;
		VkPipeline skysphere;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	VkSampler samplerRepeat;

	uint32_t objectCount = 0;

	// Store the indirect draw commands containing index offsets and instance count per object
	std::vector<VkDrawIndexedIndirectCommand> indirectCommands;

	VkIndirectdraw() : VulkanBase(ENABLE_VALIDATION)
	{
		mEnableTextOverlay = true;
		title = "Vulkan Example - Indirect rendering";
		mCamera.type = VkCamera::CameraType::firstperson;
		mCamera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
		mCamera.setRotation(Vector3(-12.0f, 159.0f, 0.0f));
		mCamera.setTranslation(Vector3(0.4f, 1.25f, 0.0f));
		mCamera.movementSpeed = 5.0f;
	}

	~VkIndirectdraw()
	{
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.plants, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.ground, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.skysphere, nullptr);
		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayout, nullptr);
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.plants);
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.ground);
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.skysphere);
		textureLoader->destroyTexture(textures.plants);
		textureLoader->destroyTexture(textures.ground);
		instanceBuffer.destroy();
		indirectCommandsBuffer.destroy();
		uniformData.scene.destroy();
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
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.18f, 0.27f, 0.5f, 0.0f } };
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

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			// Plants
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.plants);
			// Binding point 0 : Mesh vertex buffer
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.plants.vertices.buf, offsets);
			// Binding point 1 : Instance data buffer
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer.buffer, offsets);

			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.plants.indices.buf, 0, VK_INDEX_TYPE_UINT32);

			// If the multi draw feature is supported:
			// One draw call for an arbitrary number of ojects
			// Index offsets and instance count are taken from the indirect buffer
			if (mVulkanDevice->mFeatures.multiDrawIndirect)
			{
				vkCmdDrawIndexedIndirect(mDrawCmdBuffers[i], indirectCommandsBuffer.buffer, 0, indirectDrawCount, sizeof(VkDrawIndexedIndirectCommand));
			}
			else
			{
				// If multi draw is not available, we must issue separate draw commands
				for (auto j = 0; j < indirectCommands.size(); j++)
				{
					vkCmdDrawIndexedIndirect(mDrawCmdBuffers[i], indirectCommandsBuffer.buffer, j * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
				}
			}

			// Ground
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.ground);
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.ground.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.ground.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.ground.indexCount, 1, 0, 0, 0);
			// Skysphere
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skysphere);
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.skysphere.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.skysphere.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.skysphere.indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(mDrawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		loadMesh(getAssetPath() + "models/plants.dae", &meshes.plants, vertexLayout, 0.0025f);
		loadMesh(getAssetPath() + "models/plane_circle.dae", &meshes.ground, vertexLayout, PLANT_RADIUS + 1.0f);
		loadMesh(getAssetPath() + "models/skysphere.dae", &meshes.skysphere, vertexLayout, 512.0f / 10.0f);

		textureLoader->loadTextureArray(getAssetPath() + "textures/texturearray_plants_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &textures.plants);
		textureLoader->loadTexture(getAssetPath() + "textures/ground_dry_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &textures.ground);
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
		// Location 4: Position
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, pos))
		);
		// Location 5: Rotation
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID, 5, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, rot))
		);
		// Location 6: Scale
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID, 6, VK_FORMAT_R32_SFLOAT, offsetof(InstanceData, scale))
		);
		// Location 7: Texture array layer index
		vertices.attributeDescriptions.push_back(
			vkTools::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID, 7, VK_FORMAT_R32_SINT, offsetof(InstanceData, texIndex))
		);

		vertices.inputState = vkTools::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		// Example uses one ubo 
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2),
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vkTools::descriptorPoolCreateInfo(
				static_cast<uint32_t>(poolSizes.size()),
				poolSizes.data(),
				2);

		VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			// Binding 0: Vertex shader uniform buffer
			vkTools::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_VERTEX_BIT,
				0),
			// Binding 1: Fragment shader combined sampler (plants texture array)
			vkTools::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				1),
			// Binding 1: Fragment shader combined sampler (ground texture)
			vkTools::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				2),
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vkTools::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				static_cast<uint32_t>(setLayoutBindings.size()));

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

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0: Vertex shader uniform buffer
			vkTools::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformData.scene.descriptor),
			// Binding 1: Plants texture array combined 
			vkTools::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&textures.plants.descriptor),
			// Binding 2: Ground texture combined 
			vkTools::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				2,
				&textures.ground.descriptor)
		};

		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
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
				VK_CULL_MODE_NONE,
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
				static_cast<uint32_t>(dynamicStateEnables.size()),
				0);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vkTools::pipelineCreateInfo(
				pipelineLayout,
				mRenderPass,
				0);

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

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

		// Indirect (and instanced) pipeline for the plants
		shaderStages[0] = loadShader(getAssetPath() + "shaders/indirectdraw/indirectdraw.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/indirectdraw/indirectdraw.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.plants));

		// Ground
		shaderStages[0] = loadShader(getAssetPath() + "shaders/indirectdraw/ground.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/indirectdraw/ground.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		//rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.ground));

		// Skysphere
		shaderStages[0] = loadShader(getAssetPath() + "shaders/indirectdraw/skysphere.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/indirectdraw/skysphere.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		//rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skysphere));
	}

	// Prepare (and stage) a buffer containing the indirect draw commands
	void prepareIndirectData()
	{
		indirectCommands.clear();

		// Create on indirect command for each mesh in the scene
		uint32_t m = 0;
		for (auto& meshDescriptor : meshes.plants.meshDescriptors)
		{
			VkDrawIndexedIndirectCommand indirectCmd{};
			indirectCmd.instanceCount = OBJECT_INSTANCE_COUNT;
			indirectCmd.firstInstance = m * OBJECT_INSTANCE_COUNT;
			indirectCmd.firstIndex = meshDescriptor.indexBase;
			indirectCmd.indexCount = meshDescriptor.indexCount;

			indirectCommands.push_back(indirectCmd);

			m++;
		}

		indirectDrawCount = static_cast<uint32_t>(indirectCommands.size());

		objectCount = 0;
		for (auto indirectCmd : indirectCommands)
		{
			objectCount += indirectCmd.instanceCount;
		}

		vk::Buffer stagingBuffer;
		VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stagingBuffer,
			indirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
			indirectCommands.data()));

		VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&indirectCommandsBuffer,
			stagingBuffer.size));

		mVulkanDevice->copyBuffer(&stagingBuffer, &indirectCommandsBuffer, mQueue);

		stagingBuffer.destroy();
	}

	// Prepare (and stage) a buffer containing instanced data for the mesh draws
	void prepareInstanceData()
	{
		std::vector<InstanceData> instanceData;
		instanceData.resize(objectCount);

		std::mt19937 rndGenerator((unsigned)time(NULL));
		std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);

		for (uint32_t i = 0; i < objectCount; i++)
		{
			instanceData[i].rot = Vector3(0.0f, float(M_PI) * uniformDist(rndGenerator), 0.0f);
			float theta = 2 * float(M_PI) * uniformDist(rndGenerator);
			float phi = acos(1 - 2 * uniformDist(rndGenerator));
			instanceData[i].pos = Vector3(sin(phi) * cos(theta), 0.0f, cos(phi)) * PLANT_RADIUS;
			instanceData[i].scale = 1.0f + uniformDist(rndGenerator) * 2.0f;
			instanceData[i].texIndex = i / OBJECT_INSTANCE_COUNT;
		}

		vk::Buffer stagingBuffer;
		VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stagingBuffer,
			instanceData.size() * sizeof(InstanceData),
			instanceData.data()));

		VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&instanceBuffer,
			stagingBuffer.size));

		mVulkanDevice->copyBuffer(&stagingBuffer, &instanceBuffer, mQueue);

		stagingBuffer.destroy();
	}

	void prepareUniformBuffers()
	{
		VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformData.scene,
			sizeof(uboVS)));

		VK_CHECK_RESULT(uniformData.scene.map());

		updateUniformBuffer(true);
	}

	void updateUniformBuffer(bool viewChanged)
	{
		if (viewChanged)
		{
			uboVS.projection = mCamera.mMatrices.perspective;
			uboVS.view = mCamera.mMatrices.view;
		}

		memcpy(uniformData.scene.mapped, &uboVS, sizeof(uboVS));
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
		loadAssets();
		prepareIndirectData();
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
	}

	virtual void viewChanged()
	{
		updateUniformBuffer(true);
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
		textOverlay->addText(std::to_string(objectCount) + " objects", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		if (!mVulkanDevice->mFeatures.multiDrawIndirect)
		{
			textOverlay->addText("multiDrawIndirect not supported", 5.0f, 105.0f, VulkanTextOverlay::alignLeft);
		}
	}
};
