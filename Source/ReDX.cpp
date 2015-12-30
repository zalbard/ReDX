#include "Common\Utility.h"
#include "D3D12\Renderer.h"
#include "UI\Window.h"

int main(const int argc, const char* argv[]) {
    // Parse command line arguments
	if (argc > 1) {
		printError("The following command line arguments have been ignored:");
        for (auto i = 1; i < argc; ++i) {
            printError("%s", argv[i]);
        }
	}
    // Verify SSE2 support for the DirectXMath library
    if (!DirectX::XMVerifyCPUSupport()) {
        printError("The CPU doesn't support SSE2. Aborting.");
        return -1;
    }
    constexpr long resX = 1280;
    constexpr long resY = 720;
    // Create a window for rendering output
    Window::open(resX, resY);
    // Initialize the renderer (internally uses the Window)
    D3D12::Renderer engine;
    // Main loop
    while (true) {
        MSG msg;
        // If the queue is not empty, retrieve a message
        while (PeekMessage(&msg, nullptr, 0u, 0u, PM_REMOVE)) {
            // Forward the message to the window
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // Process the message locally
            switch (msg.message) {
            case WM_KEYDOWN:
                // TODO: Process keyboard input
                break;
            case WM_QUIT:
                engine.stop();
                // Return this part of the WM_QUIT message to Windows
                return static_cast<int>(msg.wParam);
            }
        }
        // The message queue is now empty; execute engine code
        engine.renderFrame();
    }
}
