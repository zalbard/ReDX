#pragma once

#include <minwindef.h>
#include "..\Common\Definitions.h"

// GUI Window.
class Window {
public:
   STATIC_CLASS(Window);
   // Creates a window; takes the client (drawable) area dimensions (in pixels) as input.
   static void open(const long width, const long height);
   // Returns the handle of the window.
   static HWND handle();
   // Returns the client (drawable) area width (in pixels).
   static long width();
   // Returns the client (drawable) area height (in pixels).
   static long height();
   // Returns the width/height ratio of the drawable window area.
   static float aspectRatio();
   // Displays information (in milliseconds) in the title bar:
   // 'cpuFrameTime', 'gpuFrameTime' - the frame times (in milliseconds) of CPU/GPU timelines. 
   static void displayInfo(const float cpuFrameTime, const float gpuFrameTime);
private:
   static HWND m_hwnd;              // Handle
   static long m_width, m_height;   // Client area dimensions
   static RECT m_rect;              // Screen-space rectangle
};
