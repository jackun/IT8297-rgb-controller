#pragma once

#include <windows.h>
#include <process.h>
#include <iostream>
#include <vector>
#include <functional>
#include <utility>

static const char* g_AppName = "rgblights";

using func_cb = std::function<void()>;

class MyWindow
{
	HWND  m_hWnd;
	int m_width;
	int m_height;

	std::vector<func_cb> m_suspend_cb;
	std::vector<func_cb> m_resume_cb;

public:
	MyWindow(const int width, const int height) :m_hWnd(NULL), m_width(width), m_height(height)
	{
		_beginthread(&MyWindow::thread_entry, 0, this);
	}

	~MyWindow(void)
	{
		SendMessage(m_hWnd, WM_CLOSE, NULL, NULL);
	}

	void AddSuspendCB(func_cb cb)
	{
		m_suspend_cb.push_back(cb);
	}
	void AddResumeCB(func_cb cb)
	{
		m_resume_cb.push_back(cb);
	}

private:
	static void thread_entry(void* p_userdata)
	{
		MyWindow* p_win = static_cast<MyWindow*> (p_userdata);
		p_win->create_window();
		p_win->message_loop();
	}

	void create_window()
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = &MyWindow::WindowProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = GetModuleHandle(NULL);
		wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = g_AppName;
		wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

		RegisterClassEx(&wcex);

		m_hWnd = CreateWindow(g_AppName, g_AppName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, GetModuleHandle(NULL), NULL);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
		ShowWindow(m_hWnd, SW_HIDE);
		UpdateWindow(m_hWnd);
	}

	void message_loop()
	{
		MSG msg = { 0 };

		while (GetMessage(&msg, NULL, 0, 0))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	static LRESULT WINAPI WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		MyWindow* ptr = reinterpret_cast<MyWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		switch (uMsg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_POWERBROADCAST:
		{
			if (wParam == PBT_APMSUSPEND) {
				for (auto& cb : ptr->m_suspend_cb)
					cb();
			}
			else if (wParam == PBT_APMRESUMEAUTOMATIC) {
				for (auto& cb : ptr->m_resume_cb)
					cb();
			}
		}

		}

		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
};