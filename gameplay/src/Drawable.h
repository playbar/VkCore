#ifndef DRAWABLE_H_
#define DRAWABLE_H_

namespace vkcore
{

class Node;
class NodeCloneContext;

/**
 * Defines a drawable object that can be attached to a Node.
 */
class Drawable
{
    friend class Node;

public:
    Drawable();
    virtual ~Drawable();
    virtual unsigned int draw(bool wireframe = false) = 0;
    Node* getNode() const;

protected:
    virtual Drawable* clone(NodeCloneContext& context) = 0;
    virtual void setNode(Node* node);
    Node* _node;
};

}

#endif
