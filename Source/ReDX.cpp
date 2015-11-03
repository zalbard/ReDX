#include <cstdio>
#include <d3d12.h>

int main(const int argc, const char* argv[]) {
	if (argc > 1) {
		printf("The following command line arguments have been ignored:\n");
        for (auto i = 1; i < argc; ++i) {
            printf("%s\n", argv[i]);
        }
	}
	return 0;
}
