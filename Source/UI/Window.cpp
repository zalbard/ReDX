#include <algorithm>
#include <cassert>
#include <cwchar>
#include <Windows.h>
#include "Window.h"
#include "..\Common\Utility.h"

// Perform static member initialization.
HWND Window::m_hwnd   = nullptr;
long Window::m_width  = 0;
long Window::m_height = 0;
RECT Window::m_rect   = {};

// Main message handler.
LRESULT CALLBACK WindowProc(const HWND hWnd, const UINT message,
                            const WPARAM wParam, const LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (VK_ESCAPE == wParam) {
                DestroyWindow(hWnd);
                return 0;
            }
            // Otherwise fall through to DefWindowProc().
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

void Window::open(const long width, const long height) {
    m_width  = width;
    m_height = height;
    // Set up the rectangle position and dimensions.
    m_rect = {0, 0, width, height};
    AdjustWindowRect(&m_rect, WS_OVERLAPPEDWINDOW, FALSE);
    // Get the handle to the instance of the application.
    const HINSTANCE appHandle = GetModuleHandle(nullptr);
    // Register the window class.
    WNDCLASS wndClass{};
    wndClass.style         = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = WindowProc;
    wndClass.hInstance     = appHandle;
    wndClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wndClass.lpszClassName = L"ReDXWindowClass";
    if (!RegisterClass(&wndClass)) {
        printError("RegisterClass failed.");
        TERMINATE();
    }
    // Create a window and store its handle.
    m_hwnd = CreateWindow(wndClass.lpszClassName, L"ReDX",
                          WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,  // Disable resizing
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          m_rect.right - m_rect.left,
                          m_rect.bottom - m_rect.top,
                          nullptr, nullptr,                         // No parent window, no menus
                          appHandle, nullptr);
    if (!m_hwnd) {
        printError("CreateWindow failed.");
        TERMINATE();
    }
    // Make the window visible.
    ShowWindow(m_hwnd, SW_SHOWNORMAL);
}

HWND Window::handle() {
    assert(m_hwnd && "Uninitialized window handle.");
    return m_hwnd;
}

long Window::width() {
    return m_width;
}

long Window::height() {
    return m_height;
}

float Window::aspectRatio() {
    return static_cast<float>(width())/static_cast<float>(height());
}

void Window::displayInfo(const float cpuFrameTime, const float gpuFrameTime) {
    static wchar_t title[] = L"ReDX | CPU: 00.00 ms, GPU: 00.00 ms";
    swprintf(title, _countof(title) + 1, L"ReDX | CPU: %5.2f ms, GPU: %5.2f ms",
             std::min(cpuFrameTime, 99.99f), std::min(gpuFrameTime, 99.99f));
    SetWindowText(m_hwnd, title);
}
