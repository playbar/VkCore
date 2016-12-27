#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include "define.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>

#include <vulkan/vulkan.h>
#include "VulkanBase.h"
#include "vulkanMeshLoader.hpp"

#define ENABLE_VALIDATION false

class VkDisplacement : public VulkanBase
{
	// Vertex layout for this example
	std::vector<vkMeshLoader::VertexLayout> vertexLayout =
	{
		vkMeshLoader::VERTEX_LAYOUT_POSITION,
		vkMeshLoader::VERTEX_LAYOUT_NORMAL,
		vkMeshLoader::VERTEX_LAYOUT_UV
	};
private:
	struct {
		vkTools::VulkanTexture colorHeightMap;
	} textures;
public:
	bool splitScreen = true;
	bool displacement = true;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct {
		vkMeshLoader::MeshBuffer object;
	} meshes;

	vkTools::UniformData uniformDataTC, uniformDataTE;

	struct {
		float tessLevel = 64.0f;
	} uboTC;

	struct {
		Matrix projection;
		Matrix model;
		Vector4 lightPos = Vector4(0.0f, -1.0f, 0.0f, 0.0f);
		float tessAlpha = 1.0f;
		float tessStrength = 0.1f;
	} uboTE;

	struct {
		VkPipeline solid;
		VkPipeline wireframe;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	VkDisplacement() : VulkanBase(ENABLE_VALIDATION)
	{
		mZoom = -1.25f;
		mRotation = Vector3(-20.0f, 45.0f, 0.0f);
		mEnableTextOverlay = true;
		title = "Vulkan Example - Tessellation shader displacement mapping";
		// Support for tessellation shaders is optional, so check first
		if (!mVulkanDevice->mFeatures.tessellationShader)
		{
			vkTools::exitFatal("Selected GPU does not support tessellation shaders!", "Feature not supported");
		}
	}

