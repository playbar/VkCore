#ifndef MESH_H_
#define MESH_H_

#include "Ref.h"
#include "VertexFormat.h"
#include "Vector3.h"
#include "BoundingBox.h"
#include "BoundingSphere.h"
#include "vulkan/vulkan.h"
#include "VkCoreDevice.hpp"

namespace vkcore
{

class MeshPart;
class Material;
class Model;

/**
 * Defines a mesh supporting various vertex formats and 1 or more
 * MeshPart(s) to define how the vertices are connected.
 */
class Mesh : public Ref
{
    friend class Model;
    friend class Bundle;

public:

    /**
     * Defines supported index formats.
     */
    enum IndexFormat
    {
        INDEX8 = GL_UNSIGNED_BYTE,
        INDEX16 = GL_UNSIGNED_SHORT,
        INDEX32 = GL_UNSIGNED_INT
    };

    /**
     * Defines supported primitive types.
     */
    enum PrimitiveType
    {
        TRIANGLES = GL_TRIANGLES,
        TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
        LINES = GL_LINES,
        LINE_STRIP = GL_LINE_STRIP,
        POINTS = GL_POINTS
    };

    static Mesh* createMesh( const VertexFormat& vertexFormat, unsigned int vertexCount, bool dynamic = false);

    static Mesh* createQuad(const Vector3& p1, const Vector3& p2, const Vector3& p3, const Vector3& p4);

    static Mesh* createQuad(float x, float y, float width, float height, float s1 = 0.0f, float t1 = 0.0f, float s2 = 1.0f, float t2 = 1.0f);

    static Mesh* createQuadFullscreen();

    static Mesh* createLines(Vector3* points, unsigned int pointCount);

    static Mesh* createBoundingBox(const BoundingBox& box);

    const char* getUrl() const;


    const VertexFormat& getVertexFormat() const;

    unsigned int getVertexCount() const;

    unsigned int getVertexSize() const;

    VertexBufferHandle getVertexBuffer() const;

    bool isDynamic() const;

    PrimitiveType getPrimitiveType() const;

    void setPrimitiveType(Mesh::PrimitiveType type);

    void* mapVertexBuffer();

    bool unmapVertexBuffer();

    void setVertexData(const void* vertexData, unsigned int vertexStart = 0, unsigned int vertexCount = 0);

    MeshPart* addPart(PrimitiveType primitiveType, Mesh::IndexFormat indexFormat, unsigned int indexCount, bool dynamic = false);

    unsigned int getPartCount() const;

    MeshPart* getPart(unsigned int index);

    const BoundingBox& getBoundingBox() const;

    void setBoundingBox(const BoundingBox& box);

    const BoundingSphere& getBoundingSphere() const;

    void setBoundingSphere(const BoundingSphere& sphere);

    virtual ~Mesh();

private:

    Mesh( const VertexFormat& vertexFormat);

    Mesh(const Mesh& copy);

    Mesh& operator=(const Mesh&);


	// Vertex buffer and attributes
	struct
	{
		VkDeviceMemory memory;															// Handle to the device memory for this buffer
		VkBuffer buffer;																// Handle to the Vulkan buffer object that the memory is bound to
		VkPipelineVertexInputStateCreateInfo inputState;
		VkVertexInputBindingDescription inputBinding;
		std::vector<VkVertexInputAttributeDescription> inputAttributes;
	} mVertices;


	// Index buffer
	struct
	{
		VkDeviceMemory memory;
		VkBuffer buffer;
		uint32_t count;
	} mIndices;

    std::string _url;
    const VertexFormat _vertexFormat;
    unsigned int _vertexCount;
    //VertexBufferHandle _vertexBuffer;
    PrimitiveType _primitiveType;
    unsigned int _partCount;
    MeshPart** _parts;
    bool _dynamic;
    BoundingBox _boundingBox;
    BoundingSphere _boundingSphere;
};

}

#endif
