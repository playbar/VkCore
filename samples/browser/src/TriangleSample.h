#ifndef TRIANGLESAMPLE_H_
#define TRIANGLESAMPLE_H_

#include "gameplay.h"
#include "Sample.h"

using namespace vkcore;

/**
 * Sample creating and draw a single triangle.
 */
class TriangleSample : public Sample
{

	struct Vertex
	{
		float position[3];
		float color[3];
	};

public:
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


	// For simplicity we use the same uniform block layout as in the shader:
	//	layout(set = 0, binding = 0) uniform UBO
	//	{
	//		mat4 projectionMatrix;
	//		mat4 modelMatrix;
	//		mat4 viewMatrix;
	//	} ubo;
	//
	// This way we can just memcopy the ubo data to the ubo
	// Note: You should use data types that align with the GPU in order to avoid manual padding (vec4, mat4)
	struct
	{
		vkcore::Matrix projectionMatrix;
		vkcore::Matrix modelMatrix;
		vkcore::Matrix viewMatrix;
	} mUboVS;

	// Uniform block object
	struct
	{
		VkDeviceMemory memory;
		VkBuffer buffer;
		VkDescriptorBufferInfo descriptor;
	}  mUniformDataVS;

	VkPipelineLayout mPipelineLayout;
	VkPipeline mPipeline;
	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorSet mDescriptorSet;

	VkSemaphore presentCompleteSemaphore;
	VkSemaphore renderCompleteSemaphore;

	std::vector<VkFence> mWaitFences;

	void Init();

	void UnInit();

	void prepareSynchronizationPrimitives();

	VkCommandBuffer getCommandBuffer(bool begin);

	void flushCommandBuffer(VkCommandBuffer commandBuffer);
	
	void buildCommandBuffers();

	void draw();

	void updateUniformBuffers();

	void setupDescriptorPool();

	void setupDescriptorSetLayout();

	void setupDescriptorSet();

	void setupDepthStencil();

	void setupFrameBuffer();

	void setupRenderPass();

	void preparePipelines();

	void prepareUniformBuffers();

	void prepare();

	virtual void render();

	virtual void viewChanged();

	////////////////////////////////////////////////
public:

    TriangleSample();
	~TriangleSample();

    void touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);

protected:

    void initialize();

    void finalize();

    void update(float elapsedTime);

    void render(float elapsedTime);

private:

    //Font* _font;
    Model* _model;
    float _spinDirection;
    Matrix _worldViewProjectionMatrix;
};

#endif
