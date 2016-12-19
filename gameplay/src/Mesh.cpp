#include "Base.h"
#include "Mesh.h"
#include "MeshPart.h"
#include "Effect.h"
#include "Model.h"
#include "Material.h"
#include "define.h"
#include "Game.h"
#include "vulkantools.h"

namespace vkcore
{

Mesh::Mesh( const VertexFormat& vertexFormat)
    : mVertexFormat(vertexFormat), mVertexCount(0), mPrimitiveType(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      mPartCount(0), mParts(NULL), _dynamic(false)
{
	//_vertexBuffer = 0;
}

Mesh::~Mesh()
{
    if (mParts)
    {
        for (unsigned int i = 0; i < mPartCount; ++i)
        {
            SAFE_DELETE(mParts[i]);
        }
        SAFE_DELETE_ARRAY(mParts);
    }

	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mVertices.buffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mVertices.memory, nullptr);

    //if (_vertexBuffer)
    //{
    //    glDeleteBuffers(1, &_vertexBuffer);
    //    _vertexBuffer = 0;
    //}
}

Mesh* Mesh::createMesh(const VertexFormat& vertexFormat, unsigned int vertexCount, bool dynamic)
{
	///////////////////////////////////////////
	Mesh* mesh = new Mesh(vertexFormat);
	mesh->mVertexCount = vertexCount;
	//mesh->_vertexBuffer = vbo;
	mesh->_dynamic = dynamic;

	uint32_t vertexBufferSize = vertexFormat.getVertexSize() * vertexCount;


	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	VkBufferCreateInfo vertexBufferInfo = {};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.size = vertexBufferSize;
	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &vertexBufferInfo, nullptr, &mesh->mVertices.buffer));
	vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mesh->mVertices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = gVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mesh->mVertices.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mesh->mVertices.buffer, mesh->mVertices.memory, 0));
	
	// Vertex input binding
	mesh->mVertices.inputBinding.binding = VERTEX_BUFFER_BIND_ID;
	mesh->mVertices.inputBinding.stride = vertexFormat.getVertexSize();
	mesh->mVertices.inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// Inpute attribute binding describe shader attribute locations and memory layouts
	// These match the following shader layout (see triangle.vert):
	//	layout (location = 0) in vec3 inPos;
	//	layout (location = 1) in vec3 inColor;
	mesh->mVertices.inputAttributes.resize(2);
	// Attribute location 0: Position
	mesh->mVertices.inputAttributes[0].binding = VERTEX_BUFFER_BIND_ID;
	mesh->mVertices.inputAttributes[0].location = 0;
	mesh->mVertices.inputAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	mesh->mVertices.inputAttributes[0].offset = 0;// offsetof(Vertex, position);
	// Attribute location 1: Color
	mesh->mVertices.inputAttributes[1].binding = VERTEX_BUFFER_BIND_ID;
	mesh->mVertices.inputAttributes[1].location = 1;
	mesh->mVertices.inputAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	mesh->mVertices.inputAttributes[1].offset = 12;// offsetof(Vertex, color);

	// Assign to the vertex input state used for pipeline creation
	mesh->mVertices.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	mesh->mVertices.inputState.pNext = nullptr;
	mesh->mVertices.inputState.flags = VK_FLAGS_NONE;
	mesh->mVertices.inputState.vertexBindingDescriptionCount = 1;
	mesh->mVertices.inputState.pVertexBindingDescriptions = &mesh->mVertices.inputBinding;
	mesh->mVertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(mesh->mVertices.inputAttributes.size());
	mesh->mVertices.inputState.pVertexAttributeDescriptions = mesh->mVertices.inputAttributes.data();

    return mesh;
}


