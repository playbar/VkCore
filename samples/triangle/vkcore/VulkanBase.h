#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#elif defined(__ANDROID__)
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include "vulkanandroid.h"
#elif defined(__linux__)
#include <xcb/xcb.h>
#endif

#include <iostream>
#include <chrono>
#include "define.h"
#include <glm/glm.hpp>
#include <string>
#include <array>

#include "vulkan/vulkan.h"

#include "Keyboard.h"
#include "vulkantools.h"
#include "vulkandebug.h"
#include "VkCoreDevice.hpp"
#include "vulkanswapchain.hpp"
#include "vulkanTextureLoader.hpp"
#include "vulkanMeshLoader.hpp"
#include "vulkantextoverlay.hpp"
#include "VkCamera.hpp"
#include "Vector2.h"
#include "define.h"

#define ENABLE_VALIDATION false
#define USE_STAGING false


// Function pointer for getting physical device fetures to be enabled
typedef VkPhysicalDeviceFeatures (*PFN_GetEnabledFeatures)();

class VulkanBase
{
public:
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

	// The pipeline layout is used by a pipline to access the descriptor sets 
	// It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
	// A pipeline layout can be shared among multiple pipelines as long as their interfaces match
	VkPipelineLayout mPipelineLayout;

	// Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
	// While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
	// So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
	// Even though this adds a new dimension of planing ahead, it's a great opportunity for performance optimizations by the driver
	VkPipeline mPipeline;

	// The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
	// Like the pipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
	VkDescriptorSetLayout mDescriptorSetLayout;

	// The descriptor set stores the resources bound to the binding points in a shader
	// It connects the binding points of the different shaders with the buffers and images used for those bindings
	VkDescriptorSet mDescriptorSet;

	uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties);

	VkCommandBuffer getCommandBuffer(bool begin);

	void flushCommandBuffer(VkCommandBuffer commandBuffer);

	void buildCommandBuffers();

	void draw();

	void updateUniformBuffers();

	void prepareVertices(bool useStagingBuffers);

	void setupDescriptorPool();

	void setupDescriptorSetLayout();

	void setupDescriptorSet();

	void setupDepthStencil();

	void setupFrameBuffer();

	void setupRenderPass();

	void preparePipelines();

	void prepareUniformBuffers();

	virtual void render();

	virtual void viewChanged();

private:	
	// Set to true when example is created with enabled validation layers
	bool mEnableValidation = false;
	// Set to true if v-sync will be forced for the swapchain
	bool enableVSync = false;	
	// fps timer (one second interval)
	float fpsTimer = 0.0f;

	/** brief Indicates that the view (position, rotation) has changed and */
	bool viewUpdated = false;
	// Destination dimensions for resizing the window
	uint32_t destWidth;
	uint32_t destHeight;
	bool resizing = false;

protected:
	// Last frame time, measured using a high performance timer (if available)
	float frameTimer = 1.0f;
	// Frame counter to display fps
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	// Vulkan instance, stores all per-application states
	VkInstance mInstance;

	// Device features enabled by the example
	// If not set, no additional features are enabled (may result in validation layer errors)
	VkPhysicalDeviceFeatures enabledFeatures = {};

	// Color buffer format
	VkFormat mColorformat = VK_FORMAT_B8G8R8A8_UNORM;
	// Depth buffer format
	// Depth format is selected during Vulkan initialization
	VkFormat mDepthFormat;

	VkCommandPool mCmdPool;
	// Command buffer used for setup
	//VkCommandBuffer setupCmdBuffer = VK_NULL_HANDLE;
	/** @brief Pipeline stages used to wait at for graphics queue submissions */
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// Contains command buffers and semaphores to be presented to the queue
	// Command buffers used for rendering
	std::vector<VkCommandBuffer> mDrawCmdBuffers;
	// Global render pass for frame buffer writes
	VkRenderPass mRenderPass;
	// List of available frame buffers (same as number of swap chain images)
	std::vector<VkFramebuffer>mFrameBuffers;

	// Descriptor set pool
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> shaderModules;
	// Pipeline cache object
	VkPipelineCache pipelineCache;
	
public: 
	bool prepared = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	float mZoom = 0;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	
	bool paused = false;

	// Use to adjust mouse rotation speed
	float rotationSpeed = 0.5f;
	// Use to adjust mouse zoom speed
	float zoomSpeed = 1.0f;

	VkCamera mCamera;

	Vector3 mRotation;
	Vector3 cameraPos;
	Vector2 mousePos;

	std::string title = "VkCore";
	std::string name = "VkCore";

	struct 
	{
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} depthStencil;

	// Gamepad state (only one pad supported)
	struct
	{
		glm::vec2 axisLeft = glm::vec2(0.0f);
		glm::vec2 axisRight = glm::vec2(0.0f);
	} gamePadState;

	// OS specific 
#if defined(_WIN32)
	HWND mHwndWinow;
	HINSTANCE mWindowInstance;
#elif defined(__ANDROID__)
	android_app* androidApp;
	// true if application has focused, false if moved to background
	bool focused = false;
#elif defined(__linux__)
	struct {
		bool left = false;
		bool right = false;
		bool middle = false;
	} mouseButtons;
	bool quit = false;
	xcb_connection_t *connection;
	xcb_screen_t *screen;
	xcb_window_t mHwndWinow;
	xcb_intern_atom_reply_t *atom_wm_delete_window;
#endif

	VulkanBase(bool enableValidation, PFN_GetEnabledFeatures enabledFeaturesFn = nullptr);

	~VulkanBase();

	// Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
	void initVulkan(bool enableValidation);

	// Called if the window is resized and some resources have to be recreatesd
	void windowResize();
	VkResult createInstance(bool enableValidation);
	std::string getWindowTitle();
	const std::string getAssetPath();

	void setupConsole(std::string title);
	HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc);
	void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


	virtual void keyPressed(uint32_t keyCode);

	virtual void windowResized();
	// Pure virtual function to be overriden by the dervice class
	// Called in case of an event where e.g. the framebuffer has to be rebuild and thus
	// all command buffers that may reference this

	// Creates a new (graphics) command pool object storing command buffers
	void createCommandPool();

	// Connect and prepare the swap chain
	void initSwapchain();
	// Create swap chain images
	void setupSwapChain();

	// Check if command buffers are valid (!= VK_NULL_HANDLE)
	bool checkCommandBuffers();
	// Create command buffers for drawing commands
	void createCommandBuffers();
	// Destroy all command buffers and set their handles to VK_NULL_HANDLE
	// May be necessary during runtime if options are toggled 
	void destroyCommandBuffers();
	
	// Command buffer creation
	// Creates and returns a new command buffer
	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
	// End the command buffer, submit it to the queue and free (if requested)
	// Note : Waits for the queue to become idle
	void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

	// Create a cache pool for rendering pipelines
	void createPipelineCache();

	// Prepare commonly used Vulkan functions
	virtual void prepare();

	// Load a SPIR-V shader
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);
	
	// Load a mesh (using ASSIMP) and create vulkan vertex and index buffers with given vertex layout
	void loadMesh(
		std::string fiename, 
		vkMeshLoader::MeshBuffer *meshBuffer, 
		std::vector<vkMeshLoader::VertexLayout> vertexLayout, 
		float scale);
	void loadMesh(
		std::string filename, 
		vkMeshLoader::MeshBuffer *meshBuffer, 
		std::vector<vkMeshLoader::VertexLayout> 
		vertexLayout, 
		vkMeshLoader::MeshCreateInfo *meshCreateInfo);

	// Start the main render loop
	void renderLoop();


};
