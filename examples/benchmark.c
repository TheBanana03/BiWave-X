#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main() {
    int num_iters = 10;
    struct timespec start, end;
    double total_time = 0.0;

    // there are errors in the building, but it still compiles successfully
    // make[1]: *** [Makefile:33: align_benchmark] Error 1
    // make[1]: Leaving directory '/home/jupyter-administrator/WFA2-lib/tools/align_benchmark'
    // make: *** [Makefile:90: tools/align_benchmark] Error 2
    if (system("bash build.sh") || 1) {
        for (int i = 0; i < num_iters; i++) {
            clock_gettime(CLOCK_MONOTONIC, &start);

            system("bash run.sh");

            clock_gettime(CLOCK_MONOTONIC, &end);

            // Compute elapsed time in nanoseconds
            double time_taken = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
            total_time += time_taken;

            printf("Iteration %d: Time taken = %.6fs (%.3fns)\n", i+1, time_taken/1e9, time_taken);
        }
    }

    double average_time = total_time/num_iters;
    printf("\nAverage execution time: %.3f ns (%.6f s)\n", average_time/1e9, average_time);
    
    return 0;
}