Mesh* Mesh::createQuad(float x, float y, float width, float height, float s1, float t1, float s2, float t2)
{
    float x2 = x + width;
    float y2 = y + height;

    float vertices[] =
    {
        x, y2, 0,   0, 0, 1,    s1, t2,
        x, y, 0,    0, 0, 1,    s1, t1,
        x2, y2, 0,  0, 0, 1,    s2, t2,
        x2, y, 0,   0, 0, 1,    s2, t1,
    };

    VertexFormat::Element elements[] =
    {
        VertexFormat::Element(VertexFormat::POSITION, 3),
        VertexFormat::Element(VertexFormat::NORMAL, 3),
        VertexFormat::Element(VertexFormat::TEXCOORD0, 2)
    };
    Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 3), 4, false);
    if (mesh == NULL)
    {
        GP_ERROR("Failed to create mesh.");
        return NULL;
    }

    mesh->mPrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    mesh->setVertexData(vertices, 0, 4);

    return mesh;
}

Mesh* Mesh::createQuadFullscreen()
{
    float x = -1.0f;
    float y = -1.0f;
    float x2 = 1.0f;
    float y2 = 1.0f;

    float vertices[] =
    {
        x, y2,   0, 1,
        x, y,    0, 0,
        x2, y2,  1, 1,
        x2, y,   1, 0
    };

    VertexFormat::Element elements[] =
    {
        VertexFormat::Element(VertexFormat::POSITION, 2),
        VertexFormat::Element(VertexFormat::TEXCOORD0, 2)
    };
    Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 2), 4, false);
    if (mesh == NULL)
    {
        GP_ERROR("Failed to create mesh.");
        return NULL;
    }

    mesh->mPrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    mesh->setVertexData(vertices, 0, 4);

    return mesh;
}

Mesh* Mesh::createQuad(const Vector3& p1, const Vector3& p2, const Vector3& p3, const Vector3& p4)
{
    // Calculate the normal vector of the plane.
    Vector3 v1, v2, n;
    Vector3::subtract(p2, p1, &v1);
    Vector3::subtract(p3, p2, &v2);
    Vector3::cross(v1, v2, &n);
    n.normalize();

    float vertices[] =
    {
        p1.x, p1.y, p1.z, n.x, n.y, n.z, 0, 1,
        p2.x, p2.y, p2.z, n.x, n.y, n.z, 0, 0,
        p3.x, p3.y, p3.z, n.x, n.y, n.z, 1, 1,
        p4.x, p4.y, p4.z, n.x, n.y, n.z, 1, 0
    };

    VertexFormat::Element elements[] =
    {
        VertexFormat::Element(VertexFormat::POSITION, 3),
        VertexFormat::Element(VertexFormat::NORMAL, 3),
        VertexFormat::Element(VertexFormat::TEXCOORD0, 2)
    };

    Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 3), 4, false);
    if (mesh == NULL)
    {
        GP_ERROR("Failed to create mesh.");
        return NULL;
    }

    mesh->mPrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    mesh->setVertexData(vertices, 0, 4);

    return mesh;
}

Mesh* Mesh::createLines(Vector3* points, unsigned int pointCount)
{
    GP_ASSERT(points);
    GP_ASSERT(pointCount);

    float* vertices = new float[pointCount*3];
    memcpy(vertices, points, pointCount*3*sizeof(float));

    VertexFormat::Element elements[] =
    {
        VertexFormat::Element(VertexFormat::POSITION, 3)
    };
    Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 1), pointCount, false);
    if (mesh == NULL)
    {
        GP_ERROR("Failed to create mesh.");
        SAFE_DELETE_ARRAY(vertices);
        return NULL;
    }

    mesh->mPrimitiveType = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    mesh->setVertexData(vertices, 0, pointCount);

    SAFE_DELETE_ARRAY(vertices);
    return mesh;
}

