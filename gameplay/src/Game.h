#ifndef GAME_H_
#define GAME_H_

#include "Keyboard.h"
#include "Mouse.h"
#include "Touch.h"
#include "Gesture.h"
#include "Gamepad.h"
#include "AudioController.h"
#include "AnimationController.h"
#include "PhysicsController.h"
#include "AIController.h"
#include "AudioListener.h"
#include "Rectangle.h"
#include "Vector4.h"
#include "TimeListener.h"

#include "vulkan/vulkan.h"
#include "vulkantools.h"
#include "vulkandebug.h"
#include "VkCoreDevice.hpp"
#include "vulkanswapchain.hpp"
#include "vulkanTextureLoader.hpp"
#include "vulkanMeshLoader.hpp"
#include "vulkantextoverlay.hpp"
#include "VkCamera.hpp"
#include "Vector2.h"


namespace vkcore
{


class ScriptController;

typedef VkPhysicalDeviceFeatures(*PFN_GetEnabledFeatures)();

extern VkCoreDevice *mVulkanDevice;

class Game
{
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

	// Handle to the device graphics queue that command buffers are submitted to
	VkQueue mQueue;
	// Color buffer format
	VkFormat mColorformat = VK_FORMAT_B8G8R8A8_UNORM;
	// Depth buffer format
	// Depth format is selected during Vulkan initialization
	VkFormat mDepthFormat;
	// Command buffer pool
	VkCommandPool mCmdPool;
	// Command buffer used for setup
	VkCommandBuffer setupCmdBuffer = VK_NULL_HANDLE;
	/** @brief Pipeline stages used to wait at for graphics queue submissions */
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// Contains command buffers and semaphores to be presented to the queue
	VkSubmitInfo mSubmitInfo;
	// Command buffers used for rendering
	std::vector<VkCommandBuffer> mDrawCmdBuffers;
	// Global render pass for frame buffer writes
	VkRenderPass mRenderPass;
	// List of available frame buffers (same as number of swap chain images)
	std::vector<VkFramebuffer>mFrameBuffers;
	// Active frame buffer index
	uint32_t mCurrentBuffer = 0;
	// Descriptor set pool
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> shaderModules;
	// Pipeline cache object
	VkPipelineCache pipelineCache;
	// Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanSwapChain mSwapChain;
	// Synchronization semaphores
	struct
	{
		// Swap chain image presentation
		VkSemaphore presentComplete;
		// Command buffer submission and execution
		VkSemaphore renderComplete;
		// Text overlay submission and execution
		VkSemaphore textOverlayComplete;
	} semaphores;
	// Simple texture loader
	vkTools::VulkanTextureLoader *textureLoader = nullptr;
	// Returns the base asset path (for shaders, models, textures) depending on the os

public:
	bool prepared = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	float mZoom = 0;

	static std::vector<const char*> args;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;

	bool paused = false;

	bool mEnableTextOverlay = true;
	VulkanTextOverlay *mTextOverlay;

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

	void InitVulkanBase(bool enableValidation, PFN_GetEnabledFeatures enabledFeaturesFn = nullptr);

	void UnInitVulkan();

	// Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
	void InitVulkan(bool enableValidation);

	// Called if the window is resized and some resources have to be recreatesd
	void windowResize();
	VkResult createInstance(bool enableValidation);
	std::string getWindowTitle();
	const std::string getAssetPath();

#if defined(_WIN32)
	void setupConsole(std::string title);
	HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc);
	void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#elif defined(__ANDROID__)
	static int32_t handleAppInput(struct android_app* app, AInputEvent* event);
	static void handleAppCommand(android_app* app, int32_t cmd);
#elif defined(__linux__)
	xcb_window_t setupWindow();
	void initxcbConnection();
	void handleEvent(const xcb_generic_event_t *event);
#endif
	virtual void render();

	virtual void viewChanged();

	virtual void keyPressed(uint32_t keyCode);

	virtual void windowResized();
	// Pure virtual function to be overriden by the dervice class
	// Called in case of an event where e.g. the framebuffer has to be rebuild and thus
	// all command buffers that may reference this
	virtual void buildCommandBuffers();

