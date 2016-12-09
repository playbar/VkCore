#ifndef EFFECT_H_
#define EFFECT_H_

#include "Ref.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix.h"
#include "array"
#include "Texture.h"
#include "vulkan/vulkan.h"

namespace vkcore
{

class Uniform;

class Effect: public Ref
{
public:

    static Effect* createFromFile(const char* vshPath, const char* fshPath, const char* defines = NULL);
 
    static Effect* createFromSource(const char* vshSource, const char* fshSource, const char* defines = NULL);

    const char* getId() const;

    VertexAttribute getVertexAttribute(const char* name) const;

    Uniform* getUniform(const char* name) const;

    Uniform* getUniform(unsigned int index) const;

    unsigned int getUniformCount() const;

    void setValue(Uniform* uniform, float value);

    void setValue(Uniform* uniform, const float* values, unsigned int count = 1);

    void setValue(Uniform* uniform, int value);

    void setValue(Uniform* uniform, const int* values, unsigned int count = 1);

    void setValue(Uniform* uniform, const Matrix& value);

    void setValue(Uniform* uniform, const Matrix* values, unsigned int count = 1);

    void setValue(Uniform* uniform, const Vector2& value);

    void setValue(Uniform* uniform, const Vector2* values, unsigned int count = 1);

    void setValue(Uniform* uniform, const Vector3& value);

    void setValue(Uniform* uniform, const Vector3* values, unsigned int count = 1);

    void setValue(Uniform* uniform, const Vector4& value);

    void setValue(Uniform* uniform, const Vector4* values, unsigned int count = 1);

    void setValue(Uniform* uniform, const Texture::Sampler* sampler);

    void setValue(Uniform* uniform, const Texture::Sampler** values, unsigned int count);

    void bind();

    static Effect* getCurrentEffect();

private:
    Effect();
    ~Effect();
    Effect& operator=(const Effect&);

    static Effect* createFromSource(const char* vshPath, const char* vshSource, const char* fshPath, const char* fshSource, const char* defines = NULL);

    GLuint _program;
    std::string _id;
    std::map<std::string, VertexAttribute> _vertexAttributes;
    mutable std::map<std::string, Uniform*> _uniforms;
    static Uniform _emptyUniform;

	VkPipelineLayout mPipelineLayout;
	VkDescriptorSetLayout mDescriptorSetLayout;
	std::vector<VkShaderModule> shaderModules;
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
};

/**
 * Represents a uniform variable within an effect.
 */
class Uniform
{
    friend class Effect;

public:

    /**
     * Returns the name of this uniform.
     */
    const char* getName() const;

    /**
     * Returns the OpenGL uniform type.
     */
    const GLenum getType() const;

    /**
     * Returns the effect for this uniform.
     */
    Effect* getEffect() const;

private:
    Uniform();

    Uniform(const Uniform& copy);
    
    ~Uniform();

    Uniform& operator=(const Uniform&);

    std::string _name;
    GLint _location;
    GLenum _type;
    unsigned int _index;
    Effect* _effect;

};

}

#endif