	~VkDisplacement()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.solid, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.wireframe, nullptr);

		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayout, nullptr);

		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.object);

		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, uniformDataTC.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, uniformDataTC.memory, nullptr);

		vkDestroyBuffer(mVulkanDevice->mLogicalDevice, uniformDataTE.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalDevice, uniformDataTE.memory, nullptr);

		textureLoader->destroyTexture(textures.colorHeightMap);
	}

	void loadTextures()
	{
		textureLoader->loadTexture(
			getAssetPath() + "textures/pattern_36_bc3.ktx",
			VK_FORMAT_BC3_UNORM_BLOCK,
			&textures.colorHeightMap);
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
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vkTools::renderPassBeginInfo();
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

			VkViewport viewport = vkTools::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vkTools::rect2D(splitScreen ? width / 2 : width, height, 0, 0);
			vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

			vkCmdSetLineWidth(mDrawCmdBuffers[i], 1.0f);

			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.object.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.object.indices.buf, 0, VK_INDEX_TYPE_UINT32);

			if (splitScreen)
			{
				vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireframe);
				vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.object.indexCount, 1, 0, 0, 0);
				scissor.offset.x = width / 2;
				vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);
			}

			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solid);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.object.indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(mDrawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
		}
	}

	void loadMeshes()
	{
		loadMesh(getAssetPath() + "models/plane.obj", &meshes.object, vertexLayout, 0.25f);
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
			vkTools::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				vkMeshLoader::vertexSize(vertexLayout),
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(3);

		// Location 0 : Position
		vertices.attributeDescriptions[0] =
			vkTools::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				0);

		// Location 1 : Normals
		vertices.attributeDescriptions[1] =
			vkTools::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 3);

		// Location 2 : Texture coordinates
		vertices.attributeDescriptions[2] =
			vkTools::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				VK_FORMAT_R32G32_SFLOAT,
				sizeof(float) * 6);

		vertices.inputState = vkTools::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		// Example uses two ubos and two image samplers
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
			vkTools::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
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
			// Binding 0 : Tessellation control shader ubo
			vkTools::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
				0),
			// Binding 1 : Tessellation evaluation shader ubo
			vkTools::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
				1),
			// Binding 2 : Combined color (rgb) and height (alpha) map
			vkTools::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				2),
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

		// Color and height map image descriptor
		VkDescriptorImageInfo texDescriptor =
			vkTools::descriptorImageInfo(
				textures.colorHeightMap.sampler,
				textures.colorHeightMap.view,
				VK_IMAGE_LAYOUT_GENERAL);

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Tessellation control shader ubo
			vkTools::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformDataTC.descriptor),
			// Binding 1 : Tessellation evaluation shader ubo
			vkTools::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
				&uniformDataTE.descriptor),
			// Binding 2 : Color and displacement map (alpha channel)
			vkTools::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				2,
				&texDescriptor),
		};

		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vkTools::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
				0,
				VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vkTools::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_NONE,
				VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vkTools::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				dynamicStateEnables.size(),
				0);

		VkPipelineTessellationStateCreateInfo tessellationState =
			vkTools::pipelineTessellationStateCreateInfo(3);

		// Tessellation pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages;
		shaderStages[0] = loadShader(getAssetPath() + "shaders/displacement/base.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/displacement/base.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderStages[2] = loadShader(getAssetPath() + "shaders/displacement/displacement.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		shaderStages[3] = loadShader(getAssetPath() + "shaders/displacement/displacement.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

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
		pipelineCreateInfo.pTessellationState = &tessellationState;
		pipelineCreateInfo.stageCount = shaderStages.size();
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.renderPass = mRenderPass;

		// Solid pipeline
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid));
		// Wireframe pipeline
		rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wireframe));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Tessellation evaluation shader uniform buffer
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uboTE),
			nullptr,
			&uniformDataTE.buffer,
			&uniformDataTE.memory,
			&uniformDataTE.descriptor);

		// Tessellation control shader uniform buffer
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uboTC),
			nullptr,
			&uniformDataTC.buffer,
			&uniformDataTC.memory,
			&uniformDataTC.descriptor);

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		// Tessellation eval
		Matrix viewMatrix, matTmp;
		Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(45.0f), (float)(width) / (float)height, 0.1f, 256.0f, &uboTE.projection);
		viewMatrix.translate(Vector3(0.0f, 0.0f, mZoom));
		//glm::mat4 viewMatrix = glm::mat4();
		//uboTE.projection = glm::perspective(glm::radians(45.0f), (float)(width) / (float)height, 0.1f, 256.0f);
		//viewMatrix = glm::translate(viewMatrix, glm::vec3(0.0f, 0.0f, mZoom));

		float offset = 0.5f;
		int uboIndex = 1;

		Matrix::createTranslation(Vector3(0.0f, 0.0f, 0.0f), &matTmp);
		Matrix::multiply(viewMatrix, matTmp, &uboTE.model);
		uboTE.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
		uboTE.model.rotateY(MATH_DEG_TO_RAD(mRotation.y));
		uboTE.model.rotateZ(MATH_DEG_TO_RAD(mRotation.z));
	
		//uboTE.model = glm::mat4();
		//uboTE.model = viewMatrix * glm::translate(uboTE.model, glm::vec3(0, 0, 0));
		//uboTE.model = glm::rotate(uboTE.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		//uboTE.model = glm::rotate(uboTE.model, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		//uboTE.model = glm::rotate(uboTE.model, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uboTE.lightPos.y = -0.5f - uboTE.tessStrength;
		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformDataTE.memory, 0, sizeof(uboTE), 0, (void **)&pData));
		memcpy(pData, &uboTE, sizeof(uboTE));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformDataTE.memory);

		// Tessellation control
		float savedLevel = uboTC.tessLevel;
		if (!displacement)
		{
			uboTC.tessLevel = 1.0f;
		}

		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformDataTC.memory, 0, sizeof(uboTC), 0, (void **)&pData));
		memcpy(pData, &uboTC, sizeof(uboTC));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformDataTC.memory);

		if (!displacement)
		{
			uboTC.tessLevel = savedLevel;
		}
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
		loadMeshes();
		loadTextures();
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
			return;
		draw();
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
	}

	void changeTessellationLevel(float delta)
	{
		uboTC.tessLevel += delta;
		uboTC.tessLevel = fmax(1.0, fmin(uboTC.tessLevel, 32.0));
		updateUniformBuffers();
		updateTextOverlay();
	}

	void changeTessellationStrength(float delta)
	{
		uboTE.tessStrength += delta;
		uboTE.tessStrength = fmax(0.0f, fmin(uboTE.tessStrength, 1.0f));
		updateUniformBuffers();
		updateTextOverlay();
	}

	void toggleSplitScreen()
	{
		splitScreen = !splitScreen;
		reBuildCommandBuffers();
		updateUniformBuffers();
	}

	void toggleDisplacement()
	{
		displacement = !displacement;
		updateUniformBuffers();
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case Keyboard::KEY_KPADD:
		case GAMEPAD_BUTTON_R1:
			changeTessellationStrength(0.025);
			break;
		case Keyboard::KEY_KPSUB:
		case GAMEPAD_BUTTON_L1:
			changeTessellationStrength(-0.025);
			break;
		case Keyboard::KEY_D:
		case GAMEPAD_BUTTON_A:
			toggleDisplacement();
			break;
		case Keyboard::KEY_S:
		case GAMEPAD_BUTTON_X:
			toggleSplitScreen();
			break;
		}
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
		std::stringstream ss;
		ss << std::setprecision(2) << std::fixed << uboTE.tessStrength;
#if defined(__ANDROID__)
		textOverlay->addText("Tessellation height: " + ss.str() + " (Buttons L1/R1)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		textOverlay->addText("Press \"Button A\" to toggle displacement", 5.0f, 100.0f, VulkanTextOverlay::alignLeft);
		textOverlay->addText("Press \"Button X\" to toggle splitscreen", 5.0f, 115.0f, VulkanTextOverlay::alignLeft);
#else
		textOverlay->addText("Tessellation height: " + ss.str() + " (numpad +/-)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		textOverlay->addText("Press \"d\" to toggle displacement", 5.0f, 100.0f, VulkanTextOverlay::alignLeft);
		textOverlay->addText("Press \"s\" to toggle splitscreen", 5.0f, 115.0f, VulkanTextOverlay::alignLeft);
#endif
	}
};
