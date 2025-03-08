#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>

struct match {
    int score;
    char* text;
    int text_index;
    long long time[2];
};

void check_nonprintable(const char* str, const char* var_name) {
    int has_nonprintable = 0;

    printf("Checking %s:\n", var_name);
    for (size_t i = 0; i < strlen(str); i++) {
        unsigned char ch = (unsigned char)str[i];

        // Check if character is printable
        if (!isprint(ch)) {
            if (!has_nonprintable) {
                printf("Raw text before processing: \"%s\"\n", str);

                printf("WARNING: Non-printable characters found in %s!\n", var_name);
                has_nonprintable = 1;
            }
            printf("Index %zu: Char '%c' (HEX: %02X)\n", i, ch, ch);
        }
    }

    if (!has_nonprintable) {
        printf("%s is clean (all printable characters).\n", var_name);
    }
    printf("\n");
}

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

void print_sequences(char* pattern, char* text, int pattern_number, int text_number) {
    printf("Pattern (Seq %d):\t%s\n", pattern_number, pattern);
    printf("Text (Seq %d):\t%s\n", text_number, text);
}

// int execute_wfa_basic(char* pattern, char* text, bool avx) {
//     const char *wfa_path = avx ? "./wfa_basic" : "/home/jupyter-administrator/WFA/WFA2-lib/examples/wfa_basic";

//     // Construct command
//     char command[1024];
//     snprintf(command, sizeof(command), "%s %s %s", wfa_path, pattern, text);

//     // Open process
//     FILE *fp = popen(command, "r");
//     if (!fp) {
//         perror("popen failed");
//         return -1;
//     }

//     // Read score from output
//     int score = -1;
//     if (fscanf(fp, "%d", &score) != 1) {
//         fprintf(stderr, "Failed to read score from wfa_basic.\n");
//     }

//     pclose(fp);
//     return score;
// }

void read_metrics(const char* file_name, int* score, long long* time_taken) {
    FILE *file = fopen(file_name, "r");
    if (!file) {
        perror("Error opening score.txt");
        return;
    }
    
    if (fscanf(file, "%d %lld", score, time_taken) != 2) {
        fprintf(stderr, "Failed to read score and time from score.txt\n");
    }

    fclose(file);
}

void execute_wfa_basic(char* pattern, char* text, bool avx, int* score, long long* time_taken) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        const char* wfa_path = avx ? "./wfa_basic" : "/home/jupyter-administrator/WFA/WFA2-lib/examples/wfa_basic";
        char *args[] = {(char*)wfa_path, (char*)pattern, (char*)text, NULL};
        // printf("%s, %s, %s", args[0], args[1], args[2]);
        execvp(args[0], args);
        
        // If execlp fails, print error and exit child process
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process: Wait for child to complete
        int status;
        waitpid(pid, &status, 0);

        const char* file_name = avx ? "./score.txt" : "/home/jupyter-administrator/WFA/WFA2-lib/examples/score.txt";
        read_metrics(file_name, score, time_taken);
        
        if (WIFEXITED(status)) {
            // printf("wfa_basic exited with status %d\n", WEXITSTATUS(status));
        } else {
            fprintf(stderr, "wfa_basic terminated abnormally\n");
        }
    }
}

void update_top_results(struct match best_matches[], struct match curr_match) {
    for (int i = 0; i < 5; i++) {
        if (curr_match.score > best_matches[i].score) {
            for (int k = 4; k > i; k--) {
                if (best_matches[k].text != NULL)
                    free(best_matches[k].text);
                
                best_matches[k] = best_matches[k-1];
                
                if (best_matches[k-1].text != NULL)
                    best_matches[k].text = strdup(best_matches[k-1].text);
            }
            if (best_matches[i].text != NULL)
                free(best_matches[i].text);
            
            best_matches[i] = curr_match;
            best_matches[i].text = strdup(curr_match.text);
            break;
        }
    }
}

int get_max_score(int score_1, int score_2) {
    return (score_1*(score_1>=score_2)) + (score_2*(score_2>score_1));
}

void write_output(const char* file_name, char* text, char* pattern, int text_index, int pattern_index, long long* time, int score) {
    const char* dir_name = "output";

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s.txt", dir_name, file_name);

    FILE* file = fopen(full_path, "a+");
    
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "Pattern\t[Sequence %04d] (Length %05ld): %s\n", pattern_index, strlen(pattern), pattern);
    fprintf(file, "Text\t[Sequence %04d] (Length %05ld): %s\n", text_index, strlen(text), text);
    fprintf(file, "Execution Time (Original)\t: %lld ns\n", time[1]);
    fprintf(file, "Execution Time (AVX)\t\t: %lld ns\n", time[0]);
    fprintf(file, "Score: %d\n\n", score);
    fprintf(file, "----------------------------\n\n");

    fclose(file);
}

