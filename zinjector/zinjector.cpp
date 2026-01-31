#include "app.h"

int main(int, char**)
{
	ZInjector::Application app;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, int nCmdShow) {
	return main(NULL, NULL);
}