Mesh* Mesh::createBoundingBox(const BoundingBox& box)
{
    Vector3 corners[8];
    box.getCorners(corners);

    float vertices[] =
    {
        corners[7].x, corners[7].y, corners[7].z,
        corners[6].x, corners[6].y, corners[6].z,
        corners[1].x, corners[1].y, corners[1].z,
        corners[0].x, corners[0].y, corners[0].z,
        corners[7].x, corners[7].y, corners[7].z,
        corners[4].x, corners[4].y, corners[4].z,
        corners[3].x, corners[3].y, corners[3].z, 
        corners[0].x, corners[0].y, corners[0].z,
        corners[0].x, corners[0].y, corners[0].z,
        corners[1].x, corners[1].y, corners[1].z,
        corners[2].x, corners[2].y, corners[2].z,
        corners[3].x, corners[3].y, corners[3].z, 
        corners[4].x, corners[4].y, corners[4].z,
        corners[5].x, corners[5].y, corners[5].z, 
        corners[2].x, corners[2].y, corners[2].z,
        corners[1].x, corners[1].y, corners[1].z,
        corners[6].x, corners[6].y, corners[6].z,
        corners[5].x, corners[5].y, corners[5].z
    };

    VertexFormat::Element elements[] =
    {
        VertexFormat::Element(VertexFormat::POSITION, 3)
    };
    Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 1), 18, false);
    if (mesh == NULL)
    {
        GP_ERROR("Failed to create mesh.");
        return NULL;
    }

    mesh->mPrimitiveType = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    mesh->setVertexData(vertices, 0, 18);

    return mesh;
}

const char* Mesh::getUrl() const
{
    return _url.c_str();
}

const VertexFormat& Mesh::getVertexFormat() const
{
    return mVertexFormat;
}

unsigned int Mesh::getVertexCount() const
{
    return mVertexCount;
}

unsigned int Mesh::getVertexSize() const
{
    return mVertexFormat.getVertexSize();
}

VertexBufferHandle Mesh::getVertexBuffer() const
{
	GP_ASSERT(false);
    return 0;
}

bool Mesh::isDynamic() const
{
    return _dynamic;
}

VkPrimitiveTopology Mesh::getPrimitiveType() const
{
    return mPrimitiveType;
}

void Mesh::setPrimitiveType(VkPrimitiveTopology type)
{
    mPrimitiveType = type;
}

void* Mesh::mapVertexBuffer()
{
    //return (void*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	GP_ASSERT(false);
	void *data;
	uint32_t vertexBufferSize = mVertexFormat.getVertexSize() * mVertexCount;
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mVertices.memory, 0, vertexBufferSize, 0, &data));
	return data;
}

bool Mesh::unmapVertexBuffer()
{
	GP_ASSERT(false);
    //return glUnmapBuffer(GL_ARRAY_BUFFER);
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mVertices.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mVertices.buffer, mVertices.memory, 0));
	return true;
}

void Mesh::setVertexData(const void* vertexData, unsigned int vertexStart, unsigned int vertexCount)
{
	void *data;
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mVertices.memory, vertexStart * mVertexFormat.getVertexSize(), vertexCount * mVertexFormat.getVertexSize(), 0, &data));
	memcpy(data, vertexData, vertexCount * mVertexFormat.getVertexSize());
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mVertices.memory);
}

MeshPart* Mesh::addPart(VkPrimitiveTopology primitiveType, IndexFormat indexFormat, unsigned int indexCount, bool dynamic)
{
    MeshPart* part = MeshPart::create(this, mPartCount, primitiveType, indexFormat, indexCount, dynamic);
    if (part)
    {
        // Increase size of part array and copy old subets into it.
        MeshPart** oldParts = mParts;
        mParts = new MeshPart*[mPartCount + 1];
        for (unsigned int i = 0; i < mPartCount; ++i)
        {
            mParts[i] = oldParts[i];
        }

        // Add new part to array.
        mParts[mPartCount++] = part;

        // Delete old part array.
        SAFE_DELETE_ARRAY(oldParts);
    }

    return part;
}

unsigned int Mesh::getPartCount() const
{
    return mPartCount;
}

MeshPart* Mesh::getPart(unsigned int index)
{
    GP_ASSERT(mParts);
    return mParts[index];
}

const BoundingBox& Mesh::getBoundingBox() const
{
    return mBoundingBox;
}

void Mesh::setBoundingBox(const BoundingBox& box)
{
    mBoundingBox = box;
}

const BoundingSphere& Mesh::getBoundingSphere() const
{
    return mBoundingSphere;
}

void Mesh::setBoundingSphere(const BoundingSphere& sphere)
{
    mBoundingSphere = sphere;
}

}
