#pragma once

#include <minwindef.h>
#include "..\Common\Definitions.h"

// GUI Window.
class Window {
public:
   SINGLETON(Window);
   // Creates a window; takes the window dimensions (in pixels) as input.
   static void open(const long width, const long height);
   // Returns the handle of the window.
   static HWND handle();
   // Returns the width (in pixels) of the drawable window area.
   static long width();
   // Returns the height (in pixels) of the drawable window area.
   static long height();
   // Returns the width/height ratio of the drawable window area.
   static float aspectRatio();
   // Displays the frame time (in milliseconds) in the title bar.
   static void displayFrameTime(const float deltaTime);
private:
   static HWND m_hwnd;      // Handle
   static RECT m_rect;      // Screen-space rectangle
};
