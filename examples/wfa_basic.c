/*
 *                             The MIT License
 *
 * Wavefront Alignment Algorithms
 * Copyright (c) 2017 by Santiago Marco-Sola  <santiagomsola@gmail.com>
 *
 * This file is part of Wavefront Alignment Algorithms.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * PROJECT: Wavefront Alignment Algorithms
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION: WFA Sample-Code
 */
#include <time.h>
#include "wavefront/wavefront_align.h"
#include "wavefront/wavefront_plot.h"

int main(int argc, char* argv[]) {
    // Check if pattern and text are provided as command-line arguments
    // if (argc != 3) {
    //     fprintf(stderr, "Usage: %s <pattern> <text>\n", argv[0]);
    //     return 1;
    // }

    // // Pattern & Text from command-line arguments
    // char* pattern = argv[1];
    // char* text = argv[2];
    // char* pattern = "ACGGGCACCGCCTCGAAGGGCAAGTCGGGCGATTCGGGCGACGTGTGCCTCACCCAGGAGTGCATCCACACGGCTTCCACCGTTTTGCGGAAGATGAAGCCCGAGGTAGAGCCGTGCGACAACTTTTACGAGTTCGCCTGCGGCACATACCTTGAGGAGGAGAACATCCCCGACGACAAGGTCTCCATCAGCACCTTCTCCGTGATCTCCGACAAGCTGCAGGAGCAGTTGAAGGACATCATCACCGCGGAGCGACCGGAAACGGAGCCGAAGCACTTCCGACTGCCCAACCTGCTTTACAAGGCCTGCATGAACAAAACTCTGATCGAGACCCTGGGCCCTGAGCCCATAACTCGCGTGGCCGAGAGGCTGGGCGGCTGGCCGCTGATCAAGGGAGACTCTTGGAACGCGGACGACAGCTGGACTTGGCAGGAGCAGGTCAAGAAGTTCCGCACCGCCGGTTTTAGCATGGACTACATCATCGACTTTTCAATCGGAGTGGATTTGCAGAACAGCACTAAGCGGCTTATAGATCTGGACCAGTCCTCCCTGGCCTTGAGTCGCGAGTACCTCGTGAAAGGATTCAATGAGACGCTGGTGACGGCATACTACAAGTATATGGTTGACATCGCGGTTCTTTTCGGAGCAAATAGGGACTTGGCCAAGACGGAGCTGCTGTTATCTTTGGAGTTCGAGATGGCGCTGGCCAATATCTCCTGGCCCAACGAGAAGCGACGCAACTCCTCTGAGCTGTACAATCTAAGAACTCCAGCGCAGCTCCAGGCCGCCTACCCCTACGTGCAGTGGGTGGACTACATGAACGCCCTGCTGCCGGAGGGACTGAACGTGGCCGAGGACGAGATGATCAACCTGTCGGTGCCCAGCTTCTTCGAGGACCTTGGCAAGCTGCTGGCCAAGACGCCGAAGCGTGTCATCGCCAACTATATGTTCTGGCGTATCCACGGCTTCTCCGTGGGATTTCTGAGTGAGGAGTTCCGCA";
    // char* text = "GACGCCTTCAAACCTGCAGTCGGTAGTTGGTCATGTAGCGCATGCTATGTGAACAATCCCGGAGAGTCTCTTTACTGCAGCGCCTGTGATGCACCCAAGAATGACACGGTACCACAGAAGGAGAAGTCACTAGGATCGGGACTTAATCTGCCACCTACCTCCAAGTTTAGCTTTGGTTTCGGAGCCGCAGCGGCTGGTGACAAAGATCAGGCTGGCGATGGAGCCACGTTCAACTTCGCTGCGATGCCCGCAGCTGTTGCTCCGACTACCTCAATTGGATCCAGCAGCTTCACCTTCTCCATGACCAAGCCCAAGCCCGACCAGCAGCAGCCAAATAGTACAGCAGCTAAGGAGGATGAAGATAACGATTCGCAAGAGGTGGAGGAGGAGGAGAACAACACTTACTTTTCACCAGTAATCCCATTACCCGATAAGATTGATGTAAAAACAGGTGAGGAGGACGAGGAGCTATTGTATGTGCATAAGGCCAAGTTGTACCGCCTAAACGAGAGCGATTGGAAAGAGCGCGGCTTGGGGGATGTGAAGATCCTGCGCCATAGGCAGACCAAAAAGCTGCGCGTGGTCATGCGTCGGGAGCAGGTGTTCAAGATCTGTCTTAACCATGTGCTTAACGAGAATGTGGTGTACAGGGAGAAGACTGAAACGTCGTGGATGTTTGCTGTACACGATTTTAGCGAGGGCGAGAGTGTCCTAGAGCGATTCACTTTGCGTTTTAAAAACAAGGAGGTGGCACAAGGATTTATGGAAGCGATCAAAAATGCTCTTAATGAAACCGCAAAGCCGATAGAAGATAGCCCAGTGGTCGGCTCCGTTTCCCAATCGACGGAAGCAAATAAACCGTCCCAAAAAAATGATGGGGCGGCAAAGAGCAGAGGAGGCGAATCGGAAGTACTTGATGTTGGCAAAACCTCTTCTGTAAGGCCAACCACGCACGAAGTAATCCCTCCACTTCCCATGACCCTTCCCCTACTGACTCTCCGCA";
    
  char* pattern = NULL;
  char * text = NULL;
  //read from file
  FILE* file = fopen("/home/retraxius/inputs/input1.txt", "r");
  if (!file)
  {
    perror("Error opening file");
    return EXIT_FAILURE;
  }
  char* line = NULL;
  size_t len = 0;
  ssize_t read;

  while ((read = getline(&line, &len, file)) != -1)
  {
    if(read > 0 && line[read-1] == '\n') {
      line[read-1] = '\0';
    }
    if(line[0] != '>') {
      pattern = strdup(line);
    }
  }

  fclose(file);
  if (line)
  {
    free(line);
  }

  //read from file 2
  file = fopen("/home/retraxius/inputs/input2.txt", "r");
  if (!file)
  {
    perror("Error opening file");
    return EXIT_FAILURE;
  }
  line = NULL;
  len = 0;
  ssize_t read2;

  while ((read2 = getline(&line, &len, file)) != -1)
  {
    if(read2 > 0 && line[read2-1] == '\n') {
      line[read2-1] = '\0';
    }
    if(line[0] != '>') {
      text = strdup(line);
    }
  }

  fclose(file);
  if (line)
  {
    free(line);
  }

  
  
  
  // Configure alignment attributes
  wavefront_aligner_attr_t attributes = wavefront_aligner_attr_default;
  attributes.distance_metric = gap_affine;
  attributes.affine_penalties.match = 0;
  attributes.affine_penalties.mismatch = 4;
  attributes.affine_penalties.gap_opening = 6;
  attributes.affine_penalties.gap_extension = 2;
    attributes.alignment_form.span = alignment_end2end;
    attributes.memory_mode = wavefront_memory_ultralow;

    attributes.plot.enabled = 1 ;
    attributes.plot.resolution_points = 2000;
    attributes.plot.align_level = 0;
    

    wavefront_heuristic_set_none(&attributes.heuristic);
    
  // Initialize Wavefront Aligner
  wavefront_aligner_t* const wf_aligner = wavefront_aligner_new(&attributes);
    
  // Align
  //wavefront_align(wf_aligner,pattern,strlen(pattern),text,strlen(text));
  fprintf(stderr, "Start AVX-latest\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Align
    wavefront_align(wf_aligner, pattern, strlen(pattern), text, strlen(text));
    int score = cigar_score_gap_affine(wf_aligner->cigar, &attributes.affine_penalties);

    // Stop clock
    clock_gettime(CLOCK_MONOTONIC, &end);
    
  // fprintf(stderr,"WFA-Alignment returns score %d\n",wf_aligner->cigar->score);

  // // Display alignment
  // fprintf(stderr,"  PATTERN  %s\n",pattern);
  // fprintf(stderr,"  TEXT     %s\n",text);
  fprintf(stderr,"  SCORE (RE)COMPUTED %d\n",
       cigar_score_gap_affine(wf_aligner->cigar,&attributes.affine_penalties));
  cigar_print_pretty(stderr,wf_aligner->cigar,
      pattern,strlen(pattern),text,strlen(text));

     long long elapsed_time = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    
    printf("%d\n", cigar_score_gap_affine(wf_aligner->cigar,&attributes.affine_penalties));

    //FILE *file = fopen("/tmp/score.txt", "w");
    FILE *file2 = fopen("/home/retraxius/avx-latest/score.txt", "w");
    if (file2 == NULL) {
      perror("fopen");
      exit(EXIT_FAILURE);
    }
    //int32_t tempscore = score;
    //fprintf(file, "%" PRId32 "\n", tempscore);
    fprintf(file2, "%d %lld\n", score, elapsed_time);
    fclose(file2);
    
    // wavefront_plot_print(stderr, wf_aligner);
  // Free
  wavefront_aligner_delete(wf_aligner);
}