	// Creates a new (graphics) command pool object storing command buffers
	void createCommandPool();
	// Setup default depth and stencil views
	virtual void setupDepthStencil();
	// Create framebuffers for all requested swap chain images
	// Can be overriden in derived class to setup a custom framebuffer (e.g. for MSAA)
	virtual void setupFrameBuffer();
	// Setup a default render pass
	// Can be overriden in derived class to setup a custom render pass (e.g. for MSAA)
	virtual void setupRenderPass();

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
	// Create command buffer for setup commands
	void createSetupCommandBuffer();
	// Finalize setup command bufferm submit it to the queue and remove it
	void flushSetupCommandBuffer();

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

	// Create a buffer, fill it with data (if != NULL) and bind buffer memory
	VkBool32 createBuffer(
		VkBufferUsageFlags usageFlags,
		VkMemoryPropertyFlags memoryPropertyFlags,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory);
	// This version always uses HOST_VISIBLE memory
	VkBool32 createBuffer(
		VkBufferUsageFlags usage,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory);
	// Overload that assigns buffer info to descriptor
	VkBool32 createBuffer(
		VkBufferUsageFlags usage,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory,
		VkDescriptorBufferInfo *descriptor);
	// Overload to pass memory property flags
	VkBool32 createBuffer(
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags memoryPropertyFlags,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory,
		VkDescriptorBufferInfo *descriptor);

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

	void updateTextOverlay();

	// Called when the text overlay is updating
	// Can be overriden in derived class to add custom text to the overlay
	virtual void getOverlayText(VulkanTextOverlay * textOverlay);

	// Prepare the frame for workload submission
	// - Acquires the next image from the swap chain 
	// - Sets the default wait and signal semaphores
	void prepareFrame();

	// Submit the frames' workload 
	// - Submits the text overlay (if enabled)
	void submitFrame();

public:
	////////////////////////////////////////////////////
    friend class Platform;
    friend class Gamepad;
    friend class ShutdownListener;

public:
    
    /**
     * The game states.
     */
    enum State
    {
        UNINITIALIZED,
        RUNNING,
        PAUSED
    };

    /**
     * Flags used when clearing the active frame buffer targets.
     */
    enum ClearFlags
    {
        CLEAR_COLOR = GL_COLOR_BUFFER_BIT,
        CLEAR_DEPTH = GL_DEPTH_BUFFER_BIT,
        CLEAR_STENCIL = GL_STENCIL_BUFFER_BIT,
        CLEAR_COLOR_DEPTH = CLEAR_COLOR | CLEAR_DEPTH,
        CLEAR_COLOR_STENCIL = CLEAR_COLOR | CLEAR_STENCIL,
        CLEAR_DEPTH_STENCIL = CLEAR_DEPTH | CLEAR_STENCIL,
        CLEAR_COLOR_DEPTH_STENCIL = CLEAR_COLOR | CLEAR_DEPTH | CLEAR_STENCIL
    };

    /**
     * Constructor.
     */
    Game();


	Game(bool enableValidation, PFN_GetEnabledFeatures enabledFeaturesFn);

    /**
     * Destructor.
     */
    virtual ~Game();

    /**
     * Gets the single instance of the game.
     * 
     * @return The single instance of the game.
     */
    static Game* getInstance();

    /**
     * Gets whether vertical sync is enabled for the game display.
     * 
     * @return true if vsync is enabled; false if not.
     */
    static bool isVsync();

    /**
     * Sets whether vertical sync is enabled for the game display.
     *
     * @param enable true if vsync is enabled; false if not.
     */
    static void setVsync(bool enable);

    /**
     * Gets the total absolute running time (in milliseconds) since Game::run().
     * 
     * @return The total absolute running time (in milliseconds).
     */
    static double getAbsoluteTime();

    /**
     * Gets the total game time (in milliseconds). This is the total accumulated game time (unpaused).
     *
     * You would typically use things in your game that you want to stop when the game is paused.
     * This includes things such as game physics and animation.
     * 
     * @return The total game time (in milliseconds).
     */
    static double getGameTime();

