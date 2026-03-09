#pragma once

#include <windows.h>
#include "Window.h"
#include "Input.h"
#include "Camera.h"
#include "DX12Renderer.h"

class Application
{
public:
    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();

private:
    Window m_window;
    Input m_input;
    Camera m_camera;
    DX12Renderer m_renderer;
};