#include "Application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int nCmdShow)
{
    Application app;
    if (!app.Initialize(hInstance, nCmdShow))
        return 1;

    return app.Run();
}