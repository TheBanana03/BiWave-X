#include <stdlib.h>

int main() {
    int num_iters = 10;

    // there are errors in the building, but it still compiles successfully
    // make[1]: *** [Makefile:33: align_benchmark] Error 1
    // make[1]: Leaving directory '/home/jupyter-administrator/WFA2-lib/tools/align_benchmark'
    // make: *** [Makefile:90: tools/align_benchmark] Error 2
    if (system("bash build.sh") || 1) {
        for (int i = 0; i <= num_iters; i++) {
            system("bash run.sh");
        }
    }
    
    return 0;
}
