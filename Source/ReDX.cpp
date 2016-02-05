#include "Common\Scene.h"
#include "Common\Timer.h"
#include "Common\Utility.h"
#include "D3D12\Renderer.h"
#include "UI\Window.h"

int main(const int argc, const char* argv[]) {
    // Parse command line arguments
	if (argc > 1) {
		printError("The following command line arguments have been ignored:");
        for (int i = 1; i < argc; ++i) {
            printError("%s", argv[i]);
        }
	}
    // Verify SSE2 support for the DirectXMath library
    if (!DirectX::XMVerifyCPUSupport()) {
        printError("The CPU doesn't support SSE2. Aborting.");
        return -1;
    }
    // Create a window for rendering output
    Window::open(WND_WIDTH, WND_HEIGHT);
    // Initialize the renderer (internally uses the Window)
    D3D12::Renderer engine;
    // Provide the scene description
    const Scene scene{"Assets\\Sponza\\sponza.obj", engine};
    // Copy the scene description to the device
    engine.executeCopyCommands();
    // Start the timer to compute the frame time deltaT
    uint prevFrameT = HighResTimer::time_ms();
    // Main loop
    while (true) {
        // Update the timers as we start a new frame
        const uint currFrameT = HighResTimer::time_ms();
        const uint deltaT     = currFrameT - prevFrameT;
        prevFrameT = currFrameT;
        Window::displayFrameTime(deltaT);
        // If the queue is not empty, retrieve a message
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // Forward the message to the window
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // Process the message locally
            switch (msg.message) {
            case WM_KEYDOWN:
                /* TODO: Process keyboard input */
                break;
            case WM_QUIT:
                engine.stop();
                // Return this part of the WM_QUIT message to Windows
                return static_cast<int>(msg.wParam);
            }
        }
        // The message queue is now empty; execute engine code
        engine.startFrame();
        for (uint i = 0, n = scene.numObjects; i < n; ++i) {
            engine.draw(scene.vbo, scene.ibos[i]);
        }
        engine.finalizeFrame();
    }
}