    /**
     * Gets the game state.
     *
     * @return The current game state.
     */
    inline State getState() const;

    /**
     * Determines if the game has been initialized.
     *
     * @return true if the game initialization has completed, false otherwise.
     */
    inline bool isInitialized() const;

    /**
     * Returns the game configuration object.
     *
     * This method returns a Properties object containing the contents
     * of the game.config file.
     *
     * @return The game configuration Properties object.
     */
    Properties* getConfig() const;

    /**
     * Called to initialize the game, and begin running the game.
     * 
     * @return Zero for normal termination, or non-zero if an error occurred.
     */
    int run();

    /**
     * Pauses the game after being run.
     */
    void pause();

    /**
     * Resumes the game after being paused.
     */
    void resume();

    /**
     * Exits the game.
     */
    void exit();

    /**
     * Platform frame delegate.
     *
     * This is called every frame from the platform.
     * This in turn calls back on the user implemented game methods: update() then render()
     */
    void frame();

    /**
     * Gets the current frame rate.
     * 
     * @return The current frame rate.
     */
    inline unsigned int getFrameRate() const;

    /**
     * Gets the game window width.
     * 
     * @return The game window width.
     */
    inline unsigned int getWidth() const;

    /**
     * Gets the game window height.
     * 
     * @return The game window height.
     */
    inline unsigned int getHeight() const;
    
    /**
     * Gets the aspect ratio of the window. (width / height)
     * 
     * @return The aspect ratio of the window.
     */
    inline float getAspectRatio() const;

    /**
     * Gets the game current viewport.
     *
     * The default viewport is Rectangle(0, 0, Game::getWidth(), Game::getHeight()).
     */
    inline const VRectangle& getViewport() const;

    /**
     * Sets the game current viewport.
     *
     * The x, y, width and height of the viewport must all be positive.
     *
     * viewport The custom viewport to be set on the game.
     */
    void setViewport(const VRectangle& viewport);

    /**
     * Clears the specified resource buffers to the specified clear values. 
     *
     * @param flags The flags indicating which buffers to be cleared.
     * @param clearColor The color value to clear to when the flags includes the color buffer.
     * @param clearDepth The depth value to clear to when the flags includes the color buffer.
     * @param clearStencil The stencil value to clear to when the flags includes the color buffer.
     */
    void clear(ClearFlags flags, const Vector4& clearColor, float clearDepth, int clearStencil);

    /**
     * Clears the specified resource buffers to the specified clear values. 
     * 
     * @param flags The flags indicating which buffers to be cleared.
     * @param red The red channel.
     * @param green The green channel.
     * @param blue The blue channel.
     * @param alpha The alpha channel.
     * @param clearDepth The depth value to clear to when the flags includes the color buffer.
     * @param clearStencil The stencil value to clear to when the flags includes the color buffer.
     */
    void clear(ClearFlags flags, float red, float green, float blue, float alpha, float clearDepth, int clearStencil);

    /**
     * Gets the audio controller for managing control of audio
     * associated with the game.
     *
     * @return The audio controller for this game.
     */
    inline AudioController* getAudioController() const;

    /**
     * Gets the animation controller for managing control of animations
     * associated with the game.
     * 
     * @return The animation controller for this game.
     */
    inline AnimationController* getAnimationController() const;

    /**
     * Gets the physics controller for managing control of physics
     * associated with the game.
     * 
     * @return The physics controller for this game.
     */
    inline PhysicsController* getPhysicsController() const;

    /** 
     * Gets the AI controller for managing control of artificial
     * intelligence associated with the game.
     *
     * @return The AI controller for this game.
     */
    inline AIController* getAIController() const;

    /**
     * Gets the script controller for managing control of Lua scripts
     * associated with the game.
     * 
     * @return The script controller for this game.
     */
    inline ScriptController* getScriptController() const;

    /**
     * Gets the audio listener for 3D audio.
     * 
     * @return The audio listener for this game.
     */
    AudioListener* getAudioListener();
    
    /**
     * Shows or hides the virtual keyboard (if supported).
     *
     * @param display true when virtual keyboard needs to be displayed; false otherwise.
     */
     inline void displayKeyboard(bool display);
     
