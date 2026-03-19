#define _WINDOW_CLASS_
#include <Windows.h>
#include "game_timer.h"
#include "DX12App.h"
class WindowClass 
{
private:
	HINSTANCE hInstance_;
	HINSTANCE hPrevInstance_;
	WNDCLASSEX wc_;
	HWND hWnd_ = nullptr;
	RAWINPUTDEVICE rid_[2];
public:
	WindowClass(HINSTANCE hInstance, HINSTANCE hPrevInstance) : hInstance_(hInstance), hPrevInstance_(hPrevInstance) {}
	void initWindow(WNDPROC WndProc);
	void CreateWnd();
	void ShowWnd();
	void UpdateWnd();
	bool CheckRegister();
	void RegisterRawInputDevice();
	int WRun(GameTimer* gt);
	bool CheckCreation();
	HWND getHWND() const { return hWnd_; }
};
