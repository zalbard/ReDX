#include "Common\Camera.h"
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
    // Set up the camera
    PerspectiveCamera pCam{Window::width(), Window::height(), VERTICAL_FOV,
                           /* pos */ {900.f, 200.f, -35.f},
                           /* dir */ {-1.f, 0.f, 0.f},
                           /* up  */ {0.f, 1.f, 0.f}};
    // Copy the data to the device
    engine.executeCopyCommands(false);
    // Start the timer to compute the frame time deltaT
    uint64 prevFrameTime = HighResTimer::microseconds();
    // Main loop
    while (true) {
        // Update the timers as we start a new frame
        const uint64 currFrameTime = HighResTimer::microseconds();
        const uint64 deltaTime     = currFrameTime - prevFrameTime;
        prevFrameTime = currFrameTime;
        Window::displayFrameTime(deltaTime);
        // If the queue is not empty, retrieve a message
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // Forward the message to the window
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // Process the message locally
            switch (msg.message) {
            case WM_KEYDOWN:
                // Process keyboard input
                switch (msg.wParam) {
                case 0x57:
                    // Process the W key
                     pCam.moveForward(10.0f);
                    break;
                case 0x53:
                    // Process the S key
                    pCam.moveBack(10.0f);
                    break;
                case 0x41:
                    // Process the A key
                    pCam.rotateLeft(0.1f);
                    break;
                case 0x44:
                    // Process the D key
                    pCam.rotateRight(0.1f);
                    break;
                case 0x45:
                    // Process the E key
                    pCam.rotateUpwards(0.1f);
                    break;
                case 0x51:
                    // Process the Q key
                    pCam.rotateDownwards(0.1f);
                    break;
                default:
                    break;
                }
                break;
            case WM_QUIT:
                engine.stop();
                // Return this part of the WM_QUIT message to Windows
                return static_cast<int>(msg.wParam);
            }
        }
        // The message queue is now empty; execute engine code
        engine.setViewProjMatrix(pCam.computeViewProjMatrix());
        engine.executeCopyCommands(false);
        engine.startFrame();
        for (uint i = 0, n = scene.numObjects; i < n; ++i) {
            engine.draw(scene.vbo, scene.ibos[i]);
        }
        engine.finalizeFrame();
    }
}
