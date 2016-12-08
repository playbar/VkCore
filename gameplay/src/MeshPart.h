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
    Mesh::PrimitiveType getPrimitiveType() const;
    unsigned int getIndexCount() const;
    Mesh::IndexFormat getIndexFormat() const;
    IndexBufferHandle getIndexBuffer() const;
    void* mapIndexBuffer();
    bool unmapIndexBuffer();
    void setIndexData(const void* indexData, unsigned int indexStart, unsigned int indexCount);
    bool isDynamic() const;

private:
   
    MeshPart();
    MeshPart(const MeshPart& copy);
    static MeshPart* create(Mesh* mesh, unsigned int meshIndex, Mesh::PrimitiveType primitiveType, Mesh::IndexFormat indexFormat, unsigned int indexCount, bool dynamic = false);

    Mesh* _mesh;
    unsigned int _meshIndex;
	VkPrimitiveTopology _primitiveType;
    Mesh::IndexFormat _indexFormat;
    unsigned int _indexCount;
    IndexBufferHandle _indexBuffer;
    bool _dynamic;
};

}

#endif
