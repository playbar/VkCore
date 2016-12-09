#ifndef RENDERSTATE_H_
#define RENDERSTATE_H_

#include "Ref.h"
#include "Vector3.h"
#include "Vector4.h"

namespace vkcore
{

class MaterialParameter;
class Node;
class NodeCloneContext;
class Pass;


class RenderState : public Ref
{
    friend class Game;
    friend class Material;
    friend class Technique;
    friend class Pass;
    friend class Model;

public:


    class AutoBindingResolver
    {
    public:
        virtual ~AutoBindingResolver();
        virtual bool resolveAutoBinding(const char* autoBinding, Node* node, MaterialParameter* parameter) = 0;

    protected:

        AutoBindingResolver();

    };

    /**
     * Built-in auto-bind targets for material parameters.
     */
    enum AutoBinding
    {
        NONE,
        WORLD_MATRIX,
        VIEW_MATRIX,
        PROJECTION_MATRIX,
        WORLD_VIEW_MATRIX,
        VIEW_PROJECTION_MATRIX,
        WORLD_VIEW_PROJECTION_MATRIX,
        INVERSE_TRANSPOSE_WORLD_MATRIX,
        INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX,
        CAMERA_WORLD_POSITION,
        CAMERA_VIEW_POSITION,
        MATRIX_PALETTE,
        SCENE_AMBIENT_COLOR
    };

    /**
     * Defines blend constants supported by the blend function.
     */
    enum Blend
    {
        BLEND_ZERO = GL_ZERO,
        BLEND_ONE = GL_ONE,
        BLEND_SRC_COLOR = GL_SRC_COLOR,
        BLEND_ONE_MINUS_SRC_COLOR = GL_ONE_MINUS_SRC_COLOR,
        BLEND_DST_COLOR = GL_DST_COLOR,
        BLEND_ONE_MINUS_DST_COLOR = GL_ONE_MINUS_DST_COLOR,
        BLEND_SRC_ALPHA = GL_SRC_ALPHA,
        BLEND_ONE_MINUS_SRC_ALPHA = GL_ONE_MINUS_SRC_ALPHA,
        BLEND_DST_ALPHA = GL_DST_ALPHA,
        BLEND_ONE_MINUS_DST_ALPHA = GL_ONE_MINUS_DST_ALPHA,
        BLEND_CONSTANT_ALPHA = GL_CONSTANT_ALPHA,
        BLEND_ONE_MINUS_CONSTANT_ALPHA = GL_ONE_MINUS_CONSTANT_ALPHA,
        BLEND_SRC_ALPHA_SATURATE = GL_SRC_ALPHA_SATURATE
    };
 
    enum DepthFunction
    {
        DEPTH_NEVER = GL_NEVER,
        DEPTH_LESS = GL_LESS,
        DEPTH_EQUAL = GL_EQUAL,
        DEPTH_LEQUAL = GL_LEQUAL,
        DEPTH_GREATER = GL_GREATER,
        DEPTH_NOTEQUAL = GL_NOTEQUAL,
        DEPTH_GEQUAL = GL_GEQUAL,
        DEPTH_ALWAYS = GL_ALWAYS
    };

    enum CullFaceSide
    {
        CULL_FACE_SIDE_BACK = GL_BACK,
        CULL_FACE_SIDE_FRONT = GL_FRONT,
        CULL_FACE_SIDE_FRONT_AND_BACK = GL_FRONT_AND_BACK
    };
  
    enum FrontFace
    {
        FRONT_FACE_CW = GL_CW,
        FRONT_FACE_CCW = GL_CCW
    };

    enum StencilFunction
    {
		STENCIL_NEVER = GL_NEVER,
		STENCIL_ALWAYS = GL_ALWAYS,
		STENCIL_LESS = GL_LESS,
		STENCIL_LEQUAL = GL_LEQUAL,
		STENCIL_EQUAL = GL_EQUAL,
		STENCIL_GREATER = GL_GREATER,
		STENCIL_GEQUAL = GL_GEQUAL,
		STENCIL_NOTEQUAL = GL_NOTEQUAL
    };

    enum StencilOperation
    {
		STENCIL_OP_KEEP = GL_KEEP,
		STENCIL_OP_ZERO = GL_ZERO,
		STENCIL_OP_REPLACE = GL_REPLACE,
		STENCIL_OP_INCR = GL_INCR,
		STENCIL_OP_DECR = GL_DECR,
		STENCIL_OP_INVERT = GL_INVERT,
		STENCIL_OP_INCR_WRAP = GL_INCR_WRAP,
		STENCIL_OP_DECR_WRAP = GL_DECR_WRAP
    };
 
