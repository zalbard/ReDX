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
    // Initialize the renderer
    D3D12::Renderer engine{resX, resY};
    // Open the window
    ShowWindow(Window::handle(), SW_SHOWNORMAL);
    // Main loop
    MSG msg;
    while (true) {
        // If the queue is not empty, retrieve a message
        if (PeekMessage(&msg, nullptr, 0u, 0u, PM_REMOVE)) {
            // Forward the message to the window
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // Process the message
            switch (msg.message) {
            case WM_KEYDOWN:
                // Process keyboard and mouse input
                break;
            case WM_PAINT:
                engine.renderFrame();
                break;
            case WM_QUIT:
                engine.stop();
                // Return this part of the WM_QUIT message to Windows
                return static_cast<int>(msg.wParam);
            }
        }
    }
}
