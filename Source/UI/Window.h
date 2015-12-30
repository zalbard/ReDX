#pragma once

#include <Windows.h>
#include "..\Common\Definitions.h"

// Singleton GUI Window
class Window {
public:
   SINGLETON(Window);
   // Creates a window; takes horizontal and vertical resolution as input
   static void create(const long resX, const long resY);
   // Returns the handle of the window
   static HWND handle();
private:
   static HWND       m_hwnd;        // Handle
   static RECT       m_rect;        // Screen-space rectangle
   static WNDCLASSEX m_class;       // Window class
   static WCHAR*     m_title;       // Title string
};
