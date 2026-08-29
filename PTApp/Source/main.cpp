#include "RTXApp.h"

#include <Singleton.h>

int main()
{
	Singleton<RTXApplication>::Get().Run();
}

#ifdef _WIN64

#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	main();
}

#endif