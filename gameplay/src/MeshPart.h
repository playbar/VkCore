#ifndef MESHPART_H_
#define MESHPART_H_

#include "Mesh.h"

namespace vkcore
{

/**
 * Defines a part of a mesh describing the way the
 * mesh's vertices are connected together.
 */
class MeshPart
{
    friend class Mesh;
    friend class Model;

public:
    ~MeshPart();
    unsigned int getMeshIndex() const;
	VkPrimitiveTopology getPrimitiveType() const;
    unsigned int getIndexCount() const;
    Mesh::IndexFormat getIndexFormat() const;
    IndexBufferHandle getIndexBuffer() const;
    void* mapIndexBuffer();
    bool unmapIndexBuffer();
    void setIndexData(const void* indexData, unsigned int indexStart, unsigned int indexCount);
    bool isDynamic() const;

	// Index buffer
	struct
	{
		VkDeviceMemory mVKMemory;
		VkBuffer mVKBuffer;
	} mIndices;

private:
   
    MeshPart();
    MeshPart(const MeshPart& copy);
    static MeshPart* create(Mesh* mesh, unsigned int meshIndex, VkPrimitiveTopology primitiveType, Mesh::IndexFormat indexFormat, unsigned int indexCount, bool dynamic = false);

    Mesh* mMesh;
    unsigned int mMeshIndex;
	VkPrimitiveTopology mPrimitiveType;
    Mesh::IndexFormat mIndexFormat;
    unsigned int mIndexCount;
	unsigned int mIndexSize;
    //IndexBufferHandle _indexBuffer;
    bool mDynamic;
};

}

#endif
