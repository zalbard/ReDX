#pragma once

#include <minwindef.h>
#include "..\Common\Definitions.h"

// GUI Window.
class Window {
public:
   STATIC_CLASS(Window);
   // Creates a window; takes the client (drawable) area dimensions (in pixels) as input.
   static void open(const uint32_t width, const uint32_t height);
   // Returns the handle of the window.
   static HWND handle();
   // Returns the client (drawable) area width (in pixels).
   static uint32_t width();
   // Returns the client (drawable) area height (in pixels).
   static uint32_t height();
   // Displays information (in milliseconds) in the title bar:
   // 'cpuFrameTime', 'gpuFrameTime' - the frame times (in milliseconds) of CPU/GPU timelines. 
   static void displayInfo(const float cpuFrameTime, const float gpuFrameTime);
private:
   static uint32_t m_width, m_height;   // Client area dimensions
   static HWND     m_hwnd;              // Handle
};
