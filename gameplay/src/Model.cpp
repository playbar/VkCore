#include "Base.h"
#include "Model.h"
#include "MeshPart.h"
#include "Scene.h"
#include "Technique.h"
#include "Pass.h"
#include "Node.h"
#include "vulkantools.h"

namespace vkcore
{


	static bool drawWireframe(Mesh* mesh)
	{
		switch (mesh->getPrimitiveType())
		{
		case Mesh::TRIANGLES:
		{
			unsigned int vertexCount = mesh->getVertexCount();
			for (unsigned int i = 0; i < vertexCount; i += 3)
			{
				GL_ASSERT(glDrawArrays(GL_LINE_LOOP, i, 3));
			}
		}
		return true;

		case Mesh::TRIANGLE_STRIP:
		{
			unsigned int vertexCount = mesh->getVertexCount();
			for (unsigned int i = 2; i < vertexCount; ++i)
			{
				GL_ASSERT(glDrawArrays(GL_LINE_LOOP, i - 2, 3));
			}
		}
		return true;

		default:
			// not supported
			return false;
		}
	}

	static bool drawWireframe(MeshPart* part)
	{
		unsigned int indexCount = part->getIndexCount();
		unsigned int indexSize = 0;
		switch (part->getIndexFormat())
		{
		case Mesh::INDEX8:
			indexSize = 1;
			break;
		case Mesh::INDEX16:
			indexSize = 2;
			break;
		case Mesh::INDEX32:
			indexSize = 4;
			break;
		default:
			GP_ERROR("Unsupported index format (%d).", part->getIndexFormat());
			return false;
		}

		switch (part->getPrimitiveType())
		{
		case Mesh::TRIANGLES:
		{
			for (size_t i = 0; i < indexCount; i += 3)
			{
				GL_ASSERT(glDrawElements(GL_LINE_LOOP, 3, part->getIndexFormat(), ((const GLvoid*)(i*indexSize))));
			}
		}
		return true;

		case Mesh::TRIANGLE_STRIP:
		{
			for (size_t i = 2; i < indexCount; ++i)
			{
				GL_ASSERT(glDrawElements(GL_LINE_LOOP, 3, part->getIndexFormat(), ((const GLvoid*)((i - 2)*indexSize))));
			}
		}
		return true;

		default:
			// not supported
			return false;
		}
	}

Model::Model() : Drawable(),
    _mesh(NULL), _material(NULL), _partCount(0), _partMaterials(NULL), _skin(NULL)
{
	prepare();
}

Model::Model(Mesh* mesh) : Drawable(),
    _mesh(mesh), _material(NULL), _partCount(0), _partMaterials(NULL), _skin(NULL)
{
    GP_ASSERT(mesh);
    _partCount = mesh->getPartCount();
	prepare();
}

Model::~Model()
{

	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mVertices.buffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mVertices.memory, nullptr);

	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mIndices.buffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mIndices.memory, nullptr);


    SAFE_RELEASE(_material);
    if (_partMaterials)
    {
        for (unsigned int i = 0; i < _partCount; ++i)
        {
            SAFE_RELEASE(_partMaterials[i]);
        }
        SAFE_DELETE_ARRAY(_partMaterials);
    }
    SAFE_RELEASE(_mesh);
    SAFE_DELETE(_skin);
}

void Model::UninitVulkan()
{
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(gVulkanDevice->mLogicalDevice, descriptorPool, nullptr);
	}

	destroyCommandBuffers();
	vkDestroyRenderPass(gVulkanDevice->mLogicalDevice, mRenderPass, nullptr);
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(gVulkanDevice->mLogicalDevice, mFrameBuffers[i], nullptr);
	}

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(gVulkanDevice->mLogicalDevice, shaderModule, nullptr);
	}
	vkDestroyImageView(gVulkanDevice->mLogicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(gVulkanDevice->mLogicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, depthStencil.mem, nullptr);
	vkDestroyPipelineCache(gVulkanDevice->mLogicalDevice, pipelineCache, nullptr);
	vkDestroyCommandPool(gVulkanDevice->mLogicalDevice, mCmdPool, nullptr);
	vkDestroyPipeline(gVulkanDevice->mLogicalDevice, mPipeline, nullptr);
	vkDestroyPipelineLayout(gVulkanDevice->mLogicalDevice, mPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(gVulkanDevice->mLogicalDevice, mDescriptorSetLayout, nullptr);
	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory, nullptr);

	return;
}