void clear_file(const char* file_name) {
    const char* dir_name = "output";

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s.txt", dir_name, file_name);

    FILE* file = fopen(full_path, "w");

    if (!file) {
        perror("Error opening file for clearing");
        exit(EXIT_FAILURE);
    }

    fclose(file);
}

int main() {
    int num_iters = 1;
    int num_seq = 5;
    
    struct timespec start, end;
    struct match best_matches[5];
    struct match curr_match;

    for (int i = 0; i < 5; i++) {
        best_matches[i].score = -2147483648;
        best_matches[i].text = NULL;
        best_matches[i].text_index = 0;
        best_matches[i].time[0] = 0;
        best_matches[i].time[1] = 0;
    }

    int total_score[2] = {0, 0};
    int curr_score[2];
    float average_score[2];
    float average_score_per_len[2];
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

    int total_text = count_sequences(ref_file);
    int num_text = 10;
    if (total_text <= 0) {
        fprintf(stderr, "No sequences found in file.\n");
        return EXIT_FAILURE;
    }

    // there are errors in the building, but it still compiles successfully
    // make[1]: *** [Makefile:33: align_benchmark] Error 1
    // make[1]: Leaving directory '/home/jupyter-administrator/WFA2-lib/tools/align_benchmark'
    // make: *** [Makefile:90: tools/align_benchmark] Error 2
    
    if (/*system("bash build.sh") ||*/ 1) {
        for (int k = 0; k < num_len; k++) {
            
            snprintf(curr_file, sizeof(curr_file), "./test_cases/%s.txt", file_names[k]);
            clear_file(file_names[k]);
            
            int total_seq = count_sequences(curr_file);
            if (total_seq <= 0) {
                fprintf(stderr, "No sequences found in file.\n");
                return EXIT_FAILURE;
            }

            total_time_per_len[0] = 0;
            average_time_per_len[0] = 0;
            average_score_per_len[0] = 0;

            total_time_per_len[1] = 0;
            average_time_per_len[1] = 0;
            average_score_per_len[1] = 0;
            
            for (int i = 0; i < num_seq; i++) {
                int pattern_length = 150;
    
                int text_length = (rand()%10000)+1;
                // char* text = (char*)malloc(text_length+1);
                // if (!text) {
                //     perror("Memory allocation failed.");
                //     exit(EXIT_FAILURE);
                // }
                
                // generate_dna(text, text_length);
                
                int target_line = rand() % total_seq;
                char* pattern = extract_sequence(curr_file, target_line);
                if (!pattern) {
                    fprintf(stderr, "Failed to get random pattern. Skipping...\n");
                    continue;
                }
                
                total_time[0] = 0;
                average_time[0] = 0;
                time_taken[0] = 0;
                total_score[0] = 0;

                total_time[1] = 0;
                average_time[1] = 0;
                time_taken[1] = 0;
                total_score[1] = 0;

                for (int i = 0; i < 5; i++) {
                    best_matches[i].score = -2147483648;
                    best_matches[i].text = NULL;
                    best_matches[i].text_index = 0;
                    best_matches[i].time[0] = 0;
                    best_matches[i].time[1] = 0;
                }

                for (int l = 0; l < num_text; l++) {
                    int target_text = l;
                    char* text = extract_sequence(ref_file, target_text);
                    if (!text) {
                        fprintf(stderr, "Failed to get random text. Skipping...\n");
                        free(pattern);
                        continue;
                    }

                    // print_sequences(pattern, text, target_line, text_length);
                    
                    for (int j = 0; j < num_iters; j++) {
                        // Test avx
                        execute_wfa_basic(pattern, text, true, &curr_score[0], &time_taken[0]);
                        total_time[0] += time_taken[0];
                        // printf("Iteration (A) %d: Time taken = %.6fs (%lldns)\t", j+1, time_taken[0]/1e9, time_taken[0]);

                        // Test original
                        execute_wfa_basic(pattern, text, false, &curr_score[1], &time_taken[1]);
                        total_time[1] += time_taken[1];
                        // printf("Iteration (O) %d: Time taken = %.6fs (%lldns)\n", j+1, time_taken[1]/1e9, time_taken[1]);

                        if (curr_score[0] != curr_score[1]) {
                            printf("Error: Differing scores detected.\n");
                            print_sequences(pattern, text, target_line, text_length);
                            printf("AVX Score: %d\tOriginal Score: %d\n", curr_score[0], curr_score[1]);
                            printf("Exiting...\n");
                            free(pattern);
                            free(text);
                            return 1;
                        }
                        total_score[0] += curr_score[0];
                        total_score[1] += curr_score[1];
                        
                        curr_match.score = curr_score[0];
                        curr_match.text = text;
                        curr_match.text_index = target_text;
                        curr_match.time[0] = time_taken[0];
                        curr_match.time[1] = time_taken[1];

                        update_top_results(best_matches, curr_match);

                        write_output(file_names[k], text, pattern, target_text, target_line, time_taken, curr_score[0]);
                        // const char* dir_name = "output";

                        // char full_path[256];
                        // snprintf(full_path, sizeof(full_path), "%s/%s.txt", dir_name, file_names[k]);
                        // full_path[sizeof(full_path) - 1] = '\0';
                    
                        // FILE* file = fopen(full_path, "a+");
                        
                        // if (!file) {
                        //     perror("Error opening file");
                        //     exit(EXIT_FAILURE);
                        // }
                        
                        // fprintf(file, "Pattern\t[Sequence %04d] (Length %05ld): %s\n", target_line, strlen(pattern), pattern);
                        // fprintf(file, "Text\t[Sequence %04d] (Length %05ld): %s\n", target_text, strlen(text), text);
                        // fprintf(file, "Execution Time (Original)\t: %lld ns\n", time_taken[1]);
                        // fprintf(file, "Execution Time (AVX)\t\t: %lld ns\n", time_taken[0]);
                        // fprintf(file, "Score: %d\n\n", curr_score[0]);
                        // fprintf(file, "----------------------------\n\n");
                    
                        // fclose(file);
                    }

                    free(text);
                    if (!(l%100)) {
                        printf("%.2f%% finished (%d/%d).\n",
                                (((k*num_seq*num_text)+(i*num_text)+l*1.0)/(num_len*num_seq*num_text))*100.0,
                                (k*num_seq*num_text)+(i*num_text)+l, num_len*num_seq*num_text);
                    }
                }
                
                average_time[0] = total_time[0]/num_iters;
                total_time_per_len[0] += average_time[0];
                average_score[0] = (total_score[0]*1.0)/num_iters;

                average_time[1] = total_time[1]/num_iters;
                total_time_per_len[1] += average_time[1];
                average_score[1] = (total_score[1]*1.0)/num_iters;
                // printf("Average execution time: %.6fs (%lldns)\t%.6fs (%lldns)\n\n", average_time[0]/1e9, average_time[0], average_time[1]/1e9, average_time[1]);

                char file_path[256];
                snprintf(file_path, sizeof(file_path), "best_score/%s/%d", file_names[k], target_line);
                clear_file(file_path);
                for (int m = 0; m < 5; m++) {
                    write_output(file_path, best_matches[m].text, pattern, best_matches[m].text_index, 
                                target_line, best_matches[m].time, best_matches[m].score);

                    // const char* dir_name = "output";

                    // char full_path[256];
                    // snprintf(full_path, sizeof(full_path), "%s/%s.txt", dir_name, file_path);
                    // full_path[sizeof(full_path) - 1] = '\0';
                
                    // FILE* file = fopen(full_path, "a+");
                    
                    // if (!file) {
                    //     perror("Error opening file");
                    //     exit(EXIT_FAILURE);
                    // }
                    
                    // fprintf(file, "Pattern\t[Sequence %04d] (Length %05ld): %s\n", 
                    //                 target_line, strlen(pattern), pattern);
                    // fprintf(file, "Text\t[Sequence %04d] (Length %05ld): %s\n",
                    //                 best_matches[m].text_index, strlen(best_matches[m].text), best_matches[m].text);
                    // fprintf(file, "Execution Time (Original)\t: %lld ns\n", best_matches[m].time[1]);
                    // fprintf(file, "Execution Time (AVX)\t\t: %lld ns\n", best_matches[m].time[0]);
                    // fprintf(file, "Score: %d\n\n", best_matches[m].score);
                    // fprintf(file, "----------------------------\n\n");
                    
                    // fclose(file);
                }

                printf("Pattern %d finished.\n\n", i);
                free(pattern);
            }

            average_time_per_len[0] = total_time_per_len[0]/num_len;
            average_time_per_len[1] = total_time_per_len[1]/num_len;
            average_score_per_len[0] = average_score[0]/num_len;
            average_score_per_len[1] = average_score[1]/num_len;
            // printf("| Text Length: %s\t|\t%.6fs (%lldns)\t|\t%.6fs (%lldns)\t|\t%d\t|\n", file_names[k], average_time_per_len[0]/1e9, average_time_per_len[0],  average_time_per_len[1]/1e9, average_time_per_len[1], best_score);
        }
    }
    
    return 0;
}