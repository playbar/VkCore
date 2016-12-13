#ifndef MODEL_H_
#define MODEL_H_

#include "Mesh.h"
#include "MeshSkin.h"
#include "Material.h"
#include "Drawable.h"

namespace vkcore
{

class Bundle;
class MeshSkin;

class Model : public Ref, public Drawable
{
    friend class Node;
    friend class Scene;
    friend class Mesh;
    friend class Bundle;

public:

    static Model* create(Mesh* mesh);
    Mesh* getMesh() const;
    unsigned int getMeshPartCount() const;
  
    Material* getMaterial(int partIndex = -1);
 
    void setMaterial(Material* material, int partIndex = -1);

    Material* setMaterial(const char* vshPath, const char* fshPath, const char* defines = NULL, int partIndex = -1);
  
    Material* setMaterial(const char* materialPath, int partIndex = -1);

    bool hasMaterial(unsigned int partIndex) const;

    MeshSkin* getSkin() const;

    unsigned int draw(bool wireframe = false);
	
	void prepare();
	void setupDepthStencil();
	void createPipelineCache();
	void setupDescriptorPool();
	void setupDescriptorSet();
	void setupDescriptorSetLayout();
	bool checkCommandBuffers();
	void createCommandPool();
	void createCommandBuffers();
	void buildCommandBuffers();
	void destroyCommandBuffers();
	void setupRenderPass();
	void setupFrameBuffer();
	void preparePipelines();
	virtual void prepareUniformBuffers();
	void updateUniformBuffers();

	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);


private:

    Model();

    Model(Mesh* mesh);

    ~Model();

	void UninitVulkan();

    Model& operator=(const Model&);
    
    void setNode(Node* node);

    Drawable* clone(NodeCloneContext& context);

    void setSkin(MeshSkin* skin);

    void setMaterialNodeBinding(Material *m);

    void validatePartCount();


    Mesh* _mesh;
    Material* _material;
    unsigned int _partCount;
    Material** _partMaterials;
    MeshSkin* _skin;
	
	//////////////////////////

	uint32_t width = 1280;
	uint32_t height = 720;
	struct
	{
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} depthStencil;

	// Uniform block object
	struct
	{
		VkDeviceMemory memory;
		VkBuffer buffer;
		VkDescriptorBufferInfo descriptor;
	}  mUniformDataVS;

	// For simplicity we use the same uniform block layout as in the shader:
	//	layout(set = 0, binding = 0) uniform UBO
	//	{
	//		mat4 projectionMatrix;
	//		mat4 modelMatrix;
	//		mat4 viewMatrix;
	//	} ubo;
	struct
	{
		vkcore::Matrix projectionMatrix;
		vkcore::Matrix modelMatrix;
		vkcore::Matrix viewMatrix;
	} mUboVS;

	VkFormat mColorformat = VK_FORMAT_B8G8R8A8_UNORM;
	VkFormat mDepthFormat;
	VkFence mFence;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorSet mDescriptorSet;
	VkPipelineLayout mPipelineLayout;
	VkPipeline mPipeline;
	VkRenderPass mRenderPass;
	VkCommandPool mCmdPool;
	VkPipelineCache pipelineCache;

	std::vector<VkShaderModule> shaderModules;
	std::vector<VkFramebuffer>mFrameBuffers;
	std::vector<VkCommandBuffer> mDrawCmdBuffers;

};

}

#endif