Model* Model::create(Mesh* mesh)
{
    GP_ASSERT(mesh);
    mesh->addRef();
    return new Model(mesh);
}

Mesh* Model::getMesh() const
{
    return _mesh;
}

unsigned int Model::getMeshPartCount() const
{
    GP_ASSERT(_mesh);
    return _mesh->getPartCount();
}

Material* Model::getMaterial(int partIndex)
{
    GP_ASSERT(partIndex == -1 || partIndex >= 0);

    Material* m = NULL;

    if (partIndex < 0)
        return _material;
    if (partIndex >= (int)_partCount)
        return NULL;

    // Look up explicitly specified part material.
    if (_partMaterials)
    {
        m = _partMaterials[partIndex];
    }
    if (m == NULL)
    {
        // Return the shared material.
         m = _material;
    }

    return m;
}

void Model::setMaterial(Material* material, int partIndex)
{
    GP_ASSERT(partIndex == -1 || (partIndex >= 0 && partIndex < (int)getMeshPartCount()));

    Material* oldMaterial = NULL;

    if (partIndex == -1)
    {
        oldMaterial = _material;

        // Set new shared material.
        if (material)
        {
            _material = material;
            _material->addRef();
        }
    }
    else if (partIndex >= 0 && partIndex < (int)getMeshPartCount())
    {
        // Ensure mesh part count is up-to-date.
        validatePartCount();

        // Release existing part material and part binding.
        if (_partMaterials)
        {
            oldMaterial = _partMaterials[partIndex];
        }
        else
        {
            // Allocate part arrays for the first time.
            if (_partMaterials == NULL)
            {
                _partMaterials = new Material*[_partCount];
                memset(_partMaterials, 0, sizeof(Material*) * _partCount);
            }
        }

        // Set new part material.
        if (material)
        {
            _partMaterials[partIndex] = material;
            material->addRef();
        }
    }

    // Release existing material and binding.
    if (oldMaterial)
    {
        for (unsigned int i = 0, tCount = oldMaterial->getTechniqueCount(); i < tCount; ++i)
        {
            Technique* t = oldMaterial->getTechniqueByIndex(i);
            GP_ASSERT(t);
            for (unsigned int j = 0, pCount = t->getPassCount(); j < pCount; ++j)
            {
                GP_ASSERT(t->getPassByIndex(j));
                t->getPassByIndex(j)->setVertexAttributeBinding(NULL);
            }
        }
        SAFE_RELEASE(oldMaterial);
    }

    if (material)
    {
        // Hookup vertex attribute bindings for all passes in the new material.
        for (unsigned int i = 0, tCount = material->getTechniqueCount(); i < tCount; ++i)
        {
            Technique* t = material->getTechniqueByIndex(i);
            GP_ASSERT(t);
            for (unsigned int j = 0, pCount = t->getPassCount(); j < pCount; ++j)
            {
                Pass* p = t->getPassByIndex(j);
                GP_ASSERT(p);
                VertexAttributeBinding* b = VertexAttributeBinding::create(_mesh, p->getEffect());
                p->setVertexAttributeBinding(b);
                SAFE_RELEASE(b);
            }
        }
        // Apply node binding for the new material.
        if (_node)
        {
            setMaterialNodeBinding(material);
        }
    }
}

Material* Model::setMaterial(const char* vshPath, const char* fshPath, const char* defines, int partIndex)
{
    // Try to create a Material with the given parameters.
    Material* material = Material::create(vshPath, fshPath, defines);
    if (material == NULL)
    {
        GP_ERROR("Failed to create material for model.");
        return NULL;
    }

    // Assign the material to us.
    setMaterial(material, partIndex);

    // Release the material since we now have a reference to it.
    material->release();

    return material;
}

Material* Model::setMaterial(const char* materialPath, int partIndex)
{
    // Try to create a Material from the specified material file.
    Material* material = Material::create(materialPath);
    if (material == NULL)
    {
        GP_ERROR("Failed to create material for model.");
        return NULL;
    }

    // Assign the material to us
    setMaterial(material, partIndex);

    // Release the material since we now have a reference to it
    material->release();

    return material;
}

