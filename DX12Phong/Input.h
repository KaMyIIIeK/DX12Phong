#pragma once
#include <windows.h>

class Input
{
public:
    void AttachWindow(HWND hwnd);
    void ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ResetMouseDelta();

    bool IsKeyDown(unsigned char key) const { return m_keys[key]; }
    int GetMouseDeltaX() const { return m_mouseDeltaX; }
    int GetMouseDeltaY() const { return m_mouseDeltaY; }
    bool IsMouseCaptured() const { return m_mouseCaptured; }

private:
    void RegisterRawMouse();

private:
    HWND m_hWnd = nullptr;
    bool m_keys[256] = {};
    int m_mouseDeltaX = 0;
    int m_mouseDeltaY = 0;
    bool m_mouseCaptured = false;
};