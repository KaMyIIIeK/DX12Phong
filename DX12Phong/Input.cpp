#include "Input.h"

void Input::AttachWindow(HWND hwnd)
{
    m_hWnd = hwnd;
    RegisterRawMouse();
}

void Input::RegisterRawMouse()
{
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = 0;
    rid.hwndTarget = m_hWnd;

    RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

void Input::ResetMouseDelta()
{
    m_mouseDeltaX = 0;
    m_mouseDeltaY = 0;
}

void Input::ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam < 256) m_keys[wParam] = true;
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        break;

    case WM_KEYUP:
        if (wParam < 256) m_keys[wParam] = false;
        break;

    case WM_INPUT:
    {
        if (!m_mouseCaptured)
            break;

        UINT size = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
        if (size == 0)
            break;

        BYTE* buffer = new BYTE[size];
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size)
        {
            RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
            if (raw->header.dwType == RIM_TYPEMOUSE)
            {
                m_mouseDeltaX += raw->data.mouse.lLastX;
                m_mouseDeltaY += raw->data.mouse.lLastY;
            }
        }
        delete[] buffer;
        break;
    }

    case WM_LBUTTONDOWN:
        m_mouseCaptured = true;
        SetCapture(hwnd);
        ShowCursor(FALSE);
        ResetMouseDelta();
        break;

    case WM_LBUTTONUP:
        m_mouseCaptured = false;
        ReleaseCapture();
        ShowCursor(TRUE);
        break;
    }
}