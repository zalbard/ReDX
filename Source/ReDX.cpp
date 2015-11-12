#include "Common\Utility.h"
#include "UI\Window.h"
#include "D3D12\Renderer.h"

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
    static const LONG resX = 1280;
    static const LONG resY = 720;
    // Open a window for rendering output
    Window::create(resX, resY);
    // Initialize the renderer
    D3D12::Renderer engine{resX, resY, Window::handle()};
    // Main loop
    MSG msg = {};
    while (WM_QUIT != msg.message) {
        // Process the messages in the queue
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}