bool Model::hasMaterial(unsigned int partIndex) const
{
    return (partIndex < _partCount && _partMaterials && _partMaterials[partIndex]);
}

MeshSkin* Model::getSkin() const
{
    return _skin;
}

void Model::setSkin(MeshSkin* skin)
{
    if (_skin != skin)
    {
        // Free the old skin
        SAFE_DELETE(_skin);

        // Assign the new skin
        _skin = skin;
        if (_skin)
            _skin->_model = this;
    }
}

void Model::setNode(Node* node)
{
    Drawable::setNode(node);

    // Re-bind node related material parameters
    if (node)
    {
        if (_material)
        {
           setMaterialNodeBinding(_material);
        }
        if (_partMaterials)
        {
            for (unsigned int i = 0; i < _partCount; ++i)
            {
                if (_partMaterials[i])
                {
                    setMaterialNodeBinding(_partMaterials[i]);
                }
            }
        }
    }
}

bool Model::checkCommandBuffers()
{
	for (auto& cmdBuffer : mDrawCmdBuffers)
	{
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			return false;
		}
	}
	return true;
}

unsigned int Model::draw(bool wireframe)
{
    GP_ASSERT(_mesh);

	VkFence fence = gVulkanDevice->mWaitFences[gSwapChain.mCurrentBuffer];
	VK_CHECK_RESULT(vkWaitForFences(gVulkanDevice->mLogicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));
	VK_CHECK_RESULT(vkResetFences(gVulkanDevice->mLogicalDevice, 1, &fence));

    unsigned int partCount = _mesh->getPartCount();

	VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = &waitStageMask;
	submitInfo.pWaitSemaphores = &gVulkanDevice->presentCompleteSemaphore;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &gVulkanDevice->renderCompleteSemaphore;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pCommandBuffers = &mDrawCmdBuffers[gSwapChain.mCurrentBuffer];
	submitInfo.commandBufferCount = 1;
	
	VK_CHECK_RESULT(vkQueueSubmit(gVulkanDevice->mQueue, 1, &submitInfo, fence));

    return partCount;
}


// Prepare vertex and index buffers for an indexed triangle
// Also uploads them to device local memory using staging and initializes vertex input and attribute binding to match the vertex shader
void Model::prepareVertices()
{
	// A note on memory management in Vulkan in general:
	//	This is a very complex topic and while it's fine for an example application to to small individual memory allocations that is not
	//	what should be done a real-world application, where you should allocate large chunkgs of memory at once isntead.

	// Setup vertices
	std::vector<Vertex> vertexBuffer =
	{
		{ { 0.9f,  1.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },
		{ { -1.0f,  1.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },
		{ { -1.0f, -1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
		//{ { 0.9f, -1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f } }
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
	std::vector<uint32_t> indexBuffer = { 1, 0, 2 };
	mIndices.count = static_cast<uint32_t>(indexBuffer.size());
	uint32_t indexBufferSize = mIndices.count * sizeof(uint32_t);

	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	void *data;

	
	// Don't use staging
	// Create host-visible buffers only and use these for rendering. This is not advised and will usually result in lower rendering performance

	// Vertex buffer
	VkBufferCreateInfo vertexBufferInfo = {};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.size = vertexBufferSize;
	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	// Copy vertex data to a buffer visible to the host
	VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &vertexBufferInfo, nullptr, &mVertices.buffer));
	vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mVertices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = gVulkanDevice->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mVertices.memory));
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mVertices.memory, 0, memAlloc.allocationSize, 0, &data));
	memcpy(data, vertexBuffer.data(), vertexBufferSize);
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mVertices.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mVertices.buffer, mVertices.memory, 0));

	// Index buffer
	VkBufferCreateInfo indexbufferInfo = {};
	indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexbufferInfo.size = indexBufferSize;
	indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	// Copy index data to a buffer visible to the host
	VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &indexbufferInfo, nullptr, &mIndices.buffer));
	vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mIndices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = gVulkanDevice->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mIndices.memory));
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mIndices.memory, 0, indexBufferSize, 0, &data));
	memcpy(data, indexBuffer.data(), indexBufferSize);
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mIndices.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mIndices.buffer, mIndices.memory, 0));


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

