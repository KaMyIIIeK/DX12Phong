#pragma once
#include <windows.h>

class Input;

class Window
{
public:
    bool Initialize(HINSTANCE hInstance, int nCmdShow, int width, int height, const wchar_t* title);
    HWND GetHWND() const { return m_hWnd; }
    void SetInput(Input* input) { m_input = input; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hWnd = nullptr;
    Input* m_input = nullptr;
};