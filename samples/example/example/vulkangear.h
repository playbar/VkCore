#pragma once

#include <math.h>
#include <vector>
#include "define.h"

#include "vulkan/vulkan.h"

#include "vulkantools.h"
#include "VkCoreDevice.hpp"
#include "vulkanbuffer.hpp"
#include "Vector3.h"

using namespace vkcore;

struct VertexGear
{
	float pos[3];
	float normal[3];
	float color[3];

	VertexGear(const Vector3& p, const Vector3& n, const Vector3& c)
	{
		pos[0] = p.x;
		pos[1] = p.y;
		pos[2] = p.z;
		color[0] = c.x;
		color[1] = c.y;
		color[2] = c.z;
		normal[0] = n.x;
		normal[1] = n.y;
		normal[2] = n.z;
	}
};

struct GearInfo
{
	float innerRadius;
	float outerRadius;
	float width;
	int numTeeth;
	float toothDepth;
	Vector3 color;
	Vector3 pos;
	float rotSpeed;
	float rotOffset;
};

class VulkanGear
{
private:
	struct UBO
	{
		Matrix projection;
		Matrix model;
		Matrix normal;
		Matrix view;
		Vector3 lightPos;
	};

	VkCoreDevice *vulkanDevice;

	Vector3 color;
	Vector3 pos;
	float rotSpeed;
	float rotOffset;

	vk::Buffer vertexBuffer;
	vk::Buffer indexBuffer;
	uint32_t indexCount;

	UBO ubo;
	vkTools::UniformData uniformData;

	int32_t newVertex(std::vector<VertexGear> *vBuffer, float x, float y, float z, const Vector3& normal);
	void newFace(std::vector<uint32_t> *iBuffer, int a, int b, int c);

	void prepareUniformBuffer();
public:
	VkDescriptorSet descriptorSet;

	void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout);
	void updateUniformBuffer(Matrix perspective, Vector3 rotation, float zoom, float timer);

	void setupDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout);

	VulkanGear(VkCoreDevice *vulkanDevice) : vulkanDevice(vulkanDevice) {};
	~VulkanGear();

	void generate(GearInfo *gearinfo, VkQueue queue);

};

