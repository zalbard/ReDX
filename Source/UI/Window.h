#pragma once

#include <minwindef.h>
#include "..\Common\Definitions.h"

// GUI Window.
class Window {
public:
   STATIC_CLASS(Window);
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
   // Displays information (in milliseconds) in the title bar:
   // 'fracObjVis' - the fraction of objects visible on screen;
   // 'cpuFrameTime', 'gpuFrameTime' - the frame times (in milliseconds) of CPU/GPU timelines. 
   static void displayInfo(const float fracObjVis, const float cpuFrameTime,
                           const float gpuFrameTime);
private:
   static HWND m_hwnd;      // Handle
   static RECT m_rect;      // Screen-space rectangle
};
