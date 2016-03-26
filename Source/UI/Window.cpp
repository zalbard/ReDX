#include <algorithm>
#include <cassert>
#include <cwchar>
#include <Windows.h>
#include "Window.h"

// Perform static member initialization.
HWND Window::m_hwnd = nullptr;
RECT Window::m_rect = {};

// Main message handler.
LRESULT CALLBACK WindowProc(const HWND hWnd, const UINT message,
                            const WPARAM wParam, const LPARAM lParam) {
    switch (message) {
        case WM_KEYDOWN:
            if (VK_ESCAPE != wParam) return 0;
            // Otherwise fall through to WM_DESTROY
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

void Window::open(const long width, const long height) {
    // Set up the rectangle position and dimensions.
    m_rect = {0, 0, width, height};
    AdjustWindowRect(&m_rect, WS_OVERLAPPEDWINDOW, FALSE);
    // Set up the window class.
    WNDCLASSEX wndClass    = {};
    wndClass.cbSize        = sizeof(WNDCLASSEX);
    wndClass.style         = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = WindowProc;
    wndClass.hInstance     = GetModuleHandle(nullptr);
    wndClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wndClass.lpszClassName = L"ReDXWindowClass";
    RegisterClassEx(&wndClass);
    // Create a window and store its handle.
    m_hwnd = CreateWindow(wndClass.lpszClassName,
                          L"ReDX",
                          WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,  // Disable resizing
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          Window::width(), Window::height(),
                          nullptr, nullptr,                         // No parent window, no menus
                          GetModuleHandle(nullptr),
                          nullptr);
    // Make the window visible.
    ShowWindow(Window::handle(), SW_SHOWNORMAL);
}

HWND Window::handle() {
    assert(m_hwnd && "Uninitialized window handle.");
    return m_hwnd;
}

long Window::width() {
    return m_rect.right - m_rect.left;
}

long Window::height() {
    return m_rect.bottom - m_rect.top;
}

float Window::aspectRatio() {
    return static_cast<float>(width())/static_cast<float>(height());
}

void Window::displayFrameTime(const float deltaTime) {
    // Only print up to 4 digits (plus the separator).
    wchar_t title[] = L"ReDX : 00.00 ms";
    swprintf(title + 7, 9, L"%5.2f ms", std::min(99.99f, deltaTime));
    SetWindowText(m_hwnd, title);
}
