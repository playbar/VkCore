#include "Base.h"
#include "MeshPart.h"
#include "vulkantools.h"

namespace vkcore
{

MeshPart::MeshPart() :
    mMesh(NULL), mMeshIndex(0), mPrimitiveType(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST), 
	mIndexCount(0), mIndexSize(0), mDynamic(false)
{
}

MeshPart::~MeshPart()
{
    //if (_indexBuffer)
    //{
    //    glDeleteBuffers(1, &_indexBuffer);
    //}

	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mIndices.mVKBuffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mIndices.mVKMemory, nullptr);
}

MeshPart* MeshPart::create(Mesh* mesh, unsigned int meshIndex, VkPrimitiveTopology primitiveType,
    Mesh::IndexFormat indexFormat, unsigned int indexCount, bool dynamic)
{
    unsigned int indexSize = 0;
    switch (indexFormat)
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
        return NULL;
    }

    //GL_ASSERT( glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexSize * indexCount, NULL, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW) );

    MeshPart* part = new MeshPart();
    part->mMesh = mesh;
    part->mMeshIndex = meshIndex;
    part->mPrimitiveType = primitiveType;
    part->mIndexFormat = indexFormat;
    part->mIndexCount = indexCount;
	part->mIndexSize = indexSize;
    //part->_indexBuffer = vbo;
    part->mDynamic = dynamic;
	uint32_t indexBufferSize = indexCount * indexSize;

	// Index buffer
	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;
	VkBufferCreateInfo indexbufferInfo = {};
	indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexbufferInfo.size = indexBufferSize;
	indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	// Copy index data to a buffer visible to the host
	VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &indexbufferInfo, nullptr, &part->mIndices.mVKBuffer));
	vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, part->mIndices.mVKBuffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = gVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &part->mIndices.mVKMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, part->mIndices.mVKBuffer, part->mIndices.mVKMemory, 0));

    return part;
}

unsigned int MeshPart::getMeshIndex() const
{
    return mMeshIndex;
}

VkPrimitiveTopology MeshPart::getPrimitiveType() const
{
    return mPrimitiveType;
}

unsigned int MeshPart::getIndexCount() const
{
    return mIndexCount;
}

Mesh::IndexFormat MeshPart::getIndexFormat() const
{
    return mIndexFormat;
}

IndexBufferHandle MeshPart::getIndexBuffer() const
{
	GP_ASSERT(false);
    return 0;
}

void* MeshPart::mapIndexBuffer()
{
	uint32_t indexBufferSize = mIndexCount * mIndexSize;
	void *data;

	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mIndices.mVKMemory, 0, indexBufferSize, 0, &data));
	return data;
}

bool MeshPart::unmapIndexBuffer()
{
    //return glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mIndices.mVKMemory);
	return true;
}

void MeshPart::setIndexData(const void* indexData, unsigned int indexStart, unsigned int indexCount)
{
	uint32_t indexBufferSize = indexCount * mIndexSize;
	void *data;
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mIndices.mVKMemory, indexStart * mIndexSize, indexBufferSize, 0, &data));
	memcpy(data, indexData, indexBufferSize);
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mIndices.mVKMemory);
}

bool MeshPart::isDynamic() const
{
    return mDynamic;
}

}
