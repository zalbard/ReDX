#pragma once

#include <Windows.h>
#include "..\Common\Definitions.h"

// Singleton GUI Window
class Window {
public:
   // Creates window; takes horizontal and vertical resolution as input
   static void create(const LONG resX, const LONG resY);
   // Returns window handle
   static HWND handle();
private:
   RULE_OF_ZERO_MOVE_ONLY(Window);
   /* Data members */
   static HWND       m_hwnd;        // Handle
   static RECT       m_rect;        // Screen-space rectangle
   static WNDCLASSEX m_class;       // Window class
   static WCHAR*     m_title;       // Title string
};
