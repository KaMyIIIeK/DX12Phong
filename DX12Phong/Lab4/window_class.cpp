#include <Windows.h>
#include "window_class.h"

void WindowClass::initWindow(WNDPROC WndProc) {
	wc_.cbSize = sizeof(WNDCLASSEX);
	wc_.hInstance = hInstance_;
	wc_.lpszClassName = L"WindowClass";
	wc_.lpfnWndProc = WndProc;
	wc_.style = CS_HREDRAW | CS_VREDRAW;
	wc_.cbClsExtra = 0;
	wc_.cbWndExtra = 0;
	wc_.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc_.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc_.lpszMenuName = nullptr;
	wc_.hIconSm = wc_.hIcon;
}

void WindowClass::CreateWnd() {
	hWnd_ = CreateWindowExW(0, L"WindowClass", L"WindowName", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance_, nullptr);
}

void WindowClass::ShowWnd() {
	ShowWindow(hWnd_, SW_SHOW);
}

void WindowClass::UpdateWnd()
{
	UpdateWindow(hWnd_);
}

bool WindowClass::CheckRegister() {
	if (!RegisterClassEx(&wc_)) {
		return false;
	}
	return true;
}

bool WindowClass::CheckCreation() {
	return (hWnd_ != NULL);
}

void WindowClass::RegisterRawInputDevice() {

	// Мышь
	rid_[0].usUsagePage = 0x01;
	rid_[0].usUsage = 0x02;
	rid_[0].dwFlags = RIDEV_INPUTSINK;
	rid_[0].hwndTarget = hWnd_;

	// Клавиатура
	rid_[1].usUsagePage = 0x01;
	rid_[1].usUsage = 0x06;
	rid_[1].dwFlags = 0;
	rid_[1].hwndTarget = hWnd_;

	if (RegisterRawInputDevices(rid_, 2, sizeof(RAWINPUTDEVICE)) == FALSE) {
		MessageBoxW(hWnd_, L"Failed to register raw input devices", L"Error", MB_OK);
	}
}
