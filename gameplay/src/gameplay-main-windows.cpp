#ifndef GP_NO_PLATFORM
#ifdef WIN32

#include "gameplay.h"

using namespace vkcore;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Game* game = Game::getInstance();
	if (game != nullptr)
	{
		game->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)

{
	Game* game = Game::getInstance();
	Platform* platform = Platform::create(game);
	int result = platform->enterMessagePump();
	delete platform;
	return result;

	//Game *game = Game::getInstance();
	//game->setupWindow(hInstance, WndProc);
	//game->initSwapchain();
	//game->prepare();
	//game->renderLoop();
	return 0;
}


//extern "C" int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdLine, int cmdShow)
//{
//    Game* game = Game::getInstance();
//    Platform* platform = Platform::create(game);
//    GP_ASSERT(platform);
//    int result = platform->enterMessagePump();
//    delete platform;
//    return result;
//}

#endif
#endif
