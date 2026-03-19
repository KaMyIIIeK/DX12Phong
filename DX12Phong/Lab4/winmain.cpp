#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <dxgi.h>
#include <DirectXHelpers.h>
#include <comdef.h>
#include "window_class.h"
#include "DX12App.h"
#include "game_timer.h"
#include "vertex.h"
#include "d3dUtil.h"
#pragma comment(linker, "/entry:wWinMainCRTStartup")
HWND g_hWnd = 0;
DX12App MyFramework;

using namespace DirectX;
using namespace DX12;
using namespace Microsoft::WRL;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);

        SetWindowLongPtr(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
        return 0;
    }
    case WM_INPUT:
    {
        UINT dwSize = 0;

        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize,
            sizeof(RAWINPUTHEADER));

        BYTE* lpb = new BYTE[dwSize];
        if (lpb == NULL) return 0;

        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize,
            sizeof(RAWINPUTHEADER)) != dwSize) {
            delete[] lpb;
            return 0;
        }

        RAWINPUT* raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEMOUSE)
        {
            short dx = raw->data.mouse.lLastX;
            short dy = raw->data.mouse.lLastY;

            USHORT buttons = raw->data.mouse.usButtonFlags;

            if (buttons & RI_MOUSE_LEFT_BUTTON_DOWN)
                MyFramework.OnMouseDown(hwnd);

            if (buttons & RI_MOUSE_LEFT_BUTTON_UP)
                MyFramework.OnMouseUp();

            bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            MyFramework.OnMouseMove(leftDown ? MK_LBUTTON : 0, dx, dy);
        }

        else if (raw->header.dwType == RIM_TYPEKEYBOARD)
        {
            RAWKEYBOARD& keyboard = raw->data.keyboard;

            UINT virtualKey = keyboard.VKey;
            UINT scanCode = keyboard.MakeCode;
            UINT flags = keyboard.Flags;

            bool keyDown = !(flags & RI_KEY_BREAK);

        }

        delete[] lpb;
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WindowClass::WRun(GameTimer* gt) {
    MSG msg = {};
    gt->Reset();

    while (true) {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            gt->Tick();
                MyFramework.CalculateGameStats(*gt, hWnd_);
                MyFramework.Update(*gt);
                MyFramework.Draw(*gt);
        }
    }

    return static_cast<int>(msg.wParam);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	WindowClass wnd(hInstance, hPrevInstance);
	wnd.initWindow(WndProc);
	wnd.CheckRegister();
	wnd.CreateWnd();
	if (!wnd.CheckCreation()) {
		return 0;
	}
	g_hWnd = wnd.getHWND();

	MyFramework.InitializeDevice();
	MyFramework.InitializeCommandObjects();
	MyFramework.CreateSwapChain(g_hWnd);
	MyFramework.CreateRTVAndDSVDescriptorHeaps();
	MyFramework.CreateRTV();
	MyFramework.CreateDSV();
	MyFramework.SetViewport();
	MyFramework.SetScissor();
	MyFramework.SetTopology();
	MyFramework.BuildLayout();
	MyFramework.InitProjectionMatrix();
    MyFramework.ParseFile();
	MyFramework.CreateVertexBuffer();
	MyFramework.CreateIndexBuffer();
	MyFramework.InitUploadBuffer();
	MyFramework.CreateCBVDescriptorHeap();
	MyFramework.CreateConstantBufferView();
	MyFramework.CreateRootSignature();
	MyFramework.CompileShaders();
	MyFramework.CreatePSO();
	GameTimer gt;


	wnd.RegisterRawInputDevice();
	wnd.ShowWnd();
	wnd.UpdateWnd();
	

	wnd.WRun(&gt);

	return 0;
}