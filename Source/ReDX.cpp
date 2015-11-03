#include <cstdio>
#include <d3d12.h>
#include <Windows.h>
#include <DirectXMath.h>

int main(const int argc, const char* argv[]) {
    // Parse command line arguments
	if (argc > 1) {
		fprintf(stderr, "The following command line arguments have been ignored:\n");
        for (auto i = 1; i < argc; ++i) {
            fprintf(stderr, "%s ", argv[i]);
        }
        fputs("\n", stderr);
	}
    // Verify SSE2 support for DirectXMath library
    if (!DirectX::XMVerifyCPUSupport()) {
        fprintf(stderr, "The CPU doesn't support SSE2. Aborting.\n");
        return -1;
    }
	return 0;
}