    class StateBlock : public Ref
    {
        friend class RenderState;
        friend class Game;

    public:
        static StateBlock* create();
        void bind();
        void setBlend(bool enabled);
        void setBlendSrc(Blend blend);
        void setBlendDst(Blend blend);
        void setCullFace(bool enabled);
        void setCullFaceSide(CullFaceSide side);
        void setFrontFace(FrontFace winding);
        void setDepthTest(bool enabled);
        void setDepthWrite(bool enabled);
        void setDepthFunction(DepthFunction func);
		void setStencilTest(bool enabled);
		void setStencilWrite(unsigned int mask);
		void setStencilFunction(StencilFunction func, int ref, unsigned int mask);
		void setStencilOperation(StencilOperation sfail, StencilOperation dpfail, StencilOperation dppass);
        void setState(const char* name, const char* value);

    private:
        StateBlock();
        StateBlock(const StateBlock& copy);
        ~StateBlock();

        void bindNoRestore();

        static void restore(long stateOverrideBits);

        static void enableDepthWrite();

        void cloneInto(StateBlock* state);

        // States
        bool _cullFaceEnabled;
        bool _depthTestEnabled;
        bool _depthWriteEnabled;
        DepthFunction _depthFunction;
        bool _blendEnabled;
        Blend _blendSrc;
        Blend _blendDst;
        CullFaceSide _cullFaceSide;
        FrontFace _frontFace;
		bool _stencilTestEnabled;
		unsigned int _stencilWrite;
		StencilFunction _stencilFunction;
		int _stencilFunctionRef;
		unsigned int _stencilFunctionMask;
		StencilOperation _stencilOpSfail;
		StencilOperation _stencilOpDpfail;
		StencilOperation _stencilOpDppass;
        long _bits;

        static StateBlock* _defaultState;
    };

    MaterialParameter* getParameter(const char* name) const;
    unsigned int getParameterCount() const;
    MaterialParameter* getParameterByIndex(unsigned int index);
    void addParameter(MaterialParameter* param);
 
    void removeParameter(const char* name);

    void setParameterAutoBinding(const char* name, AutoBinding autoBinding);

    void setParameterAutoBinding(const char* name, const char* autoBinding);

    void setStateBlock(StateBlock* state);

    StateBlock* getStateBlock() const;

    virtual void setNodeBinding(Node* node);

protected:

    RenderState();

    virtual ~RenderState();

    static void initialize();

    static void finalize();

    void applyAutoBinding(const char* uniformName, const char* autoBinding);

    void bind(Pass* pass);

    RenderState* getTopmost(RenderState* below);

    void cloneInto(RenderState* renderState, NodeCloneContext& context) const;

private:

    RenderState(const RenderState& copy);

    RenderState& operator=(const RenderState&);

    // Internal auto binding handler methods.
    const Matrix& autoBindingGetWorldMatrix() const;
    const Matrix& autoBindingGetViewMatrix() const;
    const Matrix& autoBindingGetProjectionMatrix() const;
    const Matrix& autoBindingGetWorldViewMatrix() const;
    const Matrix& autoBindingGetViewProjectionMatrix() const;
    const Matrix& autoBindingGetWorldViewProjectionMatrix() const;
    const Matrix& autoBindingGetInverseTransposeWorldMatrix() const;
    const Matrix& autoBindingGetInverseTransposeWorldViewMatrix() const;
    Vector3 autoBindingGetCameraWorldPosition() const;
    Vector3 autoBindingGetCameraViewPosition() const;
    const Vector4* autoBindingGetMatrixPalette() const;
    unsigned int autoBindingGetMatrixPaletteSize() const;
    const Vector3& autoBindingGetAmbientColor() const;
    const Vector3& autoBindingGetLightColor() const;
    const Vector3& autoBindingGetLightDirection() const;

protected:

    mutable std::vector<MaterialParameter*> _parameters;

    std::map<std::string, std::string> _autoBindings;
  
    Node* _nodeBinding;

    mutable StateBlock* _state;

    RenderState* _parent;

    static std::vector<AutoBindingResolver*> _customAutoBindingResolvers;
};

}

#include "MaterialParameter.h"

#endif
