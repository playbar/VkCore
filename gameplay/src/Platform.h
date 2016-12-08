#ifndef PLATFORM_H_
#define PLATFORM_H_

#include "Vector2.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Touch.h"
#include "Gesture.h"
#include "Gamepad.h"
#include "FileSystem.h"

namespace vkcore
{

class Game;


class Platform
{
public:

    friend class Game;
    friend class Gamepad;
    friend class ScreenDisplayer;
    friend class FileSystem;

   
    ~Platform();
 
    static Platform* create(Game* game);
 
    int enterMessagePump();

    static void swapBuffers();

private:

    static void signalShutdown();
    static bool canExit();
    static unsigned int getDisplayWidth();
    static unsigned int getDisplayHeight();
    static double getAbsoluteTime();
    static void setAbsoluteTime(double time);
    static bool isVsync();

    static void setVsync(bool enable);
    static void sleep(long ms);
    static void setMultiSampling(bool enabled);
    static bool isMultiSampling();

    static void setMultiTouch(bool enabled);

   /**
    * Is multi-touch mode enabled.
    */
    static bool isMultiTouch();

    /**
     * Whether the platform has mouse support.
     */
    static bool hasMouse();

    static void setMouseCaptured(bool captured);

    /**
     * Determines if mouse capture is currently enabled.
     */
    static bool isMouseCaptured();

    /**
     * Sets the visibility of the platform cursor.
     *
     * On platforms that support a visible cursor, this method
     * toggles the visibility of the cursor.
     *
     * @param visible true to show the platform cursor, false to hide it.
     */
    static void setCursorVisible(bool visible);

    /**
     * Determines whether the platform cursor is currently visible.
     *
     * @return true if the platform cursor is visible, false otherwise.
     */
    static bool isCursorVisible();

    /**
     * Whether the platform has accelerometer support.
     */
    static bool hasAccelerometer();

    /**
     * Gets the platform accelerometer values for use as an indication of device
     * orientation. Despite its name, implementations are at liberty to combine
     * accelerometer data with data from other sensors as well, such as the gyros.
     * This method is best used to obtain an indication of device orientation; it
     * does not necessarily distinguish between acceleration and rotation rate.
     *
     * @param pitch The accelerometer pitch. Zero if hasAccelerometer() returns false.
     * @param roll The accelerometer roll. Zero if hasAccelerometer() returns false.
     */
    static void getAccelerometerValues(float* pitch, float* roll);

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
    static void getSensorValues(float* accelX, float* accelY, float* accelZ, float* gyroX, float* gyroY, float* gyroZ);

    /**
     * Gets the command line arguments.
     *
     * @param argc The number of command line arguments.
     * @param argv The array of command line arguments.
     */
    static void getArguments(int* argc, char*** argv);

    /**
     * Shows or hides the virtual keyboard (if supported).
     *
     * @param display true when virtual keyboard needs to be displayed and false otherwise.
     */
    static void displayKeyboard(bool display);

    /**
     * Tests if the specified gesture is supported on the platform.
     *
     * @return true if it is supported; false if not supported.
     */
    static bool isGestureSupported(Gesture::GestureEvent evt);

    /**
     * Registers the platform for gesture recognition for the specified gesture event.
     *
     * @param evt The gesture event to register to start recognizing events for.
     */
    static void registerGesture(Gesture::GestureEvent evt);

    /**
     * Registers the platform for gesture recognition for the specified gesture event.
     *
     * @param evt The gesture event to register to start recognizing events for.
     */
    static void unregisterGesture(Gesture::GestureEvent evt);

    /**
     * Tests if the specified gesture is registered for gesture recognition for the specified gesture event
     *
     * @param evt The gesture event to register to start recognizing events for.
     */
    static bool isGestureRegistered(Gesture::GestureEvent evt);

    /**
     * Opens an URL in an external browser, if available.
     *
     * @param url URL to be opened.
     *
     * @return True if URL was opened successfully, false otherwise.
     */
    static bool launchURL(const char* url);

    /**
     * Constructor.
     */
    Platform(Game* game);

    /**
     * Constructor.
     */
    Platform(const Platform& copy);

public:

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void touchEventInternal(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex, bool actuallyMouse = false);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void keyEventInternal(Keyboard::KeyEvent evt, int key);

   /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static bool mouseEventInternal(Mouse::MouseEvent evt, int x, int y, int wheelDelta);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gestureSwipeEventInternal(int x, int y, int direction);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gesturePinchEventInternal(int x, int y, float scale);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gestureTapEventInternal(int x, int y);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
	static void gestureLongTapEventInternal(int x, int y, float duration);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
	static void gestureDragEventInternal(int x, int y);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
	static void gestureDropEventInternal(int x, int y);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void resizeEventInternal(unsigned int width, unsigned int height);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gamepadEventConnectedInternal(GamepadHandle handle, unsigned int buttonCount, unsigned int joystickCount, unsigned int triggerCount, const char* name);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gamepadEventDisconnectedInternal(GamepadHandle handle);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gamepadButtonPressedEventInternal(GamepadHandle handle, Gamepad::ButtonMapping mapping);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gamepadButtonReleasedEventInternal(GamepadHandle handle, Gamepad::ButtonMapping button);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gamepadTriggerChangedEventInternal(GamepadHandle handle, unsigned int index, float value);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void gamepadJoystickChangedEventInternal(GamepadHandle handle, unsigned int index, float x, float y);

    /**
     * Internal method used to poll the platform for the updated Gamepad
     * states such as buttons, joytick and trigger values.
     *
     * Some platforms require to poll the gamepad system to get deltas. 
     *
     * @param gamepad The gamepad to be returned with the latest polled values populated.
     * @script{ignore}
     */
    static void pollGamepadState(Gamepad* gamepad);

    /**
     * Displays an open or save dialog using the native platform dialog system.
     *
     * @param mode The mode of the dialog. (Ex. OPEN or SAVE)
     * @param title The title of the dialog. (Ex. Select File or Save File)
     * @param filterDescription The file filter description. (Ex. Image Files)
     * @param filterExtensions The semi-colon delimited list of filtered file extensions. (Ex. png;jpg;bmp)
     * @param initialDirectory The initial directory to open or save files from. (Ex. "res") If NULL this will use the executable directory.
     * @return The file that is opened or saved, or an empty string if canceled.
     *
     * @script{ignore}
     */
    static std::string displayFileDialog(size_t mode, const char* title, const char* filterDescription, const char* filterExtensions, const char* initialDirectory);

    /**
     * Internal method used only from static code in various platform implementation.
     *
     * @script{ignore}
     */
    static void shutdownInternal();

private:

    Game* _game;                // The game this platform is interfacing with.
};

}

#include "Game.h"

#endif
