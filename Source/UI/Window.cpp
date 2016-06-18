#include <algorithm>
#include <cassert>
#include <cwchar>
#include <Windows.h>
#include "Window.h"
#include "..\Common\Utility.h"

// Perform static member initialization.
uint32_t Window::m_width  = 0;
uint32_t Window::m_height = 0;
HWND     Window::m_hwnd   = nullptr;

// Main message handler.
LRESULT CALLBACK WindowProc(const HWND hWnd, const UINT message,
                            const WPARAM wParam, const LPARAM lParam) {
    switch (message) {
        case WM_KEYDOWN:
            if (VK_ESCAPE == wParam) {
                DestroyWindow(hWnd);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

void Window::open(const uint32_t width, const uint32_t height) {
    m_width  = width;
    m_height = height;
    // Set up the rectangle position and dimensions.
    RECT rect = {0, 0, static_cast<long>(width), static_cast<long>(height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
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
                          rect.right - rect.left,
                          rect.bottom - rect.top,
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

uint32_t Window::width() {
    return m_width;
}

uint32_t Window::height() {
    return m_height;
}

void Window::displayInfo(const float cpuFrameTime, const float gpuFrameTime) {
    static wchar_t title[] = L"ReDX | CPU: 00.00 ms, GPU: 00.00 ms";
    swprintf(title, _countof(title) + 1, L"ReDX | CPU: %5.2f ms, GPU: %5.2f ms",
             std::min(cpuFrameTime, 99.99f), std::min(gpuFrameTime, 99.99f));
    SetWindowText(m_hwnd, title);
}
