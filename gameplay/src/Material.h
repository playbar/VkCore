#ifndef MATERIAL_H_
#define MATERIAL_H_

#include "RenderState.h"
#include "Technique.h"
#include "Properties.h"

namespace vkcore
{

class NodeCloneContext;


class Material : public RenderState
{
    friend class RenderState;
    friend class Node;
    friend class Model;

public:

    typedef std::string(*PassCallback)(Pass*, void*);


    static Material* create(const char* url);

    static Material* create(const char* url, PassCallback callback, void* cookie = NULL);
 
    static Material* create(Properties* materialProperties);

    static Material* create(const char* vshPath, const char* fshPath, const char* defines = NULL);

    void setNodeBinding(Node* node);

	void createPipelineLayout();

	VkPipelineLayout mPipelineLayout;
	VkDescriptorSetLayout mDescriptorSetLayout;
	std::vector<VkShaderModule> shaderModules;
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

private:

    Material();

    Material(const Material& m);

    ~Material();

    Material* clone(NodeCloneContext &context) const;
  
    static Material* create(Properties* materialProperties, PassCallback callback, void* cookie);

    static bool loadPass(Technique* technique, Properties* passProperites, PassCallback callback, void* cookie);

    static void loadRenderState(RenderState* renderState, Properties* properties);

};

}

#endif
