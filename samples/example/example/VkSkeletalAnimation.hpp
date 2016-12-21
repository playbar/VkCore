#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include "define.h"

#include <vulkan/vulkan.h>
#include "VulkanBase.h"
#include "vulkanbuffer.hpp"

#define ENABLE_VALIDATION false

std::vector<vkMeshLoader::VertexLayout> vertexLayout =
{
	vkMeshLoader::VERTEX_LAYOUT_POSITION,
	vkMeshLoader::VERTEX_LAYOUT_NORMAL,
	vkMeshLoader::VERTEX_LAYOUT_UV,
	vkMeshLoader::VERTEX_LAYOUT_COLOR,
	vkMeshLoader::VERTEX_LAYOUT_DUMMY_VEC4,
	vkMeshLoader::VERTEX_LAYOUT_DUMMY_VEC4
};

// Maximum number of bones per mesh
// Must not be higher than same const in skinning shader
#define MAX_BONES 64
// Maximum number of bones per vertex
#define MAX_BONES_PER_VERTEX 4

// Skinned mesh class

// Per-vertex bone IDs and weights
struct VertexBoneData
{
	std::array<uint32_t, MAX_BONES_PER_VERTEX> IDs;
	std::array<float, MAX_BONES_PER_VERTEX> weights;

	// Ad bone weighting to vertex info
	void add(uint32_t boneID, float weight)
	{
		for (uint32_t i = 0; i < MAX_BONES_PER_VERTEX; i++)
		{
			if (weights[i] == 0.0f)
			{
				IDs[i] = boneID;
				weights[i] = weight;
				return;
			}
		}
	}
};

// Stores information on a single bone
struct BoneInfo
{
	aiMatrix4x4 offset;
	aiMatrix4x4 finalTransformation;

	BoneInfo()
	{
		offset = aiMatrix4x4();
		finalTransformation = aiMatrix4x4();
	};
};

class SkinnedMesh
{
public:
	// Bone related stuff
	// Maps bone name with index
	std::map<std::string, uint32_t> boneMapping;
	// Bone details
	std::vector<BoneInfo> boneInfo;
	// Number of bones present
	uint32_t numBones = 0;
	// Root inverese transform matrix
	aiMatrix4x4 globalInverseTransform;
	// Per-vertex bone info
	std::vector<VertexBoneData> bones;
	// Bone transformations
	std::vector<aiMatrix4x4> boneTransforms;

	// Modifier for the animation 
	float animationSpeed = 0.75f;
	// Currently active animation
	aiAnimation* pAnimation;

	// Vulkan buffers
	vkMeshLoader::MeshBuffer meshBuffer;
	// Reference to assimp mesh
	// Required for animation
	VulkanMeshLoader *meshLoader;

	// Set active animation by index
	void setAnimation(uint32_t animationIndex)
	{
		assert(animationIndex < meshLoader->pScene->mNumAnimations);
		pAnimation = meshLoader->pScene->mAnimations[animationIndex];
	}

	// Load bone information from ASSIMP mesh
	void loadBones(uint32_t meshIndex, const aiMesh* pMesh, std::vector<VertexBoneData>& Bones)
	{
		for (uint32_t i = 0; i < pMesh->mNumBones; i++)
		{
			uint32_t index = 0;

			assert(pMesh->mNumBones <= MAX_BONES);

			std::string name(pMesh->mBones[i]->mName.data);

			if (boneMapping.find(name) == boneMapping.end())
			{
				// Bone not present, add new one
				index = numBones;
				numBones++;
				BoneInfo bone;
				boneInfo.push_back(bone);
				boneInfo[index].offset = pMesh->mBones[i]->mOffsetMatrix;
				boneMapping[name] = index;
			}
			else
			{
				index = boneMapping[name];
			}

			for (uint32_t j = 0; j < pMesh->mBones[i]->mNumWeights; j++)
			{
				uint32_t vertexID = meshLoader->m_Entries[meshIndex].vertexBase + pMesh->mBones[i]->mWeights[j].mVertexId;
				Bones[vertexID].add(index, pMesh->mBones[i]->mWeights[j].mWeight);
			}
		}
		boneTransforms.resize(numBones);
	}

