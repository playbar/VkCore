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

private:

    Model();

    Model(Mesh* mesh);

    ~Model();

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
};

}

#endif
