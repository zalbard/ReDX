#include <cassert>
#include "Window.h"

// Static member initialization
HWND       Window::m_hwnd  = nullptr;
RECT       Window::m_rect  = {};
WNDCLASSEX Window::m_class = {};
WCHAR*     Window::m_title = L"ReDX";

// WindowProc function prototype
LRESULT CALLBACK WindowProc(const HWND hWnd, const UINT message,
                            const WPARAM wParam, const LPARAM lParam);

void Window::create(const LONG resX, const LONG resY) {
    // Set up the rectangle position and dimensions
    m_rect = {0, 0, resX, resY};
    AdjustWindowRect(&m_rect, WS_OVERLAPPEDWINDOW, FALSE);
    // Set up the window class
    m_class.cbSize        = sizeof(WNDCLASSEX);
    m_class.style         = CS_HREDRAW | CS_VREDRAW;
    m_class.lpfnWndProc   = WindowProc;
    m_class.hInstance     = GetModuleHandle(nullptr);
    m_class.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    m_class.lpszClassName = L"ReDXWindowClass";
    RegisterClassEx(&m_class);
    // Create a window and store its handle
    m_hwnd = CreateWindow(m_class.lpszClassName,
                          m_title,
                          WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,  // Disable resizing
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          m_rect.right  - m_rect.left,
                          m_rect.bottom - m_rect.top,
                          nullptr, nullptr,                         // No parent window, no menus
                          GetModuleHandle(nullptr),
                          nullptr);
    // Display the window
    ShowWindow(m_hwnd, SW_SHOWNORMAL);
}

HWND Window::handle() {
    assert(m_hwnd && "Uninitialized window handle.");
    return m_hwnd;
}

// Main message handler
LRESULT CALLBACK WindowProc(const HWND hWnd, const UINT message, const WPARAM wParam,
                            const LPARAM lParam) {
    switch (message) {
        case WM_PAINT:
            //OnUpdate();
            //OnRender();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
}
