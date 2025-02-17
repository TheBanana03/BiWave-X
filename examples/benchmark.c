#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void generate_dna(char *sequence, int length) {
    const char bases[] = "ACGT";
    for (int i = 0; i < length; i++) {
        sequence[i] = bases[rand() % 4];
    }
    sequence[length] = '\0';
}

char* generate_command() {
    int pattern_length = (rand()%10000)+1;
    int text_length = (rand()%10000)+1;

    char* pattern = (char*)malloc(pattern_length+1);
    char* text = (char*)malloc(text_length+1);
    
    if (!pattern || !text) {
        perror("Memory allocation failed.");
        exit(EXIT_FAILURE);
    }

    generate_dna(pattern, pattern_length);
    generate_dna(text, text_length);

    if (pattern_length > 20) {
        printf("Pattern (%d): %.20s...\n", pattern_length, pattern);
    } else {
        printf("Pattern (%d): %s\n", pattern_length, pattern);
    }
    if (text_length > 20) {
        printf("Text    (%d): %.20s...\n", text_length, text);
    } else {
        printf("Text    (%d): %s\n", text_length, text);
    }

    // Allocate memory for command
    size_t command_size = pattern_length + text_length + 50;
    char *command = (char*)malloc(command_size);
    
    if (!command) {
        perror("Memory allocation failed for command.");
        free(pattern);
        free(text);
        exit(EXIT_FAILURE);
    }

    snprintf(command, command_size, "./wfa_basic %s %s", pattern, text);

    free(pattern);
    free(text);

    return command;
}

int main() {
    int num_iters = 10;
    int num_sequences = 5;
    struct timespec start, end;

    long long total_time;
    long long average_time;
    long long time_taken;

    // there are errors in the building, but it still compiles successfully
    // make[1]: *** [Makefile:33: align_benchmark] Error 1
    // make[1]: Leaving directory '/home/jupyter-administrator/WFA2-lib/tools/align_benchmark'
    // make: *** [Makefile:90: tools/align_benchmark] Error 2
    if (system("bash build.sh") || 1) {
        for (int i = 0; i < num_sequences; i++) {
            
            char* command = generate_command();
            
            total_time = 0;
            average_time = 0;
            time_taken = 0;
            
            for (int j = 0; j < num_iters; j++) {
                clock_gettime(CLOCK_MONOTONIC, &start);
    
                system(command);
    
                clock_gettime(CLOCK_MONOTONIC, &end);
    
                // Compute elapsed time in nanoseconds
                time_taken = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
                total_time += time_taken;
    
                printf("Iteration %d: Time taken = %.6fs (%lldns)\n", j+1, time_taken/1e9, time_taken);
            }
            
            average_time = total_time/num_iters;
            printf("\nAverage execution time: %.6fs (%lldns)\n\n", average_time/1e9, average_time);
            
            free(command);
        }
    }
    
    return 0;
}
