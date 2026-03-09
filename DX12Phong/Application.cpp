#include "Application.h"
#include <chrono>

bool Application::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    if (!m_window.Initialize(hInstance, nCmdShow, 800, 600, L"DX12 Cube Framework"))
        return false;

    m_input.AttachWindow(m_window.GetHWND());
    m_window.SetInput(&m_input);

    if (!m_renderer.Initialize(m_window.GetHWND(), 800, 600))
        return false;

    m_camera.SetPosition(0.0f, 0.3f, -2.5f);
    return true;
}

int Application::Run()
{
    MSG msg = {};
    auto prevTime = std::chrono::steady_clock::now();

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - prevTime).count();
            prevTime = now;

            m_camera.Update(m_input, dt);
            m_renderer.Render(m_camera);
        }
    }

    m_renderer.Shutdown();
    return static_cast<int>(msg.wParam);
}