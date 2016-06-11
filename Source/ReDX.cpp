#include <future>
#include "Common\Camera.h"
#include "Common\Utility.h"
#include "D3D12\Renderer.hpp"
#include "UI\Window.h"

using namespace DirectX;

// Key press status: 1 if pressed, 0 otherwise.
struct KeyPressStatus {
    uint w : 1;
    uint s : 1;
    uint a : 1;
    uint d : 1;
    uint q : 1;
    uint e : 1;
};

int __cdecl main(const int argc, const char* argv[]) {
    // Parse command line arguments.
	if (argc > 1) {
		printWarning("The following command line arguments have been ignored:");
        for (int i = 1; i < argc; ++i) {
            printWarning("%s", argv[i]);
        }
	}
    // Verify SSE4.1 support for the DirectXMath library.
    if (!SSE4::XMVerifySSE4Support()) {
        printError("The CPU doesn't support SSE4.1. Aborting.");
        return -1;
    }
    // Create a window for rendering output.
    Window::open(RES_X, RES_Y);
    // Initialize the renderer (internally uses the Window).
    D3D12::Renderer engine;
    // Provide the scene description.
    Scene scene{"..\\..\\Assets\\Sponza\\", "sponza.obj", engine};
    // Set up the camera.
    PerspectiveCamera pCam{Window::width(), Window::height(), VERTICAL_FOV,
                           /* pos */ {300.f, 200.f, -35.f},
                           /* dir */ {-1.f, 0.f, 0.f},
                           /* up  */ {0.f, 1.f, 0.f}};
    // Initialize the input status (no pressed keys).
    KeyPressStatus keyPressStatus{};
    // Initialize the timings.
    float  timeDelta = 0.f;
    uint64 cpuTime0, gpuTime0;
    std::tie(cpuTime0, gpuTime0) = engine.getTime();
    // Main loop.
    while (true) {
        // Drain the message queue.
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // Forward the message to the window.
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // Process the message locally.
            switch (msg.message) {
                case WM_KEYUP:
                case WM_KEYDOWN:
                    {
                        const uint status = msg.message ^ 0x1;
                        switch (msg.wParam) {
                            case 0x57: keyPressStatus.w = status; break;
                            case 0x53: keyPressStatus.s = status; break;
                            case 0x41: keyPressStatus.a = status; break;
                            case 0x44: keyPressStatus.d = status; break;
                            case 0x45: keyPressStatus.e = status; break;
                            case 0x51: keyPressStatus.q = status; break;
                        }
                    }
                    break;
                case WM_QUIT:
                    engine.stop();
                    // Return this part of the WM_QUIT message to Windows.
                    return static_cast<int>(msg.wParam);
            }
        }
        // Compute the camera movement parameters.
        {
            const float dist  = CAM_SPEED     * timeDelta;
            const float angle = CAM_ANG_SPEED * timeDelta;
            float totalDist   = 0.f;
            float totalPitch  = 0.f;
            float totalYaw    = 0.f;
            // Process keyboard input.
            if (keyPressStatus.w) totalDist  += dist;
            if (keyPressStatus.s) totalDist  -= dist;
            if (keyPressStatus.q) totalPitch += angle;
            if (keyPressStatus.e) totalPitch -= angle;
            if (keyPressStatus.d) totalYaw   += angle;
            if (keyPressStatus.a) totalYaw   -= angle;
            pCam.rotateAndMoveForward(totalPitch, totalYaw, totalDist);
        }
        // --> Fork engine tasks.
        auto asyncRec0 = std::async(std::launch::async, [&engine, &pCam, &scene](){
            // Thread 1.
            engine.recordGBufferPass(pCam, scene);
        });
        auto asyncRec1 = std::async(std::launch::async, [&engine, &pCam](){
            // Thread 2.
            engine.recordShadingPass(pCam);
        });
        // <-- Join engine tasks.
        asyncRec0.wait();
        asyncRec1.wait();
        engine.renderFrame();
        // Update the timings.
        {
            uint64 cpuTime1, gpuTime1;
            std::tie(cpuTime1, gpuTime1) = engine.getTime();
            const uint64 cpuFrameTime    = cpuTime1 - cpuTime0;
            const uint64 gpuFrameTime    = gpuTime1 - gpuTime0;
            // Convert the frame times from microseconds to milliseconds.
            Window::displayInfo(cpuFrameTime * 1e-3f, gpuFrameTime * 1e-3f);
            // Convert the frame time from microseconds to seconds.
            timeDelta = static_cast<float>(cpuFrameTime * 1e-6);
            cpuTime0  = cpuTime1;
            gpuTime0  = gpuTime1;
        }
    }
}
