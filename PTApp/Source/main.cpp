#include "RTXApp.h"

#ifdef _WIN64

#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	RTXApp app;
	app.Run();
}

#else

int main(int argc, const char** argv)
{
	RTXApp app;
	app.Run();
}

#endif