    /**
     * Keyboard callback on keyPress events.
     *
     * @param evt The key event that occurred.
     * @param key If evt is KEY_PRESS or KEY_RELEASE then key is the key code from Keyboard::Key.
     *            If evt is KEY_CHAR then key is the unicode value of the character.
     * 
     * @see Keyboard::KeyEvent
     * @see Keyboard::Key
     */
    virtual void keyEvent(Keyboard::KeyEvent evt, int key);

    /**
     * Touch callback on touch events.
     *
     * @param evt The touch event that occurred.
     * @param x The x position of the touch in pixels. Left edge is zero.
     * @param y The y position of the touch in pixels. Top edge is zero.
     * @param contactIndex The order of occurrence for multiple touch contacts starting at zero.
     *
     * @see Touch::TouchEvent
     */
    virtual void touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);

    /**
     * Mouse callback on mouse events. If the game does not consume the mouse move event or left mouse click event
     * then it is interpreted as a touch event instead.
     *
     * @param evt The mouse event that occurred.
     * @param x The x position of the mouse in pixels. Left edge is zero.
     * @param y The y position of the mouse in pixels. Top edge is zero.
     * @param wheelDelta The number of mouse wheel ticks. Positive is up (forward), negative is down (backward).
     *
     * @return True if the mouse event is consumed or false if it is not consumed.
     *
     * @see Mouse::MouseEvent
     */
    virtual bool mouseEvent(Mouse::MouseEvent evt, int x, int y, int wheelDelta);
    
    /**
     * Called when the game window has been resized.
     *
     * This method is called once the game window is created with its initial size
     * and then again any time the game window changes size.
     *
     * @param width The new game window width.
     * @param height The new game window height.
     */
    virtual void resizeEvent(unsigned int width, unsigned int height);

    /** 
     * Gets whether the current platform supports mouse input.
     *
     * @return true if a mouse is supported, false otherwise.
     */
    inline bool hasMouse();
    
    /**
     * Gets whether mouse input is currently captured.
     *
     * @return is the mouse captured.
     */
    inline bool isMouseCaptured();
    
    /**
     * Enables or disables mouse capture.
     *
     * On platforms that support a mouse, when mouse capture is enabled,
     * the platform cursor will be hidden and the mouse will be warped
     * to the center of the screen. While mouse capture is enabled,
     * all mouse move events will then be delivered as deltas instead
     * of absolute positions.
     *
     * @param captured true to enable mouse capture mode, false to disable it.
     */
    inline void setMouseCaptured(bool captured);
    
    /**
     * Sets the visibility of the platform cursor.
     *
     * @param visible true to show the platform cursor, false to hide it.
     */
    inline void setCursorVisible(bool visible);
    
    /**
     * Determines whether the platform cursor is currently visible.
     *
     * @return true if the platform cursor is visible, false otherwise.
     */
    inline bool isCursorVisible();

    /**
     * Determines whether a specified gesture event is supported.
     *
     * Use Gesture::GESTURE_ANY_SUPPORTED to test if one or more gesture events are supported.
     *
     * @param evt The gesture event to test and see if it is supported.
     * @return true if the gesture tested is supported; false if not supported.
     */
    bool isGestureSupported(Gesture::GestureEvent evt);

    /**
     * Requests the game to register and start recognizing the specified gesture event.
     *
     * Call with Gesture::GESTURE_ANY_SUPPORTED to recognize all supported gestures.
     * Once a gesture is recognized the specific gesture event methods will
     * begin to be called.
     *
     * Registering for:
     *
     * Gesture::GESTURE_SWIPE calls gestureSwipeEvent(..)
     * Gesture::GESTURE_PINCH calls gesturePinchEvent(..)
     * Gesture::GESTURE_TAP calls gestureTapEvent(..)
     *
     * @param evt The gesture event to start recognizing for
     */
    void registerGesture(Gesture::GestureEvent evt);

    /**
     * Requests the game to unregister for and stop recognizing the specified gesture event.
     *
     * Call with Gesture::GESTURE_ANY_SUPPORTED to unregister events from all supported gestures.
     *
     * @param evt The gesture event to start recognizing for
     */
    void unregisterGesture(Gesture::GestureEvent evt);

    /**
     * Determines whether a specified gesture event is registered to receive event callbacks.
     *
     * @return true if the specified gesture event is registered; false of not registered.
     */
    bool isGestureRegistered(Gesture::GestureEvent evt);

    /**
     * Gesture callback on Gesture::SWIPE events.
     *
     * @param x The x-coordinate of the start of the swipe.
     * @param y The y-coordinate of the start of the swipe.
     * @param direction The direction of the swipe
     *
     * @see Gesture::SWIPE_DIRECTION_UP
     * @see Gesture::SWIPE_DIRECTION_DOWN
     * @see Gesture::SWIPE_DIRECTION_LEFT
     * @see Gesture::SWIPE_DIRECTION_RIGHT
     */
    virtual void gestureSwipeEvent(int x, int y, int direction);

    /**
     * Gesture callback on Gesture::PINCH events.
     *
     * @param x The centroid x-coordinate of the pinch.
     * @param y The centroid y-coordinate of the pinch.
     * @param scale The scale of the pinch.
     */
    virtual void gesturePinchEvent(int x, int y, float scale);

    /**
     * Gesture callback on Gesture::LONG_TAP events.
     *
     * @param x The x-coordinate of the long tap.
     * @param y The y-coordinate of the long tap.
     * @param duration The duration of the long tap in ms.
     */
    virtual void gestureLongTapEvent(int x, int y, float duration);

    /**
     * Gesture callback on Gesture::TAP events.
     *
     * @param x The x-coordinate of the tap.
     * @param y The y-coordinate of the tap.
     */
    virtual void gestureTapEvent(int x, int y);

    /**
     * Gesture callback on Gesture::DRAG events.
     *
     * @param x The x-coordinate of the start of the drag event.
     * @param y The y-coordinate of the start of the drag event.
     */
    virtual void gestureDragEvent(int x, int y);

    /**
     * Gesture callback on Gesture::DROP events.
     *
     * @param x The x-coordinate of the drop event.
     * @param y The y-coordinate of the drop event.
     */
    virtual void gestureDropEvent(int x, int y);

    /**
     * Gamepad callback on gamepad events.  Override to receive Gamepad::CONNECTED_EVENT 
     * and Gamepad::DISCONNECTED_EVENT, and store the Gamepad* in order to poll it from update().
     *
     * @param evt The gamepad event that occurred.
     * @param gamepad The gamepad that generated the event.
     */
    virtual void gamepadEvent(Gamepad::GamepadEvent evt, Gamepad* gamepad);

    /**
     * Gets the current number of gamepads currently connected to the system.
     *
     * @return The number of gamepads currently connected to the system.
     */
    inline unsigned int getGamepadCount() const;

    /**
     * Gets the gamepad at the specified index. 
     *
     * The gamepad index can change when connected and disconnected so you
     * cannot rely on this other than iterating through them all to display
     * them or poll them.
     * 
     * The preferPhysical will bump over virtual gamepads if physical gamepads are
     * connected and return the request index of the first or second physcial and then 
     * return back to the first virtual after.
     *
     * @param index The index of the gamepad to retrieve.
     * @param preferPhysical true if you prefer return a physical if exist; false if only virtual.
     * @return The gamepad at the specified index.
     */
    inline Gamepad* getGamepad(unsigned int index, bool preferPhysical = true) const;

    /**
     * Sets whether multi-sampling is to be enabled/disabled. Default is disabled.
     *
     * @param enabled true sets multi-sampling to be enabled, false to be disabled.
     */
    inline void setMultiSampling(bool enabled);

    /**
     * Is multi-sampling enabled.
     *
     * @return true if multi-sampling is enabled.
     */
    inline bool isMultiSampling() const;

    /**
     * Sets multi-touch is to be enabled/disabled. Default is disabled.
     *
     * @param enabled true sets multi-touch is enabled, false to be disabled.
     */
    inline void setMultiTouch(bool enabled);

    /**
     * Is multi-touch mode enabled.
     *
     * @return true if multi-touch is enabled.
     */
    inline bool isMultiTouch() const;

    /**
     * Whether this game is allowed to exit programmatically.
     *
     * @return true if a programmatic exit is allowed.
     */
    inline bool canExit() const;

    /**
     * Whether this game has accelerometer support.
     */
    inline bool hasAccelerometer() const;

    /**
     * Gets the current accelerometer values for use as an indication of device
     * orientation. Despite its name, implementations are at liberty to combine
     * accelerometer data with data from other sensors as well, such as the gyros.
     * This method is best used to obtain an indication of device orientation; it
     * does not necessarily distinguish between acceleration and rotation rate.
     *
     * @param pitch The pitch angle returned (in degrees). Zero if hasAccelerometer() returns false.
     * @param roll The roll angle returned (in degrees). Zero if hasAccelerometer() returns false.
     */
    inline void getAccelerometerValues(float* pitch, float* roll);

    /**
     * Gets sensor values (raw), if equipped, allowing a distinction between device acceleration
     * and rotation rate. Returns zeros on platforms with no corresponding support. See also
     * hasAccelerometer() and getAccelerometerValues().
     *
     * @param accelX The x-coordinate of the raw accelerometer data.
     * @param accelY The y-coordinate of the raw accelerometer data.
     * @param accelZ The z-coordinate of the raw accelerometer data.
     * @param gyroX The x-coordinate of the raw gyroscope data.
     * @param gyroY The y-coordinate of the raw gyroscope data.
     * @param gyroZ The z-coordinate of the raw gyroscope data.
     */
    inline void getSensorValues(float* accelX, float* accelY, float* accelZ, float* gyroX, float* gyroY, float* gyroZ);

    /**
     * Gets the command line arguments.
     * 
     * @param argc The number of command line arguments.
     * @param argv The array of command line arguments.
     * @script{ignore}
     */
    void getArguments(int* argc, char*** argv) const;

    /**
     * Schedules a time event to be sent to the given TimeListener a given number of game milliseconds from now.
     * Game time stops while the game is paused. A time offset of zero will fire the time event in the next frame.
     * 
     * @param timeOffset The number of game milliseconds in the future to schedule the event to be fired.
     * @param timeListener The TimeListener that will receive the event.
     * @param cookie The cookie data that the time event will contain.
     * @script{ignore}
     */
    void schedule(float timeOffset, TimeListener* timeListener, void* cookie = 0);

    /**
     * Schedules a time event to be sent to the given TimeListener a given number of game milliseconds from now.
     * Game time stops while the game is paused. A time offset of zero will fire the time event in the next frame.
     * 
     * The given script function must take a single floating point number, which is the difference between the
     * current game time and the target time (see TimeListener::timeEvent). The function will be executed
     * in the context of the script envionrment that the schedule function was called from.
     * 
     * @param timeOffset The number of game milliseconds in the future to schedule the event to be fired.
     * @param function The script function that will receive the event.
     */
    void schedule(float timeOffset, const char* function);

    /**
     * Clears all scheduled time events.
     */
    void clearSchedule();

    /**
     * Opens an URL in an external browser, if available.
     *
     * @param url URL to be opened.
     *
     * @return True if URL was opened successfully, false otherwise.
     */
    bool launchURL(const char *url) const;

