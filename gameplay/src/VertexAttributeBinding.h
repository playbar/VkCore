#ifndef VERTEXATTRIBUTEBINDING_H_
#define VERTEXATTRIBUTEBINDING_H_

#include "Ref.h"
#include "VertexFormat.h"

namespace vkcore
{

class Mesh;
class Effect;

class VertexAttributeBinding : public Ref
{
public:
 
    static VertexAttributeBinding* create(Mesh* mesh, Effect* effect);

    static VertexAttributeBinding* create(const VertexFormat& vertexFormat, void* vertexPointer, Effect* effect);

    /**
     * Binds this vertex array object.
     */
    void bind();

    /**
     * Unbinds this vertex array object.
     */
    void unbind();

private:

    class VertexAttribute
    {
    public:
        bool enabled;
        int size;
        GLenum type;
        bool normalized;
        unsigned int stride;
        void* pointer;
    };

    /**
     * Constructor.
     */
    VertexAttributeBinding();

    /**
     * Destructor.
     */
    ~VertexAttributeBinding();

    /**
     * Hidden copy assignment operator.
     */
    VertexAttributeBinding& operator=(const VertexAttributeBinding&);

    static VertexAttributeBinding* create(Mesh* mesh, const VertexFormat& vertexFormat, void* vertexPointer, Effect* effect);

    void setVertexAttribPointer(GLuint indx, GLint size, GLenum type, GLboolean normalize, GLsizei stride, void* pointer);

    GLuint _handle;
    VertexAttribute* _attributes;
    Mesh* _mesh;
    Effect* _effect;
};

}

#endif
