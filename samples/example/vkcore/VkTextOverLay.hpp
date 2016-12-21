#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include "define.h"

#include <vulkan/vulkan.h>

#include "VulkanBase.h"
#include "VkCoreDevice.hpp"
#include "vulkantextoverlay.hpp"

#define ENABLE_VALIDATION false


class VkTextOverLay : public VulkanBase
{
	// Vertex layout for this example
	std::vector<vkMeshLoader::VertexLayout> vertexLayout =
	{
		vkMeshLoader::VERTEX_LAYOUT_POSITION,
		vkMeshLoader::VERTEX_LAYOUT_NORMAL,
		vkMeshLoader::VERTEX_LAYOUT_UV,
		vkMeshLoader::VERTEX_LAYOUT_COLOR,
	};

public:
	//VulkanTextOverlay *mTextOverlay = nullptr;

	struct
	{
		vkTools::VulkanTexture background;
		vkTools::VulkanTexture cube;
	} textures;

	struct
	{
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct
	{
		vkMeshLoader::MeshBuffer cube;
	} meshes;

	struct
	{
		vkTools::UniformData vsScene;
	} uniformData;

	struct
	{
		Matrix projection;
		Matrix model;
		Vector4 lightPos = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	} uboVS;

	struct
	{
		VkPipeline solid;
		VkPipeline background;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	struct
	{
		VkDescriptorSet background;
		VkDescriptorSet cube;
	} descriptorSets;


	VkTextOverLay() : VulkanBase(ENABLE_VALIDATION)
	{
		mZoom = -4.5f;
		zoomSpeed = 2.5f;
		mRotation = { -25.0f, 0.0f, 0.0f };
		title = "Text overlay";
		// Disable text overlay of the example base class
		mEnableTextOverlay = false;
	}