protected:

    /**
     * Initialize callback that is called just before the first frame when the game starts.
     */
    virtual void initialize();

    /**
     * Finalize callback that is called when the game on exits.
     */
    virtual void finalize();

    /**
     * Update callback for handling update routines.
     *
     * Called just before render, once per frame when game is running.
     * Ideal for non-render code and game logic such as input and animation.
     *
     * @param elapsedTime The elapsed game time.
     */
    virtual void update(float elapsedTime);

    /**
     * Render callback for handling rendering routines.
     *
     * Called just after update, once per frame when game is running.
     * Ideal for all rendering code.
     *
     * @param elapsedTime The elapsed game time.
     */
    virtual void render(float elapsedTime);

    /**
     * Renders a single frame once and then swaps it to the display.
     *
     * This is useful for rendering splash screens.
     */
    template <class T>
    void renderOnce(T* instance, void (T::*method)(void*), void* cookie);

    /**
     * Renders a single frame once and then swaps it to the display.
     * This calls the given script function, which should take no parameters and return nothing (void).
     *
     * This is useful for rendering splash screens.
     */
    void renderOnce(const char* function);

    /**
     * Updates the game's internal systems (audio, animation, physics) once.
     * 
     * Note: This does not call the user-defined Game::update() function.
     *
     * This is useful for rendering animated splash screens.
     */
    void updateOnce();

