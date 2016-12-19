#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <exception>
#include "VulkanBase.h"


VulkanBase *vulkanExample;

#if defined(_WIN32)
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (vulkanExample != nullptr)
	{
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
#elif defined(__linux__) && !defined(__ANDROID__) && !defined(_DIRECT2DISPLAY)
static void handleEvent(const xcb_generic_event_t *event)
{
	if (vulkanExample != nullptr)
	{
		vulkanExample->handleEvent(event);
	}
}
#endif

// Main entry point
#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
#elif defined(__ANDROID__)
// Android entry point
void android_main(android_app* state)
#elif defined(__linux__)
// Linux entry point
int main(const int argc, const char *argv[])
#endif
{
#if defined(__ANDROID__)
	app_dummy();
#endif	
#if defined(_WIN32)
	vulkanExample = new VulkanBase(false);
	vulkanExample->setupWindow(hInstance, WndProc);
	vulkanExample->initSwapchain();
	vulkanExample->prepare();
	vulkanExample->renderLoop();
	delete(vulkanExample);
	return 0;
#elif defined(__ANDROID__)
	vulkanExample = new VkTriangle();
	// Attach vulkan example to global android application state
	state->userData = vulkanExample;
	state->onAppCmd = VkTriangle::handleAppCommand;
	state->onInputEvent = VkTriangle::handleAppInput;
	vulkanExample->androidApp = state;
	vulkanExample->renderLoop();
	delete(vulkanExample);
#elif defined(__linux__) && !defined(_DIRECT2DISPLAY)
	vulkanExample = new VkTriangle();
	vulkanExample->setupWindow();
	vulkanExample->initSwapchain();
	vulkanExample->prepare();
	vulkanExample->renderLoop();
	delete(vulkanExample);
	return 0;
#endif

}
