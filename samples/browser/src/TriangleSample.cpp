#include "TriangleSample.h"
#include "SamplesGame.h"
#include "VkCoreDevice.hpp"

#if defined(ADD_SAMPLE)
    ADD_SAMPLE("Graphics", "Triangle", TriangleSample, 1);
#endif


TriangleSample game;

/**
 * Creates a triangle mesh with vertex colors.
 */
static Mesh* createTriangleMesh()
{
    // Calculate the vertices of the equilateral triangle.
    float a = 1.0f;     // length of the side
    Vector2 p1(0.0f,       a / sqrtf(3.0f));
    Vector2 p2(-a / 2.0f, -a / (2.0f * sqrtf(3.0f)));
    Vector2 p3( a / 2.0f, -a / (2.0f * sqrtf(3.0f)));

    // Create 3 vertices. Each vertex has position (x, y, z) and color (red, green, blue)
    float vertices[] =
    {
        p1.x, p1.y, 0.0f,     1.0f, 0.0f, 0.0f,
        p2.x, p2.y, 0.0f,     0.0f, 1.0f, 0.0f,
        p3.x, p3.y, 0.0f,     0.0f, 0.0f, 1.0f,
    };

	int indexBuffer[] = { 1, 0, 2 };
    unsigned int vertexCount = 3;
    VertexFormat::Element elements[] =
    {
        VertexFormat::Element(VertexFormat::POSITION, 3),
        VertexFormat::Element(VertexFormat::COLOR, 3)
    };
    Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 2), vertexCount, false);
	
    if (mesh == NULL)
    {
        GP_ERROR("Failed to create mesh.");
        return NULL;
    }
    mesh->setPrimitiveType(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    mesh->setVertexData(vertices, 0, vertexCount);
	MeshPart *meshPart = mesh->addPart(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, Mesh::INDEX32, vertexCount);
	meshPart->setIndexData(indexBuffer, 0, vertexCount);
    return mesh;
}


void TriangleSample::Init()
{
	width = 800;
	height = 600;
	mZoom = -10.0f;
	title = "VkCore";
}

void TriangleSample::prepare()
{
	Game::prepare();
	//prepareSynchronizationPrimitives();
	////prepareVertices(true);
	//prepareUniformBuffers();
	//setupDescriptorSetLayout();
	//preparePipelines();
	//setupDescriptorPool();
	//setupDescriptorSet();
	//buildCommandBuffers();
	prepared = true;
}

void TriangleSample::render()
{
	if (!prepared)
		return;
	Game::render();
}

TriangleSample::TriangleSample() :
	_model(NULL),
	_spinDirection(-1.0f)
{
	//_font = NULL;
	Init();
}

TriangleSample::~TriangleSample()
{
}

void TriangleSample::initialize()
{
    // Create the font for drawing the framerate.
    //_font = Font::create("res/ui/arial.gpb");

    // Create an orthographic projection matrix.
    float width = getWidth() / (float)getHeight();
    float height = 1.0f;
    Matrix::createOrthographic(width, height, -1.0f, 1.0f, &_worldViewProjectionMatrix);

    // Create the triangle mesh.
    Mesh* mesh = createTriangleMesh();

    _model = Model::create(mesh);
    SAFE_RELEASE(mesh);

    _model->setMaterial("shaders/triangle.vert", "shaders/triangle.frag" );
	_model->getMaterial()->prepareUniformBuffers();
	Matrix model, view;
	_model->getMaterial()->updateUniformBuffers(&_worldViewProjectionMatrix, &model, &view);
	_model->preparePipelines();
	_model->setupDescriptorPool();
	_model->setupDescriptorSet();
	_model->buildCommandBuffers();

}

void TriangleSample::finalize()
{
    SAFE_RELEASE(_model);
}

void TriangleSample::update(float elapsedTime)
{
    _worldViewProjectionMatrix.rotateZ( _spinDirection * MATH_PI * elapsedTime * 0.001f);
}

void TriangleSample::render(float elapsedTime)
{
	Game::prepareFrame();
    
	_model->getMaterial()->updateUniformProMat(&_worldViewProjectionMatrix);
    _model->draw();

    //drawFrameRate(_font, Vector4(0, 0.5f, 1, 1), 5, 1, getFrameRate());
	Game::submitFrame();
}

void TriangleSample::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex)
{
    switch (evt)
    {
    case Touch::TOUCH_PRESS:
        if (x < 75 && y < 50)
        {
            // Toggle Vsync if the user touches the top left corner
            setVsync(!isVsync());
        }
        else
        {
            // Reverse the spin direction if the user touches the screen.
            _spinDirection *= -1.0f;
        }
        break;
    case Touch::TOUCH_RELEASE:
        break;
    case Touch::TOUCH_MOVE:
        break;
    };
}