private:

    struct ShutdownListener : public TimeListener
    {
        void timeEvent(long timeDiff, void* cookie);
    };

    /**
     * TimeEvent represents the event that is sent to TimeListeners as a result of calling Game::schedule().
     */
    class TimeEvent
    {
    public:

        TimeEvent(double time, TimeListener* timeListener, void* cookie);
        bool operator<(const TimeEvent& v) const;
        double time;
        TimeListener* listener;
        void* cookie;
    };

    /**
     * Constructor.
     *
     * @param copy The game to copy.
     */
    Game(const Game& copy);

    /**
     * Starts the game.
     */
    bool startup();

    /**
     * Shuts down the game.
     */
    void shutdown();

    /**
     * Fires the time events that were scheduled to be called.
     * 
     * @param frameTime The current game frame time. Used to determine which time events need to be fired.
     */
    void fireTimeEvents(double frameTime);

    /**
     * Loads the game configuration.
     */
    void loadConfig();

    /**
     * Loads the gamepads from the configuration file.
     */
    void loadGamepads();

    void keyEventInternal(Keyboard::KeyEvent evt, int key);
    void touchEventInternal(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);
    bool mouseEventInternal(Mouse::MouseEvent evt, int x, int y, int wheelDelta);
    void resizeEventInternal(unsigned int width, unsigned int height);
    void gestureSwipeEventInternal(int x, int y, int direction);
    void gesturePinchEventInternal(int x, int y, float scale);
    void gestureTapEventInternal(int x, int y);
    void gestureLongTapEventInternal(int x, int y, float duration);
    void gestureDragEventInternal(int x, int y);
    void gestureDropEventInternal(int x, int y);
    void gamepadEventInternal(Gamepad::GamepadEvent evt, Gamepad* gamepad);

    bool _initialized;                          // If game has initialized yet.
    State _state;                               // The game state.
    unsigned int _pausedCount;                  // Number of times pause() has been called.
    static double _pausedTimeLast;              // The last time paused.
    static double _pausedTimeTotal;             // The total time paused.
    double _frameLastFPS;                       // The last time the frame count was updated.
    unsigned int _frameCount;                   // The current frame count.
    unsigned int _frameRate;                    // The current frame rate.
    unsigned int _width;                        // The game's display width.
    unsigned int _height;                       // The game's display height.
    VRectangle _viewport;                        // the games's current viewport.
    Vector4 _clearColor;                        // The clear color value last used for clearing the color buffer.
    float _clearDepth;                          // The clear depth value last used for clearing the depth buffer.
    int _clearStencil;                          // The clear stencil value last used for clearing the stencil buffer.
    Properties* _properties;                    // Game configuration properties object.
    AnimationController* _animationController;  // Controls the scheduling and running of animations.
    AudioController* _audioController;          // Controls audio sources that are playing in the game.
    PhysicsController* _physicsController;      // Controls the simulation of a physics scene and entities.
    AIController* _aiController;                // Controls AI simulation.
    AudioListener* _audioListener;              // The audio listener in 3D space.
    std::priority_queue<TimeEvent, std::vector<TimeEvent>, std::less<TimeEvent> >* _timeEvents;     // Contains the scheduled time events.
    ScriptController* _scriptController;            // Controls the scripting engine.
    ScriptTarget* _scriptTarget;                // Script target for the game

    // Note: Do not add STL object member variables on the stack; this will cause false memory leaks to be reported.

    friend class ScreenDisplayer;
};

}

#include "Game.inl"

#endif