void Model::prepare()
{
	vkTools::getSupportedDepthFormat(gVulkanDevice->mPhysicalDevice, &mDepthFormat);
	createCommandPool();
	createCommandBuffers();
	setupDepthStencil();
	setupRenderPass();
	createPipelineCache();
	setupFrameBuffer();

	prepareVertices();
	prepareUniformBuffers();
	setupDescriptorSetLayout();
	preparePipelines();
	setupDescriptorPool();
	setupDescriptorSet();
	buildCommandBuffers();

}

void Model::setupDepthStencil()
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
	VK_CHECK_RESULT(vkCreateImage(gVulkanDevice->mLogicalDevice, &image, nullptr, &depthStencil.image));

	// Allocate memory for the image (device local) and bind it to our image
	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(gVulkanDevice->mLogicalDevice, depthStencil.image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = gVulkanDevice->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(gVulkanDevice->mLogicalDevice, depthStencil.image, depthStencil.mem, 0));

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
	VK_CHECK_RESULT(vkCreateImageView(gVulkanDevice->mLogicalDevice, &depthStencilView, nullptr, &depthStencil.view));

}

void Model::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(gVulkanDevice->mLogicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}


void Model::setupDescriptorPool()
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

	VK_CHECK_RESULT(vkCreateDescriptorPool(gVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

}


void Model::setupDescriptorSet()
{
	// Allocate a new descriptor set from the global descriptor pool
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &mDescriptorSetLayout;

	VK_CHECK_RESULT(vkAllocateDescriptorSets(gVulkanDevice->mLogicalDevice, &allocInfo, &mDescriptorSet));

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

	vkUpdateDescriptorSets(gVulkanDevice->mLogicalDevice, 1, &writeDescriptorSet, 0, nullptr);

}

void Model::setupDescriptorSetLayout()
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

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(gVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));

	// Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
	// In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
	pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pPipelineLayoutCreateInfo.pNext = nullptr;
	pPipelineLayoutCreateInfo.setLayoutCount = 1;
	pPipelineLayoutCreateInfo.pSetLayouts = &mDescriptorSetLayout;

	VK_CHECK_RESULT(vkCreatePipelineLayout(gVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
}


void Model::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = gSwapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(gVulkanDevice->mLogicalDevice, &cmdPoolInfo, nullptr, &mCmdPool));
}

void Model::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	mDrawCmdBuffers.resize(gSwapChain.mImageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::initializers::commandBufferAllocateInfo(
			mCmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			static_cast<uint32_t>(mDrawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(gVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, mDrawCmdBuffers.data()));
}


void Model::buildCommandBuffers()
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
		vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &_mesh->mVertices.buffer, offsets);

		// Bind triangle index buffer
		vkCmdBindIndexBuffer(mDrawCmdBuffers[i], _mesh->getPart(0)->mIndices.mVKBuffer, 0, VK_INDEX_TYPE_UINT32);

		// Draw indexed triangle
		vkCmdDrawIndexed(mDrawCmdBuffers[i], _mesh->getPart(0)->getIndexCount(), 1, 0, 0, 1);

		vkCmdEndRenderPass(mDrawCmdBuffers[i]);

		// Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to 
		// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

		VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
	}
}


void Model::destroyCommandBuffers()
{
	vkFreeCommandBuffers(gVulkanDevice->mLogicalDevice, mCmdPool, static_cast<uint32_t>(mDrawCmdBuffers.size()), mDrawCmdBuffers.data());
}


void Model::setupRenderPass()
{
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

	VK_CHECK_RESULT(vkCreateRenderPass(gVulkanDevice->mLogicalDevice, &renderPassInfo, nullptr, &mRenderPass));
	return;
}


void Model::setupFrameBuffer()
{
	VkImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depthStencil.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = mRenderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	mFrameBuffers.resize(gSwapChain.mImageCount);
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		attachments[0] = gSwapChain.buffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(gVulkanDevice->mLogicalDevice, &frameBufferCreateInfo, nullptr, &mFrameBuffers[i]));
	}
}

void Model::preparePipelines()
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
	shaderStages[0] = loadShader(gVulkanDevice->getAssetPath() + "shaders/triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(gVulkanDevice->getAssetPath() + "shaders/triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	// Assign the pipeline states to the pipeline creation info structure
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &_mesh->mVertices.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.renderPass = mRenderPass;
	pipelineCreateInfo.pDynamicState = &dynamicState;

	// Create rendering pipeline using the specified states
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(gVulkanDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeline));

}