	~VkTextOverLay()
	{
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.solid, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.background, nullptr);
		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayout, nullptr);
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.cube);
		textureLoader->destroyTexture(textures.background);
		textureLoader->destroyTexture(textures.cube);
		vkTools::destroyUniformData(mVulkanDevice->mLogicalDevice, &uniformData.vsScene);
		delete(mTextOverlay);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[3];

		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vkTools::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = mRenderPass;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 3;
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

			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.background, 0, NULL);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.cube.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.cube.indices.buf, 0, VK_INDEX_TYPE_UINT32);

			// Background
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.background);
			// Vertices are generated by the vertex shader
			vkCmdDraw(mDrawCmdBuffers[i], 4, 1, 0, 0);

			// Cube
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solid);
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.cube, 0, NULL);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.cube.indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(mDrawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
		}

		vkQueueWaitIdle(mQueue);
	}

	// Update the text buffer displayed by the text overlay
	void updateTextOverlay(void)
	{
		mTextOverlay->beginTextUpdate();

		mTextOverlay->addText(title, 5.0f, 5.0f, VulkanTextOverlay::alignLeft);

		std::stringstream ss;
		ss << std::fixed << std::setprecision(2) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
		mTextOverlay->addText(ss.str(), 5.0f, 25.0f, VulkanTextOverlay::alignLeft);

		mTextOverlay->addText(mVulkanDevice->mProperties.deviceName, 5.0f, 45.0f, VulkanTextOverlay::alignLeft);

		mTextOverlay->addText("Press \"space\" to toggle text overlay", 5.0f, 65.0f, VulkanTextOverlay::alignLeft);

		// Display projected cube vertices
		for (int32_t x = -1; x <= 1; x += 2)
		{
			for (int32_t y = -1; y <= 1; y += 2)
			{
				for (int32_t z = -1; z <= 1; z += 2)
				{
					std::stringstream vpos;
					vpos << std::showpos << x << "/" << y << "/" << z;
					//glm::vec3 projected = glm::project(glm::vec3((float)x, (float)y, (float)z), uboVS.model, uboVS.projection, glm::vec4(0, 0, (float)width, (float)height));
					Vector4 projected = Matrix::project(Vector3(x, y, z), uboVS.model, uboVS.projection, Vector4(0, 0, width, height));
					mTextOverlay->addText(vpos.str(), projected.x, projected.y + (y > -1 ? 5.0f : -20.0f), VulkanTextOverlay::alignCenter);
				}
			}
		}

		// Display current model view matrix
		mTextOverlay->addText("model view matrix", width, 5.0f, VulkanTextOverlay::alignRight);

		for (uint32_t i = 0; i < 4; i++)
		{
			ss.str("");
			ss << std::fixed << std::setprecision(2) << std::showpos;
			ss << uboVS.model.m[i] << " " << uboVS.model.m[4 + i] << " " << uboVS.model.m[ 8 + i] << " " << uboVS.model.m[ 12 + i];
			mTextOverlay->addText(ss.str(), width, 25.0f + (float)i * 20.0f, VulkanTextOverlay::alignRight);
		}
		Vector4 projected = Matrix::project(Vector3(0.0f), uboVS.model, uboVS.projection, Vector4(0.0f, 0.0f, width, height));
		//glm::vec3 projected = glm::project(glm::vec3(0.0f), uboVS.model, uboVS.projection, glm::vec4(0, 0, (float)width, (float)height));
		mTextOverlay->addText("Uniform cube", projected.x, projected.y, VulkanTextOverlay::alignCenter);

#if defined(__ANDROID__)
		// toto
#else
		mTextOverlay->addText("Hold middle mouse button and drag to move", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
		mTextOverlay->endTextUpdate();
	}

	void loadTextures()
	{
		textureLoader->loadTexture(getAssetPath() + "textures/skysphere_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &textures.background);
		textureLoader->loadTexture(getAssetPath() + "textures/round_window_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &textures.cube);
	}

	void loadMeshes()
	{
		loadMesh(getAssetPath() + "models/cube.dae", &meshes.cube, vertexLayout, 1.0f);
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
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2),
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vkTools::initializers::descriptorPoolCreateInfo(
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
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo =
			vkTools::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayout,
				1);

		// Background
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.background));

		VkDescriptorImageInfo texDescriptor =
			vkTools::initializers::descriptorImageInfo(
				textures.background.sampler,
				textures.background.view,
				VK_IMAGE_LAYOUT_GENERAL);

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;

		// Binding 0 : Vertex shader uniform buffer
		writeDescriptorSets.push_back(
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.background,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformData.vsScene.descriptor));

		// Binding 1 : Color map 
		writeDescriptorSets.push_back(
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.background,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&texDescriptor));

		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

		// Cube
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.cube));
		texDescriptor.sampler = textures.cube.sampler;
		texDescriptor.imageView = textures.cube.view;
		writeDescriptorSets[0].dstSet = descriptorSets.cube;
		writeDescriptorSets[1].dstSet = descriptorSets.cube;
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

		// Wire frame rendering pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/textoverlay/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/textoverlay/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid));

		// Background rendering pipeline
		depthStencilState.depthTestEnable = VK_FALSE;
		depthStencilState.depthWriteEnable = VK_FALSE;

		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/textoverlay/background.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/textoverlay/background.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.background));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Vertex shader uniform buffer block
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			sizeof(uboVS),
			&uboVS,
			&uniformData.vsScene.buffer,
			&uniformData.vsScene.memory,
			&uniformData.vsScene.descriptor);

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		Matrix viewMatrix, matTmp;
		Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), (float)width / (float)height, 0.1f, 256.0f, &uboVS.projection);
		viewMatrix.translate(0.0f, 0.0f, mZoom);
		// Vertex shader
		//uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
		//glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, mZoom));

		matTmp.translate(cameraPos);
		uboVS.model = viewMatrix * matTmp;
		uboVS.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
		uboVS.model.rotateY(MATH_DEG_TO_RAD(mRotation.y));
		uboVS.model.rotateZ(MATH_DEG_TO_RAD(mRotation.z));

		//uboVS.model = viewMatrix * glm::translate(glm::mat4(), cameraPos);
		//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		//uboVS.model = glm::rotate(uboVS.model, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsScene.memory, 0, sizeof(uboVS), 0, (void **)&pData));
		memcpy(pData, &uboVS, sizeof(uboVS));
		vkUnmapMemory(mVulkanDevice->mLogicalDevice, uniformData.vsScene.memory);
	}

	void prepareTextOverlay()
	{
		// Load the text rendering shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/textoverlay/text.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/textoverlay/text.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

		mTextOverlay = new VulkanTextOverlay(
			mVulkanDevice,
			mQueue,
			mFrameBuffers,
			mColorformat,
			mDepthFormat,
			&width,
			&height,
			shaderStages
		);
		updateTextOverlay();
	}

	void draw()
	{
		VulkanBase::prepareFrame();

		// Command buffer to be sumitted to the queue
		mSubmitInfo.commandBufferCount = 1;
		mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[gSwapChain.mCurrentBuffer];

		// Submit to queue
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		// Submit text overlay to queue
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		mTextOverlay->submit(mQueue, gSwapChain.mCurrentBuffer);

		VulkanBase::submitFrame();
	}

	void prepare()
	{
		VulkanBase::prepare();
		loadTextures();
		loadMeshes();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepareTextOverlay();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
		if (frameCounter == 0)
		{
			vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);
			updateTextOverlay();
		}
	}

	virtual void viewChanged()
	{
		vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);
		updateUniformBuffers();
		updateTextOverlay();
	}

	virtual void windowResized()
	{
		updateTextOverlay();
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case Keyboard::KEY_KPADD:
		case Keyboard::KEY_SPACE:
			mTextOverlay->mVisible = !mTextOverlay->mVisible;
		}
	}
};

