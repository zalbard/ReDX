#include "Common\Utility.h"
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
    static const long resX = 1280;
    static const long resY = 720;
    // Initialize the renderer
    D3D12::Renderer engine{resX, resY};
    // Main loop
    MSG msg = {};
    while (WM_QUIT != msg.message) {
        // Process the messages in the queue
        if (PeekMessage(&msg, nullptr, 0u, 0u, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    // Return this part of the WM_QUIT message to Windows
    return static_cast<int>(msg.wParam);
}