void Model::prepareUniformBuffers()
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
	VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &bufferInfo, nullptr, &mUniformDataVS.buffer));
	// Get memory requirements including size, alignment and memory type 
	vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, &memReqs);
	allocInfo.allocationSize = memReqs.size;
	// Get the memory type index that supports host visibile memory access
	// Most implementations offer multiple memory types and selecting the correct one to allocate memory from is crucial
	// We also want the buffer to be host coherent so we don't have to flush (or sync after every update.
	// Note: This may affect performance so you might not want to do this in a real world application that updates buffers on a regular base
	allocInfo.memoryTypeIndex = gVulkanDevice->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	// Allocate memory for the uniform buffer
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &allocInfo, nullptr, &(mUniformDataVS.memory)));
	// Bind memory to buffer
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, mUniformDataVS.memory, 0));

	// Store information in the uniform's descriptor that is used by the descriptor set
	mUniformDataVS.descriptor.buffer = mUniformDataVS.buffer;
	mUniformDataVS.descriptor.offset = 0;
	mUniformDataVS.descriptor.range = sizeof(mUboVS);

	updateUniformBuffers();
}

void Model::updateUniformBuffers()
{
	// Update matrices
	float aspect = (float)width / (float)height;
	vkcore::Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), 1.0f, 0.1f, 256.0f, &mUboVS.projectionMatrix);
	Matrix::createTranslation(0.0f, 0.0f, -10.0f, &mUboVS.viewMatrix);
	Matrix::createRotationX( 0.0f, &mUboVS.modelMatrix);
	mUboVS.modelMatrix.rotateY(0.0f);
	mUboVS.modelMatrix.rotateZ(0.0f);
	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory, 0, sizeof(mUboVS), 0, (void **)&pData));
	memcpy(pData, &mUboVS, sizeof(mUboVS));
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory);
}

VkPipelineShaderStageCreateInfo Model::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
#if defined(__ANDROID__)
	shaderStage.module = vkTools::loadShader(androidApp->activity->assetManager, fileName.c_str(), gVulkanDevice->mLogicalDevice, stage);
#else
	shaderStage.module = vkTools::loadShader(fileName.c_str(), gVulkanDevice->mLogicalDevice, stage);
#endif
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != NULL);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}


void Model::setMaterialNodeBinding(Material *material)
{
    GP_ASSERT(material);

    if (_node)
    {
        material->setNodeBinding(getNode());
    }
}

Drawable* Model::clone(NodeCloneContext& context)
{
    Model* model = Model::create(getMesh());
    if (!model)
    {
        GP_ERROR("Failed to clone model.");
        return NULL;
    }

    if (getSkin())
    {
        model->setSkin(getSkin()->clone(context));
    }
    if (getMaterial())
    {
        Material* materialClone = getMaterial()->clone(context);
        if (!materialClone)
        {
            GP_ERROR("Failed to clone material for model.");
            return model;
        }
        model->setMaterial(materialClone);
        materialClone->release();
    }
    if (_partMaterials)
    {
        GP_ASSERT(_partCount == model->_partCount);
        for (unsigned int i = 0; i < _partCount; ++i)
        {
            if (_partMaterials[i])
            {
                Material* materialClone = _partMaterials[i]->clone(context);
                model->setMaterial(materialClone, i);
                materialClone->release();
            }
        }
    }
    return model;
}

void Model::validatePartCount()
{
    GP_ASSERT(_mesh);
    unsigned int partCount = _mesh->getPartCount();

    if (_partCount != partCount)
    {
        // Allocate new arrays and copy old items to them.
        if (_partMaterials)
        {
            Material** oldArray = _partMaterials;
            _partMaterials = new Material*[partCount];
            memset(_partMaterials, 0, sizeof(Material*) * partCount);
            if (oldArray)
            {
                for (unsigned int i = 0; i < _partCount; ++i)
                {
                    _partMaterials[i] = oldArray[i];
                }
            }
            SAFE_DELETE_ARRAY(oldArray);
        }
        // Update local part count.
        _partCount = _mesh->getPartCount();
    }
}

}
