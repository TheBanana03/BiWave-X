#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void generate_dna(char *sequence, int length) {
    const char bases[] = "ACGT";
    for (int i = 0; i < length; i++) {
        sequence[i] = bases[rand() % 4];
    }
    sequence[length] = '\0';
}

int count_sequences(const char* filename) {
    FILE *file = fopen(filename, "r");
    
    if (!file) {
        perror("Error opening file");
        return -1;
    }
    
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int count = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        if (line[0] == '>')
            count++;
    }

    free(line);
    fclose(file);
    return count;
}

char* extract_sequence(const char* filename, int target_line) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    int curr_line = -1;
    char* line = NULL;
    char* sequence = NULL;
    size_t len = 0;
    ssize_t read;
 
    while ((read = getline(&line, &len, file)) != -1) {
        if (line[0] == '>') {
            curr_line++;
            continue;
        }
        if (curr_line == target_line) {
            if (read > 0 && line[read - 1] == '\n') {
                line[read - 1] = '\0';
            }

            char* seq_start = strchr(line, '|');
            if (seq_start) {
                seq_start = strchr(seq_start + 1, '|');
                if (seq_start) {
                    seq_start++;
                } else {
                    seq_start = line;
                }
            } else {
                seq_start = line;
            }

            sequence = strdup(seq_start);
            break;
        }
    }

    fclose(file);
    if (line) {
        free(line);
    }
    return sequence;
}

void print_sequences(char* pattern, char* text, int pattern_number, int text_length) {
    printf("Pattern (Seq %d):\t%s\n", pattern_number, pattern);
    if (text_length > 20) {
        printf("Text (%d):\t\t%.20s...\n", text_length, text);
    } else {
        printf("Text (%d):\t\t%s\n", text_length, text);
    }
}

void execute_wfa_basic(char* pattern, char* text, bool avx) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        const char *wfa_path = avx ? "./wfa_basic" : "/home/jupyter-administrator/WFA/WFA2-lib/examples/wfa_basic";
        char *args[] = {(char *)wfa_path, (char *)pattern, (char *)text, NULL};
        execvp(args[0], args);
        
        // If execlp fails, print error and exit child process
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process: Wait for child to complete
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            // printf("wfa_basic exited with status %d\n", WEXITSTATUS(status));
        } else {
            fprintf(stderr, "wfa_basic terminated abnormally\n");
        }
    }
}

int main() {
    int num_iters = 10;
    int num_seq = 5;
    
    struct timespec start, end;

    long long total_time[2];
    long long average_time[2];
    long long total_time_per_len[2];
    long long average_time_per_len[2];
    long long time_taken[2];

    int num_len = 5;
    const char* file_names[] = {"150", "300", "500", "750", "1000"};
    const char* ref_file = "./test_cases/UNIPROT_DNA_ALL.fasta.txt";
    char curr_file[50];
    
    srand(time(NULL));

    // there are errors in the building, but it still compiles successfully
    // make[1]: *** [Makefile:33: align_benchmark] Error 1
    // make[1]: Leaving directory '/home/jupyter-administrator/WFA2-lib/tools/align_benchmark'
    // make: *** [Makefile:90: tools/align_benchmark] Error 2
    if (system("bash build.sh") || 1) {
        for (int k = 0; k < num_len; k++) {
            
            snprintf(curr_file, sizeof(curr_file), "./test_cases/%s.txt", file_names[k]);
            
            int total_seq = count_sequences(curr_file);
            if (total_seq <= 0) {
                fprintf(stderr, "No sequences found in file.\n");
                return EXIT_FAILURE;
            }

            total_time_per_len[0] = 0;
            average_time_per_len[0] = 0;

            total_time_per_len[1] = 0;
            average_time_per_len[1] = 0;

            printf("Testing for text length %s\n", file_names[k]);
            
            for (int i = 0; i < num_seq; i++) {
                int pattern_length = 150;
    
                int text_length = (rand()%10000)+1;
                // char* text = (char*)malloc(text_length+1);
                // if (!text) {
                //     perror("Memory allocation failed.");
                //     exit(EXIT_FAILURE);
                // }
                
                // generate_dna(text, text_length);
                char* text = extract_sequence(ref_file, 0);
                
                int target_line = rand() % total_seq;
                char* pattern = extract_sequence(curr_file, target_line);
                if (!pattern) {
                    fprintf(stderr, "Failed to get random pattern. Skipping...\n");
                    free(text);
                    continue;
                }
                
                // print_sequences(pattern, text, target_line, text_length);
                
                total_time[0] = 0;
                average_time[0] = 0;
                time_taken[0] = 0;

                total_time[1] = 0;
                average_time[1] = 0;
                time_taken[1] = 0;
                
                for (int j = 0; j < num_iters; j++) {
                    // Test avx
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    execute_wfa_basic(pattern, text, true);
                    clock_gettime(CLOCK_MONOTONIC, &end);
                    time_taken[0] = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
                    total_time[0] += time_taken[0];
                    // printf("Iteration %d: Time taken = %.6fs (%lldns)\n", j+1, time_taken[0]/1e9, time_taken[0]);

                    // Test original
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    execute_wfa_basic(pattern, text, false);
                    clock_gettime(CLOCK_MONOTONIC, &end);
                    time_taken[1] = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
                    total_time[1] += time_taken[1];
                }
                
                average_time[0] = total_time[0]/num_iters;
                total_time_per_len[0] += average_time[0];
                // printf("Average execution time: %.6fs (%lldns)\n\n", average_time[0]/1e9, average_time[0]);
                average_time[1] = total_time[1]/num_iters;
                total_time_per_len[1] += average_time[1];
    
                free(pattern);
                free(text);
            }
            average_time_per_len[0] += total_time_per_len[0]/num_len;
            average_time_per_len[1] += total_time_per_len[1]/num_len;
            printf("Average execution time for avx vers: %.6fs (%lldns)\n", average_time_per_len[0]/1e9, average_time_per_len[0]);
            printf("Average execution time for original: %.6fs (%lldns)\n", average_time_per_len[1]/1e9, average_time_per_len[1]);
        }
    }
    
    return 0;
}
