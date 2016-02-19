#include "Common\Camera.h"
#include "Common\Scene.h"
#include "Common\Timer.h"
#include "Common\Utility.h"
#include "D3D12\Renderer.h"
#include "UI\Window.h"

// Key press status: 1 if pressed, 0 otherwise 
struct KeyPressStatus {
    uint w : 1;
    uint s : 1;
    uint a : 1;
    uint d : 1;
    uint q : 1;
    uint e : 1;
};

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
    // Initialize the input status (no pressed keys)
    KeyPressStatus keyPressStatus{};
    // Start the timer to compute the frame time deltaT
    uint64 prevFrameTime = HighResTimer::microseconds();
    // Main loop
    while (true) {
        // Update the timers as we start a new frame
        const uint64 currFrameTime = HighResTimer::microseconds();
        const float  deltaMillisec = 1e-3f * (currFrameTime - prevFrameTime);
        const float  deltaSeconds  = static_cast<float>(1e-6 * (currFrameTime - prevFrameTime));
        prevFrameTime = currFrameTime;
        Window::displayFrameTime(deltaMillisec);
        // If the queue is not empty, retrieve a message
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // Forward the message to the window
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // Process the message locally
            switch (msg.message) {
                case WM_KEYUP:
                case WM_KEYDOWN:
                    {
                        const uint status = msg.message ^ 0x1;
                        switch (msg.wParam) {
                            case 0x57:
                                keyPressStatus.w = status;
                                break;
                            case 0x53:
                                keyPressStatus.s = status;
                                break;
                            case 0x41:
                                keyPressStatus.a = status;
                                break;
                            case 0x44:
                                keyPressStatus.d = status;
                                break;
                            case 0x45:
                                keyPressStatus.e = status;
                                break;
                            case 0x51:
                                keyPressStatus.q = status;
                                break;
                        }
                    }
                    break;
                case WM_QUIT:
                    engine.stop();
                    // Return this part of the WM_QUIT message to Windows
                    return static_cast<int>(msg.wParam);
            }
        }
        // The message queue is now empty; process keyboard input
        const float unitDist  = deltaSeconds * CAM_SPEED;
        const float unitAngle = deltaSeconds * CAM_ANG_SPEED;
        float distance  = 0.f;
        float horAngle  = 0.f;
        float vertAngle = 0.f;
        if (keyPressStatus.w) distance  += unitDist;
        if (keyPressStatus.s) distance  -= unitDist;
        if (keyPressStatus.a) horAngle  += unitAngle;
        if (keyPressStatus.d) horAngle  -= unitAngle;
        if (keyPressStatus.e) vertAngle += unitAngle;
        if (keyPressStatus.q) vertAngle -= unitAngle;
        pCam.moveForward(distance);
        pCam.rotateLeft(horAngle);
        pCam.rotateUpwards(vertAngle);
        // Execute engine code
        engine.setViewProjMatrix(pCam.computeViewProjMatrix());
        engine.executeCopyCommands(false);
        engine.startFrame();
        for (uint i = 0, n = scene.numObjects; i < n; ++i) {
            engine.draw(scene.vbo, scene.ibos[i]);
        }
        engine.finalizeFrame();
    }
}