	// Recursive bone transformation for given animation time
	void update(float time)
	{
		float TicksPerSecond = (float)(meshLoader->pScene->mAnimations[0]->mTicksPerSecond != 0 ? meshLoader->pScene->mAnimations[0]->mTicksPerSecond : 25.0f);
		float TimeInTicks = time * TicksPerSecond;
		float AnimationTime = fmod(TimeInTicks, (float)meshLoader->pScene->mAnimations[0]->mDuration);

		aiMatrix4x4 identity = aiMatrix4x4();
		readNodeHierarchy(AnimationTime, meshLoader->pScene->mRootNode, identity);

		for (uint32_t i = 0; i < boneTransforms.size(); i++)
		{
			boneTransforms[i] = boneInfo[i].finalTransformation;
		}
	}

private:
	// Find animation for a given node
	const aiNodeAnim* findNodeAnim(const aiAnimation* animation, const std::string nodeName)
	{
		for (uint32_t i = 0; i < animation->mNumChannels; i++)
		{
			const aiNodeAnim* nodeAnim = animation->mChannels[i];
			if (std::string(nodeAnim->mNodeName.data) == nodeName)
			{
				return nodeAnim;
			}
		}
		return nullptr;
	}

	// Returns a 4x4 matrix with interpolated translation between current and next frame
	aiMatrix4x4 interpolateTranslation(float time, const aiNodeAnim* pNodeAnim)
	{
		aiVector3D translation;

		if (pNodeAnim->mNumPositionKeys == 1)
		{
			translation = pNodeAnim->mPositionKeys[0].mValue;
		}
		else
		{
			uint32_t frameIndex = 0;
			for (uint32_t i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
			{
				if (time < (float)pNodeAnim->mPositionKeys[i + 1].mTime)
				{
					frameIndex = i;
					break;
				}
			}

			aiVectorKey currentFrame = pNodeAnim->mPositionKeys[frameIndex];
			aiVectorKey nextFrame = pNodeAnim->mPositionKeys[(frameIndex + 1) % pNodeAnim->mNumPositionKeys];

			float delta = (time - (float)currentFrame.mTime) / (float)(nextFrame.mTime - currentFrame.mTime);

			const aiVector3D& start = currentFrame.mValue;
			const aiVector3D& end = nextFrame.mValue;

			translation = (start + delta * (end - start));
		}

		aiMatrix4x4 mat;
		aiMatrix4x4::Translation(translation, mat);
		return mat;
	}

	// Returns a 4x4 matrix with interpolated rotation between current and next frame
	aiMatrix4x4 interpolateRotation(float time, const aiNodeAnim* pNodeAnim)
	{
		aiQuaternion rotation;

		if (pNodeAnim->mNumRotationKeys == 1)
		{
			rotation = pNodeAnim->mRotationKeys[0].mValue;
		}
		else
		{
			uint32_t frameIndex = 0;
			for (uint32_t i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++)
			{
				if (time < (float)pNodeAnim->mRotationKeys[i + 1].mTime)
				{
					frameIndex = i;
					break;
				}
			}

			aiQuatKey currentFrame = pNodeAnim->mRotationKeys[frameIndex];
			aiQuatKey nextFrame = pNodeAnim->mRotationKeys[(frameIndex + 1) % pNodeAnim->mNumRotationKeys];

			float delta = (time - (float)currentFrame.mTime) / (float)(nextFrame.mTime - currentFrame.mTime);

			const aiQuaternion& start = currentFrame.mValue;
			const aiQuaternion& end = nextFrame.mValue;

			aiQuaternion::Interpolate(rotation, start, end, delta);
			rotation.Normalize();
		}

		aiMatrix4x4 mat(rotation.GetMatrix());
		return mat;
	}


	// Returns a 4x4 matrix with interpolated scaling between current and next frame
	aiMatrix4x4 interpolateScale(float time, const aiNodeAnim* pNodeAnim)
	{
		aiVector3D scale;

		if (pNodeAnim->mNumScalingKeys == 1)
		{
			scale = pNodeAnim->mScalingKeys[0].mValue;
		}
		else
		{
			uint32_t frameIndex = 0;
			for (uint32_t i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
			{
				if (time < (float)pNodeAnim->mScalingKeys[i + 1].mTime)
				{
					frameIndex = i;
					break;
				}
			}

			aiVectorKey currentFrame = pNodeAnim->mScalingKeys[frameIndex];
			aiVectorKey nextFrame = pNodeAnim->mScalingKeys[(frameIndex + 1) % pNodeAnim->mNumScalingKeys];

			float delta = (time - (float)currentFrame.mTime) / (float)(nextFrame.mTime - currentFrame.mTime);

			const aiVector3D& start = currentFrame.mValue;
			const aiVector3D& end = nextFrame.mValue;

			scale = (start + delta * (end - start));
		}

		aiMatrix4x4 mat;
		aiMatrix4x4::Scaling(scale, mat);
		return mat;
	}

	// Get node hierarchy for current animation time
	void readNodeHierarchy(float AnimationTime, const aiNode* pNode, const aiMatrix4x4& ParentTransform)
	{
		std::string NodeName(pNode->mName.data);

		aiMatrix4x4 NodeTransformation(pNode->mTransformation);

		const aiNodeAnim* pNodeAnim = findNodeAnim(pAnimation, NodeName);

		if (pNodeAnim)
		{
			// Get interpolated matrices between current and next frame
			aiMatrix4x4 matScale = interpolateScale(AnimationTime, pNodeAnim);
			aiMatrix4x4 matRotation = interpolateRotation(AnimationTime, pNodeAnim);
			aiMatrix4x4 matTranslation = interpolateTranslation(AnimationTime, pNodeAnim);

			NodeTransformation = matTranslation * matRotation * matScale;
		}

		aiMatrix4x4 GlobalTransformation = ParentTransform * NodeTransformation;

		if (boneMapping.find(NodeName) != boneMapping.end())
		{
			uint32_t BoneIndex = boneMapping[NodeName];
			boneInfo[BoneIndex].finalTransformation = globalInverseTransform * GlobalTransformation * boneInfo[BoneIndex].offset;
		}

		for (uint32_t i = 0; i < pNode->mNumChildren; i++)
		{
			readNodeHierarchy(AnimationTime, pNode->mChildren[i], GlobalTransformation);
		}
	}
};

class VkSkeletalAnimation : public VulkanBase
{

	// Vertex layout used in this example
	struct Vertex 
	{
		Vector3 pos;
		Vector3 normal;
		Vector2 uv;
		Vector3 color;
		// Max. four bones per vertex
		float boneWeights[4];
		uint32_t boneIDs[4];
	};
public:
	struct {
		vkTools::VulkanTexture colorMap;
		vkTools::VulkanTexture floor;
	} textures;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	SkinnedMesh *skinnedMesh = nullptr;

	struct {
		vk::Buffer mesh;
		vk::Buffer floor;
	} uniformBuffers;

	struct {
		Matrix projection;
		Matrix model;
		Matrix view;
		Matrix bones[MAX_BONES];
		Vector4 lightPos = Vector4(0.0f, -250.0f, 250.0f, 1.0);
		Vector4 viewPos;
	} uboVS;

	struct {
		Matrix projection;
		Matrix model;
		Matrix view;
		Vector4 lightPos = Vector4(0.0, 0.0f, -25.0f, 1.0);
		Vector4 viewPos;
		Vector2 uvOffset;
	} uboFloor;

	struct {
		VkPipeline skinning;
		VkPipeline texture;
	} pipelines;

	struct {
		vkMeshLoader::MeshBuffer floor;
	} meshes;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	struct {
		VkDescriptorSet skinning;
		VkDescriptorSet floor;
	} descriptorSets;

	float runningTime = 0.0f;

	VkSkeletalAnimation() : VulkanBase(ENABLE_VALIDATION)
	{
		mZoom = -150.0f;
		zoomSpeed = 2.5f;
		rotationSpeed = 0.5f;
		mRotation = { -182.5f, -38.5f, 180.0f };
		mEnableTextOverlay = true;
		title = "Skeletal animation";
		cameraPos = { 0.0f, 0.0f, 12.0f };
	}

	~VkSkeletalAnimation()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.skinning, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalDevice, pipelines.texture, nullptr);

		vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, descriptorSetLayout, nullptr);

