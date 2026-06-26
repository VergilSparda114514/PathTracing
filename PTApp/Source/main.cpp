#include "RTXApp.h"

int main()
{
	RTXApplication application;
	application.Run();
}

#ifdef _WIN64

#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	main();
}

#endif