		textureLoader->destroyTexture(textures.colorMap);
		textureLoader->destroyTexture(textures.floor);

		uniformBuffers.mesh.destroy();
		uniformBuffers.floor.destroy();

		// Destroy and free mesh resources 
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &meshes.floor);
		vkMeshLoader::freeMeshBufferResources(mVulkanDevice->mLogicalDevice, &skinnedMesh->meshBuffer);
		delete(skinnedMesh->meshLoader);
		delete(skinnedMesh);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
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
			renderPassBeginInfo.framebuffer = mFrameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(mDrawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(mDrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vkTools::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vkTools::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize offsets[1] = { 0 };

			// Skinned mesh
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skinning);

			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &skinnedMesh->meshBuffer.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], skinnedMesh->meshBuffer.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], skinnedMesh->meshBuffer.indexCount, 1, 0, 0, 0);

			// Floor
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.floor, 0, NULL);
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.texture);

			vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.floor.vertices.buf, offsets);
			vkCmdBindIndexBuffer(mDrawCmdBuffers[i], meshes.floor.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(mDrawCmdBuffers[i], meshes.floor.indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(mDrawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
		}
	}

	// Load a mesh based on data read via assimp 
	// The other example will use the VulkanMesh loader which has some additional functionality for loading meshes
	void loadMesh()
	{
		skinnedMesh = new SkinnedMesh();
		skinnedMesh->meshLoader = new VulkanMeshLoader(mVulkanDevice);
#if defined(__ANDROID__)
		skinnedMesh->meshLoader->assetManager = androidApp->activity->assetManager;
#endif
		skinnedMesh->meshLoader->LoadMesh(getAssetPath() + "models/goblin.dae", 0);
		skinnedMesh->setAnimation(0);

		// Setup bones
		// One vertex bone info structure per vertex
		skinnedMesh->bones.resize(skinnedMesh->meshLoader->numVertices);
		// Store global inverse transform matrix of root node 
		skinnedMesh->globalInverseTransform = skinnedMesh->meshLoader->pScene->mRootNode->mTransformation;
		skinnedMesh->globalInverseTransform.Inverse();
		// Load bones (weights and IDs)
		for (uint32_t m = 0; m < skinnedMesh->meshLoader->m_Entries.size(); m++)
		{
			aiMesh *paiMesh = skinnedMesh->meshLoader->pScene->mMeshes[m];
			if (paiMesh->mNumBones > 0)
			{
				skinnedMesh->loadBones(m, paiMesh, skinnedMesh->bones);
			}
		}

		// Generate vertex buffer
		std::vector<Vertex> vertexBuffer;
		// Iterate through all meshes in the file
		// and extract the vertex information used in this demo
		for (uint32_t m = 0; m < skinnedMesh->meshLoader->m_Entries.size(); m++)
		{
			for (uint32_t i = 0; i < skinnedMesh->meshLoader->m_Entries[m].Vertices.size(); i++)
			{
				Vertex vertex;

				vertex.pos = skinnedMesh->meshLoader->m_Entries[m].Vertices[i].m_pos;
				vertex.pos.y = -vertex.pos.y;
				vertex.normal = skinnedMesh->meshLoader->m_Entries[m].Vertices[i].m_normal;
				vertex.uv = skinnedMesh->meshLoader->m_Entries[m].Vertices[i].m_tex;
				vertex.color = skinnedMesh->meshLoader->m_Entries[m].Vertices[i].m_color;

				// Fetch bone weights and IDs
				for (uint32_t j = 0; j < MAX_BONES_PER_VERTEX; j++)
				{
					vertex.boneWeights[j] = skinnedMesh->bones[skinnedMesh->meshLoader->m_Entries[m].vertexBase + i].weights[j];
					vertex.boneIDs[j] = skinnedMesh->bones[skinnedMesh->meshLoader->m_Entries[m].vertexBase + i].IDs[j];
				}

				vertexBuffer.push_back(vertex);
			}
		}
		VkDeviceSize vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);

		// Generate index buffer from loaded mesh file
		std::vector<uint32_t> indexBuffer;
		for (uint32_t m = 0; m < skinnedMesh->meshLoader->m_Entries.size(); m++)
		{
			uint32_t indexBase = static_cast<uint32_t>(indexBuffer.size());
			for (uint32_t i = 0; i < skinnedMesh->meshLoader->m_Entries[m].Indices.size(); i++)
			{
				indexBuffer.push_back(skinnedMesh->meshLoader->m_Entries[m].Indices[i] + indexBase);
			}
		}
		VkDeviceSize indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
		skinnedMesh->meshBuffer.indexCount = indexBuffer.size();

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
				&skinnedMesh->meshBuffer.vertices.buf,
				&skinnedMesh->meshBuffer.vertices.mem);
			// Index buffer
			createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				indexBufferSize,
				nullptr,
				&skinnedMesh->meshBuffer.indices.buf,
				&skinnedMesh->meshBuffer.indices.mem);

			// Copy from staging buffers
			VkCommandBuffer copyCmd = VulkanBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkBufferCopy copyRegion = {};

			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(
				copyCmd,
				vertexStaging.buffer,
				skinnedMesh->meshBuffer.vertices.buf,
				1,
				&copyRegion);

			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(
				copyCmd,
				indexStaging.buffer,
				skinnedMesh->meshBuffer.indices.buf,
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
				&skinnedMesh->meshBuffer.vertices.buf,
				&skinnedMesh->meshBuffer.vertices.mem);
			// Index buffer
			createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				indexBufferSize,
				indexBuffer.data(),
				&skinnedMesh->meshBuffer.indices.buf,
				&skinnedMesh->meshBuffer.indices.mem);
		}
	}

	void loadAssets()
	{
		textureLoader->loadTexture(getAssetPath() + "textures/goblin_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &textures.colorMap);
		textureLoader->loadTexture(getAssetPath() + "textures/trail_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &textures.floor);
		VulkanBase::loadMesh(getAssetPath() + "models/plane_z.obj", &meshes.floor, vertexLayout, 512.0f);
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
		vertices.attributeDescriptions.resize(6);
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
		// Location 4 : Bone weights
		vertices.attributeDescriptions[4] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				4,
				VK_FORMAT_R32G32B32A32_SFLOAT,
				sizeof(float) * 11);
		// Location 5 : Bone IDs
		vertices.attributeDescriptions[5] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				5,
				VK_FORMAT_R32G32B32A32_SINT,
				sizeof(float) * 15);

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

		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSet));

		VkDescriptorImageInfo texDescriptor =
			vkTools::initializers::descriptorImageInfo(
				textures.colorMap.sampler,
				textures.colorMap.view,
				VK_IMAGE_LAYOUT_GENERAL);

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkTools::initializers::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffers.mesh.descriptor),
			// Binding 1 : Color map 
			vkTools::initializers::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&texDescriptor)
		};

		vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

		// Floor
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &allocInfo, &descriptorSets.floor));

		texDescriptor.imageView = textures.floor.view;
		texDescriptor.sampler = textures.floor.sampler;

		writeDescriptorSets.clear();

		// Binding 0 : Vertex shader uniform buffer
		writeDescriptorSets.push_back(
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.floor,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffers.floor.descriptor));
		// Binding 1 : Color map 
		writeDescriptorSets.push_back(
			vkTools::initializers::writeDescriptorSet(
				descriptorSets.floor,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&texDescriptor));

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

		// Skinned rendering pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/skeletalanimation/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/skeletalanimation/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skinning));

		shaderStages[0] = loadShader(getAssetPath() + "shaders/skeletalanimation/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/skeletalanimation/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.texture));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Mesh uniform buffer block
		mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.mesh,
			sizeof(uboVS));
		// Map persistant
		VK_CHECK_RESULT(uniformBuffers.mesh.map());

		// Floor uniform buffer block
		mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.floor,
			sizeof(uboFloor));
		// Map persistant
		VK_CHECK_RESULT(uniformBuffers.floor.map());

		updateUniformBuffers(true);
	}

	void updateUniformBuffers(bool viewChanged)
	{
		if (viewChanged)
		{
			Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), (float)width / (float)height, 0.1f, 1024.0f, &uboVS.projection);
			Matrix viewMatrix, matTmp;
			viewMatrix.translate(0.0f, 0.0f, mZoom);
			viewMatrix.rotateX(MATH_DEG_TO_RAD(90.0f));
			viewMatrix.scale(0.025f);


			//uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 1024.0f);
			//glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, mZoom));
			//viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			//viewMatrix = glm::scale(viewMatrix, glm::vec3(0.025f));

			matTmp.translate(Vector3(cameraPos.x, -cameraPos.z, cameraPos.y) * 100.0f);
			uboVS.view = viewMatrix * matTmp;
			uboVS.view.rotateX(MATH_DEG_TO_RAD(mRotation.x));
			uboVS.view.rotateY(MATH_DEG_TO_RAD(mRotation.z));
			uboVS.view.rotateZ(MATH_DEG_TO_RAD(mRotation.y));

			//uboVS.view = viewMatrix * glm::translate(glm::mat4(), glm::vec3(cameraPos.x, -cameraPos.z, cameraPos.y) * 100.0f);
			//uboVS.view = glm::rotate(uboVS.view, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			//uboVS.view = glm::rotate(uboVS.view, glm::radians(mRotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
			//uboVS.view = glm::rotate(uboVS.view, glm::radians(-mRotation.y), glm::vec3(0.0f, 0.0f, 1.0f));

			uboVS.viewPos = Vector4(0.0f, 0.0f, -mZoom, 0.0f);

			uboFloor.projection = uboVS.projection;
			uboFloor.view = viewMatrix;
			uboFloor.model.setIdentity();
			uboFloor.model.translate(Vector3(cameraPos.x, -cameraPos.z, cameraPos.y) * 100.0f);
			uboFloor.model.rotateX(MATH_DEG_TO_RAD(mRotation.x));
			uboFloor.model.rotateY(MATH_DEG_TO_RAD(mRotation.z));
			uboFloor.model.rotateZ(MATH_DEG_TO_RAD(mRotation.y));
			uboFloor.model.translate(Vector3(0.0f, 0.0f, -1800.0f));
			//uboFloor.model = glm::translate(glm::mat4(), glm::vec3(cameraPos.x, -cameraPos.z, cameraPos.y) * 100.0f);
			//uboFloor.model = glm::rotate(uboFloor.model, glm::radians(mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			//uboFloor.model = glm::rotate(uboFloor.model, glm::radians(mRotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
			//uboFloor.model = glm::rotate(uboFloor.model, glm::radians(-mRotation.y), glm::vec3(0.0f, 0.0f, 1.0f));
			//uboFloor.model = glm::translate(uboFloor.model, glm::vec3(0.0f, 0.0f, -1800.0f));
			uboFloor.viewPos = Vector4(0.0f, 0.0f, -mZoom, 0.0f);
		}

		// Update bones
		skinnedMesh->update(runningTime);
		for (uint32_t i = 0; i < skinnedMesh->boneTransforms.size(); i++)
		{
			//uboVS.bones[i] = glm::transpose(glm::make_mat4(&skinnedMesh->boneTransforms[i].a1));
			uboVS.bones[i].set(&skinnedMesh->boneTransforms[i].a1);
			uboVS.bones[i].transpose();
		}

		uniformBuffers.mesh.copyTo(&uboVS, sizeof(uboVS));

		// Update floor animation
		uboFloor.uvOffset.y -= 0.25f * skinnedMesh->animationSpeed * frameTimer;
		uniformBuffers.floor.copyTo(&uboFloor, sizeof(uboFloor));
	}

	void draw()
	{
		VulkanBase::prepareFrame();

		mSubmitInfo.commandBufferCount = 1;
		mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[gSwapChain.mCurrentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		VulkanBase::submitFrame();
	}

	void prepare()
	{
		VulkanBase::prepare();
		loadAssets();
		loadMesh();
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
		if (!paused)
		{
			runningTime += frameTimer * skinnedMesh->animationSpeed;
			vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);
			updateUniformBuffers(false);
		}
	}

	virtual void viewChanged()
	{
		vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);
		updateUniformBuffers(true);
	}

	void changeAnimationSpeed(float delta)
	{
		skinnedMesh->animationSpeed += delta;
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case Keyboard::KEY_KPADD:
		case GAMEPAD_BUTTON_R1:
			changeAnimationSpeed(0.1f);
			break;
		case Keyboard::KEY_KPSUB:
		case GAMEPAD_BUTTON_L1:
			changeAnimationSpeed(-0.1f);
			break;
		}
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
		if (skinnedMesh != nullptr)
		{
			std::stringstream ss;
			ss << std::setprecision(2) << std::fixed << skinnedMesh->animationSpeed;
#if defined(__ANDROID__)
			textOverlay->addText("Animation speed: " + ss.str() + " (Buttons L1/R1 to change)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#else
			textOverlay->addText("Animation speed: " + ss.str() + " (numpad +/- to change)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
		}
	}
};
