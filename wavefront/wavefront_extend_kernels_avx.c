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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS Oavx_wavefront_extensionR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * PROJECT: Wavefront Alignment Algorithms
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION: WaveFront-Alignment module for the "extension" of exact matches
 */

#include "wavefront_extend.h"
#include "wavefront_align.h"
#include "wavefront_compute.h"
#include "wavefront_heuristic.h"
#include "wavefront_extend_kernels.h"
#include "wavefront_extend_kernels_avx.h"
#include "wavefront_termination.h"

#if __AVX2__
#include <immintrin.h>

extern void avx_wavefront_extension_iteration(__m512i* offsets, __m512i* ks, __mmask16* mask, const char* pattern, const char* text, int* debug);

/*
 * Wavefront-Extend Inner Kernel (Scalar)
 */
FORCE_INLINE wf_offset_t wavefront_extend_matches_packed_kernel(
    wavefront_aligner_t* const wf_aligner,
    const int k,
    wf_offset_t offset) {
  // Fetch pattern/text blocks
  uint64_t* pattern_blocks = (uint64_t*)(wf_aligner->sequences.pattern+WAVEFRONT_V(k,offset));
  uint64_t* text_blocks = (uint64_t*)(wf_aligner->sequences.text+WAVEFRONT_H(k,offset));
  // Compare 64-bits blocks
  uint64_t cmp = *pattern_blocks ^ *text_blocks;
  while (__builtin_expect(cmp==0,0)) {
    // Increment offset (full block)
    offset += 8;
    // Next blocks
    ++pattern_blocks;
    ++text_blocks;
    // Compare
    cmp = *pattern_blocks ^ *text_blocks;
  }
  // Count equal characters
  const int equal_right_bits = __builtin_ctzl(cmp);
  const int equal_chars = DIV_FLOOR(equal_right_bits,8);
  offset += equal_chars;

    
  // Return extended offset
  return offset;
}

/*
 * SIMD clz, use a native instruction when available (AVX512 CD or VL
 * extensions), or emulate the clz behavior.
 */
FORCE_INLINE  __m256i avx2_lzcnt_epi32(__m256i v) {
  #if __AVX512CD__ && __AVX512VL__
    return _mm256_lzcnt_epi32(v);
  #else
    // Emulate clz for AVX2: https://stackoverflow.com/a/58827596
    v = _mm256_andnot_si256(_mm256_srli_epi32(v,8),v); // keep 8 MSB
    v = _mm256_castps_si256(_mm256_cvtepi32_ps(v)); // convert an integer to float
    v = _mm256_srli_epi32(v,23); // shift down the exponent
    v = _mm256_subs_epu16(_mm256_set1_epi32(158),v); // undo bias
    v = _mm256_min_epi16(v,_mm256_set1_epi32(32)); // clamp at 32
    return v;
  #endif
}



/*
 * Wavefront-Extend Inner Kernel (SIMD AVX2)
 */
FORCE_NO_INLINE void wavefront_extend_matches_packed_end2end_avx2(
    wavefront_aligner_t* const wf_aligner,
    wavefront_t* const mwavefront,
    const int lo,
    const int hi) {
  // Parameters
  wf_offset_t* const offsets = mwavefront->offsets;
  int k_min = lo;
  int k_max = hi;
  const char* pattern = wf_aligner->sequences.pattern;
  const char* text = wf_aligner->sequences.text;
  const __m256i vector_null = _mm256_set1_epi32(-1);
  const __m256i eights = _mm256_set1_epi32(8);
  const __m256i vecShuffle = _mm256_set_epi8(28,29,30,31,24,25,26,27,
                                             20,21,22,23,16,17,18,19,
                                             12,13,14,15, 8, 9,10,11,
                                             4 , 5, 6, 7, 0, 1, 2 ,3);
  const int elems_per_register = 8;
  int num_of_diagonals = k_max - k_min + 1;
  int loop_peeling_iters = num_of_diagonals % elems_per_register;
  int k;
  for (k=k_min;k<k_min+loop_peeling_iters;k++) {
    const wf_offset_t offset = offsets[k];
    if (offset < 0) continue;
    // Extend offset
    offsets[k] = wavefront_extend_matches_packed_kernel(wf_aligner,k,offset);
  }
  if (num_of_diagonals < elems_per_register) return;

  k_min += loop_peeling_iters;
  __m256i ks = _mm256_set_epi32 (
      k_min+7,k_min+6,k_min+5,k_min+4,
      k_min+3,k_min+2,k_min+1,k_min);


  for (k=k_min; k<=k_max; k+=elems_per_register) {
    __m256i offsets_vector = _mm256_lddqu_si256 ((__m256i*)&offsets[k]);
    __m256i h_vector       = offsets_vector;
    __m256i v_vector       = _mm256_sub_epi32(offsets_vector,ks);
    
    // NULL offsets will read at index 0 (avoid segfaults)
    __m256i null_mask = _mm256_cmpgt_epi32(offsets_vector, vector_null);
    v_vector = _mm256_and_si256(null_mask, v_vector);
    h_vector = _mm256_and_si256(null_mask, h_vector);
    
    __m256i pattern_vector = _mm256_i32gather_epi32((int const*)&pattern[0],v_vector,1);
    __m256i text_vector    = _mm256_i32gather_epi32((int const*)&text[0],h_vector,1);
    __m256i vector_mask    = _mm256_cmpeq_epi32(text_vector, pattern_vector);
    int mask               = _mm256_movemask_epi8(vector_mask);

    __m256i xor_result_vector = _mm256_xor_si256(pattern_vector,text_vector);
    xor_result_vector         = _mm256_shuffle_epi8(xor_result_vector, vecShuffle);
    __m256i clz_vector        = avx2_lzcnt_epi32(xor_result_vector);

    __m256i equal_chars = _mm256_srli_epi32(clz_vector,3);
    //equal_chars         = _mm256_and_si256(null_mask, equal_chars);
    offsets_vector      = _mm256_add_epi32 (offsets_vector,equal_chars);
    ks                  = _mm256_add_epi32 (ks, eights);

    _mm256_storeu_si256((__m256i*)&offsets[k],offsets_vector);
    
    if(mask == 0) continue;

    while (mask != 0) {
      int tz = __builtin_ctz(mask);
      int curr_k = k + (tz/4);
      const wf_offset_t offset = offsets[curr_k];
      // Extend offset
      if (offset >= 0) {
        offsets[curr_k] = wavefront_extend_matches_packed_kernel(wf_aligner,curr_k,offset);
      } else {
        offsets[curr_k] = WAVEFRONT_OFFSET_NULL;
      }
      mask &= (0xfffffff0 << tz);
    }
  }
}


FORCE_NO_INLINE wf_offset_t wavefront_extend_matches_packed_end2end_max_avx2(
    wavefront_aligner_t* const wf_aligner,
    wavefront_t* const mwavefront,
    const int lo,
    const int hi) {
  // Parameters
  
  const int elems_per_register = 8;
  const char* pattern = wf_aligner->sequences.pattern;
  const char* text    = wf_aligner->sequences.text;
  
  wf_offset_t* const offsets = mwavefront->offsets;
  wf_offset_t max_antidiag   = 0;
  
  int k_min = lo;
  int k_max = hi;
 
  int num_of_diagonals   = k_max - k_min + 1;
  int loop_peeling_iters = num_of_diagonals % elems_per_register;
  int k;
  
  const __m256i vector_null = _mm256_set1_epi32(-1);
  const __m256i eights      = _mm256_set1_epi32(8);
  const __m256i vecShuffle  = _mm256_set_epi8(28,29,30,31,24,25,26,27,
                                              20,21,22,23,16,17,18,19,
                                              12,13,14,15, 8, 9,10,11,
                                              4 , 5, 6, 7, 0, 1, 2 ,3);
 

  for (k=k_min;k<k_min+loop_peeling_iters;k++) {
    wf_offset_t offset = offsets[k];
    if (offset < 0) continue;
    offset     =  wavefront_extend_matches_packed_kernel(wf_aligner,k,offset);  
    offsets[k] = offset;
    const wf_offset_t antidiag = WAVEFRONT_ANTIDIAGONAL(k,offset);
    if (max_antidiag < antidiag) max_antidiag = antidiag;
  }

  if (num_of_diagonals < elems_per_register) return max_antidiag;

  k_min += loop_peeling_iters;
  __m256i max_antidiag_v = _mm256_set1_epi32(max_antidiag);
  __m256i ks = _mm256_set_epi32 (
      k_min+7,k_min+6,k_min+5,k_min+4,
      k_min+3,k_min+2,k_min+1,k_min);


  for (k=k_min; k<=k_max; k+=elems_per_register) {
    __m256i offsets_vector = _mm256_lddqu_si256 ((__m256i*)&offsets[k]);
    __m256i h_vector       = offsets_vector;
    __m256i v_vector       = _mm256_sub_epi32(offsets_vector,ks);
    
    // NULL offsets will read at index 0 (avoid segfaults)
    __m256i null_mask = _mm256_cmpgt_epi32(offsets_vector, vector_null);
    v_vector = _mm256_and_si256(null_mask, v_vector);
    h_vector = _mm256_and_si256(null_mask, h_vector);
    
    __m256i pattern_vector = _mm256_i32gather_epi32((int const*)&pattern[0],v_vector,1);
    __m256i text_vector    = _mm256_i32gather_epi32((int const*)&text[0],h_vector,1);
    __m256i vector_mask    = _mm256_cmpeq_epi32(text_vector, pattern_vector);
    int mask               = _mm256_movemask_epi8(vector_mask);

    __m256i xor_result_vector = _mm256_xor_si256(pattern_vector,text_vector);
    xor_result_vector         = _mm256_shuffle_epi8(xor_result_vector, vecShuffle);
    __m256i clz_vector        = avx2_lzcnt_epi32(xor_result_vector);

    __m256i equal_chars = _mm256_srli_epi32(clz_vector,3);
    offsets_vector      = _mm256_add_epi32 (offsets_vector, equal_chars);

    _mm256_storeu_si256((__m256i*)&offsets[k],offsets_vector);

    __m256i offset_max = _mm256_and_si256(null_mask, offsets_vector);
    offset_max         = _mm256_slli_epi32(offset_max, 1); 
    offset_max         = _mm256_sub_epi32(offset_max, ks);
    max_antidiag_v     = _mm256_max_epi32(max_antidiag_v, offset_max);
    ks                 = _mm256_add_epi32(ks, eights);

    //(2*(offset)-(k))
    if(mask != 0) 
    {
      const wf_offset_t max_antidiagonal_buffer[8]; 
      _mm256_storeu_si256((__m256i*)&max_antidiagonal_buffer[0], max_antidiag_v);
      
      for (int i = 0; i < 8; i++)
      {
        const wf_offset_t antidiag = max_antidiagonal_buffer[i];
        if (max_antidiag < antidiag) max_antidiag = antidiag;
      }

      while (mask != 0) 
      {
        int tz = __builtin_ctz(mask);
        int curr_k = k + (tz/4);
        wf_offset_t offset = offsets[curr_k];
        // Extend offset
        if (offset >= 0) {
          offset          = wavefront_extend_matches_packed_kernel(wf_aligner,curr_k,offset);
          offsets[curr_k] = offset;
          const wf_offset_t antidiag = WAVEFRONT_ANTIDIAGONAL(curr_k, offset);
          if (max_antidiag < antidiag) max_antidiag = antidiag;
        } else {
          offsets[curr_k] = WAVEFRONT_OFFSET_NULL;
        }
        mask &= (0xfffffff0 << tz);
      }
      max_antidiag_v = _mm256_set1_epi32(max_antidiag);
    }
  }

  const wf_offset_t max_antidiagonal_buffer[8]; 
  _mm256_storeu_si256((__m256i*)&max_antidiagonal_buffer[0], max_antidiag_v);
  for (int i = 0; i < 8; i++)
  {
    const wf_offset_t antidiag = max_antidiagonal_buffer[i];
    if (max_antidiag < antidiag) max_antidiag = antidiag;
  }
  return max_antidiag; 
}


FORCE_NO_INLINE bool wavefront_extend_matches_packed_endsfree_avx2(
    wavefront_aligner_t* const wf_aligner,
    wavefront_t* const mwavefront,
    const int score,
    const int lo,
    const int hi) {
  // Parameters

  const int elems_per_register = 8;
  const char* pattern = wf_aligner->sequences.pattern;
  const char* text    = wf_aligner->sequences.text;
  
  wf_offset_t* const offsets = mwavefront->offsets;
  
  int k_min = lo;
  int k_max = hi;
 
  int num_of_diagonals   = k_max - k_min + 1;
  int loop_peeling_iters = num_of_diagonals % elems_per_register;
  int k;
  
  const __m256i vector_null = _mm256_set1_epi32(-1);
  const __m256i eights      = _mm256_set1_epi32(8);
  const __m256i vecShuffle  = _mm256_set_epi8(28,29,30,31,24,25,26,27,
                                              20,21,22,23,16,17,18,19,
                                              12,13,14,15, 8, 9,10,11,
                                              4 , 5, 6, 7, 0, 1, 2 ,3);
  
  for (k=k_min;k<k_min+loop_peeling_iters;k++) {
    wf_offset_t offset = offsets[k];
    if (offset < 0) continue;
    offset = wavefront_extend_matches_packed_kernel(wf_aligner,k,offset);  
    offsets[k] = offset; 
    // Check ends-free reaching boundaries
    if (wavefront_termination_endsfree(wf_aligner,mwavefront,score,k,offset)) {
      return true; // Quit (we are done)
    }
  }

  if (num_of_diagonals < elems_per_register) return false;

  // Alignment not finished
  k_min += loop_peeling_iters;
  __m256i ks = _mm256_set_epi32 (
      k_min+7,k_min+6,k_min+5,k_min+4,
      k_min+3,k_min+2,k_min+1,k_min);


  for (k=k_min; k<=k_max; k+=elems_per_register) {
    __m256i offsets_vector = _mm256_lddqu_si256 ((__m256i*)&offsets[k]);
    __m256i h_vector       = offsets_vector;
    __m256i v_vector       = _mm256_sub_epi32(offsets_vector,ks);
    
    // NULL offsets will read at index 0 (avoid segfaults)
    __m256i null_mask = _mm256_cmpgt_epi32(offsets_vector, vector_null);
    v_vector = _mm256_and_si256(null_mask, v_vector);
    h_vector = _mm256_and_si256(null_mask, h_vector);
    
    __m256i pattern_vector = _mm256_i32gather_epi32((int const*)&pattern[0],v_vector,1);
    __m256i text_vector    = _mm256_i32gather_epi32((int const*)&text[0],h_vector,1);
    __m256i vector_mask    = _mm256_cmpeq_epi32(text_vector, pattern_vector);
    int mask               = _mm256_movemask_epi8(vector_mask);

    __m256i xor_result_vector = _mm256_xor_si256(pattern_vector,text_vector);
    xor_result_vector         = _mm256_shuffle_epi8(xor_result_vector, vecShuffle);
    __m256i clz_vector        = avx2_lzcnt_epi32(xor_result_vector);

    __m256i equal_chars = _mm256_srli_epi32(clz_vector, 3);
    offsets_vector      = _mm256_add_epi32 (offsets_vector,equal_chars);
    ks                  = _mm256_add_epi32(ks, eights);
    _mm256_storeu_si256((__m256i*)&offsets[k],offsets_vector);
    
    if(mask == 0) continue; 

    while (mask != 0) 
    {
      int tz = __builtin_ctz(mask);
      int curr_k = k + (tz/4);
      const wf_offset_t offset = offsets[curr_k];
      // Extend offset
      if (offset >= 0) {
        offsets[curr_k] = wavefront_extend_matches_packed_kernel(wf_aligner,curr_k,offset);
        if (wavefront_termination_endsfree(wf_aligner,mwavefront,score,curr_k,offsets[curr_k])) {
          return true; // Quit (we are done)
        }
      } else {
        offsets[curr_k] = WAVEFRONT_OFFSET_NULL;
      }
      mask &= (0xfffffff0 << tz);
    }
  }
  for (k=k_min; k <= k_max; k++) {
    const wf_offset_t offset = offsets[k];
    if (offset < 0) continue;
    // Check ends-free reaching boundaries
    if (wavefront_termination_endsfree(wf_aligner,mwavefront,score,k,offset)) {
      return true; // Quit (we are done)
    }
  }
  return false;
}

void print_m512i(__m512i vec) {
    // Use an array to store the vector's content for printing
    int32_t values[16]; // __m512i contains 16 32-bit integers
    _mm512_storeu_si512((__m512i*)values, vec); // Store the vector into the array

    printf("Vector contents: ");
    for (int i = 0; i < 16; i++) {
        printf("%d ", values[i]);
    }
    printf("\n");
}

void print_mask(__mmask16 mask) {
    printf("Mask: ");
    for (int i = 15; i >= 0; i--) { // Print from MSB to LSB
        printf("%d", (mask >> i) & 1);
    }
    printf("\n");
}

void print_m512i_bytes(__m512i vec) {
    // __m512i contains 64 bytes (512 bits)
    uint8_t values[64];  
    _mm512_storeu_si512((__m512i*)values, vec); // Store the vector into the array

    printf("Vector contents (bytes): ");
    for (int i = 0; i < 64; i++) {
        printf("%02X ", values[i]);  // Print as two-digit hex values
    }
    printf("\n");
}

void print_m512i_to_file(__m512i vec, FILE* file) {
    // __m512i contains 64 bytes (512 bits)
    int32_t values[16];
    _mm512_storeu_si512((__m512i*)values, vec); // Store the vector into the array

    fprintf(file, ": ");
    for (int i = 0; i < 64; i++) {
        fprintf(file, "%d ", values[i]);  // Print as two-digit hex values
    }
    fprintf(file, "\n");
}

// FORCE_NO_INLINE void TEST_SAMPLE(
//     wavefront_aligner_t* const wf_aligner,
//     wavefront_t* const mwavefront,
//     const int lo,
//     const int hi,
//     const int k,
//     __m512i* ks) {
//   // Parameters
//   wf_offset_t* const offsets = mwavefront->offsets;
//   int k_min = lo;
//   int k_max = hi;
//   const char* pattern = wf_aligner->sequences.pattern;
//   const char* text = wf_aligner->sequences.text;
//   const __m512i zero_vector = _mm512_setzero_si512();
//   const __m512i vector_null = _mm512_set1_epi32(-1);
//   const __m512i offsets_null= _mm512_set1_epi32(WAVEFRONT_OFFSET_NULL);
//   const __m512i sixteens    = _mm512_set1_epi32(16);
//   const __m512i vecShuffle  = _mm512_set_epi8(60,61,62,63,56,57,58,59,
//                                              52,53,54,55,48,49,50,51,
//                                              44,45,46,47,40,41,42,43,
//                                              36,37,38,39,32,33,34,35,
//                                              28,29,30,31,24,25,26,27,
//                                              20,21,22,23,16,17,18,19,
//                                              12,13,14,15,8,9,10,11,
//                                              4,5,6,7,0,1,2,3);
//   const int elems_per_register = 16;

//     __m512i offsets_vector = _mm512_loadu_si512 ((__m512i*)&offsets[k]);
//     __m512i h_vector       = offsets_vector;
//     __m512i v_vector       = _mm512_sub_epi32(offsets_vector, *ks);
    
//     // NULL offsets will read at index 0 (avoid segfaults)
//     __mmask16 null_mask    = _mm512_cmpgt_epi32_mask(offsets_vector, vector_null);
//     __m512i pattern_vector = _mm512_mask_i32gather_epi32(zero_vector, null_mask, v_vector, &pattern[0], 1);
//     __m512i text_vector    = _mm512_mask_i32gather_epi32(zero_vector, null_mask, h_vector, &text[0],    1);
//     __mmask16 mask         = _mm512_mask_cmpeq_epi32_mask(null_mask, pattern_vector, text_vector);

//     __m512i xor_result_vector = _mm512_xor_si512(pattern_vector,text_vector);
//     xor_result_vector         = _mm512_shuffle_epi8(xor_result_vector, vecShuffle);
//     __m512i clz_vector        = _mm512_maskz_lzcnt_epi32(null_mask, xor_result_vector);

//     __m512i equal_chars = _mm512_srli_epi32(clz_vector, 3);
//     offsets_vector      = _mm512_maskz_add_epi32(null_mask, offsets_vector, equal_chars);
//     offsets_vector      = _mm512_mask_blend_epi32(null_mask, offsets_null, offsets_vector);
//     *ks                  = _mm512_add_epi32 (*ks, sixteens);

//     _mm512_storeu_si512((__m512*)&offsets[k],offsets_vector);
    
//     if(mask == 0) return;

//     int st  = __builtin_ctz(mask);
//     int en =__builtin_clz(mask)-16;

//     for (int i=st; i<16-en; i++){
//       if (((mask >> i) & 1) == 0) continue;
//       const int curr_k = k + i;
//       const wf_offset_t offset = offsets[curr_k];
//       if (offset >= 0) {
//         offsets[curr_k] = wavefront_extend_matches_packed_kernel(wf_aligner,curr_k,offset);
//       } else {
//         offsets[curr_k] = WAVEFRONT_OFFSET_NULL;
//       }
//     }
// }

#if __AVX512CD__ && __AVX512VL__
/*

 * Wavefront-Extend Inner Kernel (SIMD AVX512)
 */
FORCE_NO_INLINE void wavefront_extend_matches_packed_end2end_avx512(
    wavefront_aligner_t* const wf_aligner,
    wavefront_t* const mwavefront,
    const int lo,
    const int hi) {
// static bool executed = false;
    
  // Parameters
  wf_offset_t* const offsets = mwavefront->offsets;
  int k_min = lo;
  int k_max = hi;
  const char* pattern = wf_aligner->sequences.pattern;
  const char* text = wf_aligner->sequences.text;

  //   const __m512i zero_vector = _mm512_setzero_si512();
  // const __m512i vector_null = _mm512_set1_epi32(-1);
  // const __m512i sixteens    = _mm512_set1_epi32(16);
  // const __m512i vecShuffle  = _mm512_set_epi8(60,61,62,63,56,57,58,59,
  //                                            52,53,54,55,48,49,50,51,
  //                                            44,45,46,47,40,41,42,43,
  //                                            36,37,38,39,32,33,34,35,
  //                                            28,29,30,31,24,25,26,27,
  //                                            20,21,22,23,16,17,18,19,
  //                                            12,13,14,15,8,9,10,11,
  //                                            4,5,6,7,0,1,2,3);
    

  const int elems_per_register = 16;
  int num_of_diagonals = k_max - k_min + 1;
  int loop_peeling_iters = num_of_diagonals % elems_per_register;
  int k;
  for (k=k_min;k<k_min+loop_peeling_iters;k++) {
    const wf_offset_t offset = offsets[k];
    if (offset < 0) continue;
    // Extend offset
    offsets[k] = wavefront_extend_matches_packed_kernel(wf_aligner,k,offset);
  }
  if (num_of_diagonals < elems_per_register) return;

  k_min += loop_peeling_iters;
  __m512i ks = _mm512_set_epi32 (
      k_min+15,k_min+14,k_min+13,k_min+12,k_min+11,k_min+10,k_min+9,k_min+8,
      k_min+7,k_min+6,k_min+5,k_min+4,k_min+3,k_min+2,k_min+1,k_min);
    __m512i ks_copy = ks;

    // Maybe adapt this entire loop so that vecshuffle is only initialized once?
  for (k=k_min; k<=k_max; k+=elems_per_register) {
      // Maybe can get away with removing this line if aligned is done?
      __m512i offsets_vector = _mm512_loadu_si512 ((__m512i*)&offsets[k]);
      
      // __m512i test_offsets_vector = offsets_vector;
      // __m512i test_ks = ks;
      __mmask16 mask;
      int debug = 0;
      
    avx_wavefront_extension_iteration(&offsets_vector, &ks_copy, &mask, pattern, text, &debug);
    
    //     TEST_SAMPLE(wf_aligner,mwavefront,k_min,k_max,k,&ks);
    //   alignas(64) int32_t offsets_array[16];
    // _mm512_store_si512((__m512i*)offsets_array, offsets_vector);
    
    // for (int i = 0; i < 16; ++i) {
    //   int original = mwavefront->offsets[k + i];
    //   int computed = offsets_array[i];
    //   if (original != computed) {
    //     printf("Mismatch at k+%d: original=%d, computed=%d, iters=%d\n", i, original, computed, debug);
    //   }
    // }

    _mm512_storeu_si512((__m512*)&offsets[k],offsets_vector);


    //   __m512i h_vector       = offsets_vector;
    // __m512i v_vector       = _mm512_sub_epi32(offsets_vector, ks);
    
    // // NULL offsets will read at index 0 (avoid segfaults)
    // __mmask16 null_mask    = _mm512_cmpgt_epi32_mask(offsets_vector, vector_null);
    // __m512i pattern_vector = _mm512_mask_i32gather_epi32(zero_vector, null_mask, v_vector, &pattern[0], 1);
    // __m512i text_vector    = _mm512_mask_i32gather_epi32(zero_vector, null_mask, h_vector, &text[0],    1);
    // __mmask16 mask         = _mm512_mask_cmpeq_epi32_mask(null_mask, pattern_vector, text_vector);

    // __m512i xor_result_vector = _mm512_xor_si512(pattern_vector,text_vector);
    // xor_result_vector         = _mm512_shuffle_epi8(xor_result_vector, vecShuffle);
    // __m512i clz_vector        = _mm512_maskz_lzcnt_epi32(null_mask, xor_result_vector);

    // __m512i equal_chars = _mm512_srli_epi32(clz_vector, 3);
    // offsets_vector      = _mm512_maskz_add_epi32(null_mask, offsets_vector, equal_chars);
    // ks                  = _mm512_add_epi32 (ks, sixteens);

    //   __mmask16 diff_mask = _mm512_cmpneq_epi32_mask(test_offsets_vector, offsets_vector);
      // if (diff_mask && !executed) {
      //     executed = true;
      //     printf("1: ");
      //     print_m512i(h_vector);
      //     printf("2: ");
      //     print_m512i(debug);
      // }

    // if(mask == 0) continue;

    //   // for (mask != 0) {
          
    //   // }

    //   // en = st - 16
    // // int st  = __builtin_ctz(mask);
    // // int en =__builtin_clz(mask)-16;
    // //   if (en < 0) printf("\n");

    // //   // This might be vectorizable? Unravel the loop?
    // // for (int i=st; i<16-en; i++){
    // //   if (((mask >> i) & 1) == 0) continue; // Checks if bit at position i is 1 or 0, run the rest if 1, do nothing if 0, can just use mask itself
    // //   const int curr_k = k + i; // curr_k can be a mask that movs from k to k+16 based on prev mask instead?
    // //   const wf_offset_t offset = offsets[curr_k]; // masked mov from offsets
    // //   if (offset >= 0) {
    // //     offsets[curr_k] = wavefront_extend_matches_packed_kernel(wf_aligner,curr_k,offset); // function call? Might not be vectorizable
    // //   } else {
    // //     offsets[curr_k] = WAVEFRONT_OFFSET_NULL; // masked mov to offsets, just set null if negative
    // //   }
    // // }

    //   // load st + 1...16 into vec i
    //   // perform masked add on k and i using "mask", output is curr_k
    //   // masked gather offsets with curr_k using "mask", output is offset
    //   // make mask based on if offset is negative
    //   // perform masked mov using mask from prev line
    //   // sequentially call func for the rest of offset
    //   __m512i idx_vec = _mm512_set_epi32(15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0);
    // __m512i k_vec   = _mm512_set1_epi32(k);
    // __m512i curr_k  = _mm512_add_epi32(idx_vec, k_vec);
    //   __m512i offsets_vec = _mm512_mask_i32gather_epi32(_mm512_setzero_si512(), mask, curr_k, offsets, 4);
    //   __mmask16 neg_mask = _mm512_cmp_epi32_mask(offsets_vec, _mm512_setzero_si512(), _MM_CMPINT_LT);
    //     __mmask16 pos_mask = mask & ~neg_mask;
    //     __m512i null_val = _mm512_set1_epi32(WAVEFRONT_OFFSET_NULL);
    //     _mm512_mask_i32scatter_epi32(offsets, mask & neg_mask, curr_k, null_val, 4);
    //   int compressed_curr_k[16];
    //     int compressed_offset[16];
        
    //     // Store only the active `curr_k_arr` and `offset_arr` lanes
    //     _mm512_mask_compressstoreu_epi32(compressed_curr_k, pos_mask, curr_k);
    //     _mm512_mask_compressstoreu_epi32(compressed_offset, pos_mask, offsets_vec);
        
    //     // Count active lanes
    //     int num_active = _mm_popcnt_u32(pos_mask);
        
    //     // Now loop only over the active elements
    //     for (int i = 0; i < num_active; ++i) {
    //       const int k_i = compressed_curr_k[i];
    //       const int offset_i = compressed_offset[i];
    //       offsets[k_i] = wavefront_extend_matches_packed_kernel(wf_aligner, k_i, offset_i);
    //     }
    //   debug = mask;

    //   offsets_vector = _mm512_loadu_si512 ((__m512i*)&offsets[k]);
    //   FILE *file = fopen("dump.txt", "a");  // "w" = write mode (overwrite)
    
    // if (file == NULL) {
    //     perror("fopen");
    // }
    //   print_m512i_to_file(offsets_vector, file);
    // fclose(file);
     // print_mask(debug);
  }
}


FORCE_NO_INLINE wf_offset_t wavefront_extend_matches_packed_end2end_max_avx512(
    wavefront_aligner_t* const wf_aligner,
    wavefront_t* const mwavefront,
    const int lo,
    const int hi) {
  // Parameters
  const int elems_per_register = 16;
  const char* pattern = wf_aligner->sequences.pattern;
  const char* text    = wf_aligner->sequences.text;
  
  wf_offset_t* const offsets = mwavefront->offsets;
  wf_offset_t max_antidiag   = 0;
  
  int k_min = lo;
  int k_max = hi;
 
  int num_of_diagonals   = k_max - k_min + 1;
  int loop_peeling_iters = num_of_diagonals % elems_per_register;
  int k;

  const __m512i zero_vector = _mm512_setzero_si512();
  const
    __m512i vector_null = _mm512_set1_epi32(-1);
  const __m512i sixteens    = _mm512_set1_epi32(16);
  const __m512i vecShuffle  = _mm512_set_epi8(60,61,62,63,56,57,58,59,
                                             52,53,54,55,48,49,50,51,
                                             44,45,46,47,40,41,42,43,
                                             36,37,38,39,32,33,34,35,
                                             28,29,30,31,24,25,26,27,
                                             20,21,22,23,16,17,18,19,
                                             12,13,14,15,8,9,10,11,
                                             4,5,6,7,0,1,2,3);

  for (k=k_min;k<k_min+loop_peeling_iters;k++) 
  {
    wf_offset_t offset = offsets[k];
    if (offset < 0) continue;
    offset     =  wavefront_extend_matches_packed_kernel(wf_aligner,k,offset);  
    offsets[k] = offset;
    const wf_offset_t antidiag = WAVEFRONT_ANTIDIAGONAL(k,offset);
    if (max_antidiag < antidiag) max_antidiag = antidiag;
  }

  if (num_of_diagonals < elems_per_register) return max_antidiag;
  
  k_min += loop_peeling_iters;
  __m512i ks = _mm512_set_epi32 (
      k_min+15,k_min+14,k_min+13,k_min+12,k_min+11,k_min+10,k_min+9,k_min+8,
      k_min+7,k_min+6,k_min+5,k_min+4,k_min+3,k_min+2,k_min+1,k_min);
  __m512i max_antidiag_v = _mm512_set1_epi32(max_antidiag);

  for (k=k_min; k<=k_max; k+=elems_per_register) {
    __m512i offsets_vector = _mm512_loadu_si512 ((__m512i*)&offsets[k]);
    __m512i h_vector       = offsets_vector;
    __m512i v_vector       = _mm512_sub_epi32(offsets_vector, ks);
    
    __mmask16 null_mask    = _mm512_cmpgt_epi32_mask(offsets_vector, vector_null);
    __m512i pattern_vector = _mm512_mask_i32gather_epi32(zero_vector, null_mask, v_vector, &pattern[0], 1);
    __m512i text_vector    = _mm512_mask_i32gather_epi32(zero_vector, null_mask, h_vector, &text[0],    1);
    __mmask16 mask         = _mm512_mask_cmpeq_epi32_mask(null_mask, pattern_vector, text_vector);

    __m512i xor_result_vector = _mm512_xor_si512(pattern_vector,text_vector);
    xor_result_vector         = _mm512_shuffle_epi8(xor_result_vector, vecShuffle);
    __m512i clz_vector        = _mm512_maskz_lzcnt_epi32(null_mask, xor_result_vector);

      
    __m512i equal_chars = _mm512_srli_epi32(clz_vector, 3);
    offsets_vector      = _mm512_maskz_add_epi32(null_mask, offsets_vector, equal_chars);

    _mm512_storeu_si512((__m512*)&offsets[k], offsets_vector);

    __m512i offset_max = _mm512_maskz_slli_epi32(null_mask, offsets_vector, 1); 
    offset_max         = _mm512_maskz_sub_epi32(null_mask, offset_max, ks);
    ks                 = _mm512_add_epi32(ks, sixteens);
    max_antidiag_v     = _mm512_mask_max_epi32(max_antidiag_v, null_mask, max_antidiag_v, offset_max);

    if(mask == 0) continue;

    int st  = __builtin_ctz(mask);
    int en =__builtin_clz(mask)-16;
    max_antidiag = _mm512_reduce_max_epi32(max_antidiag_v); 

    for (int i=st; i<16-en; i++)
    {
      if (((mask >> i) & 1) == 0) continue;
      const int curr_k   = k + i;
      wf_offset_t offset = offsets[curr_k];
      if (offset >= 0) 
      {
        offset          = wavefront_extend_matches_packed_kernel(wf_aligner,curr_k,offset);
        offsets[curr_k] = offset;
        const wf_offset_t antidiag = WAVEFRONT_ANTIDIAGONAL(curr_k, offset);
        if (max_antidiag < antidiag) max_antidiag = antidiag;
      } 
      else 
      {
        offsets[curr_k] = WAVEFRONT_OFFSET_NULL;
      }
    }
    max_antidiag_v = _mm512_set1_epi32(max_antidiag);
  }

  return _mm512_reduce_max_epi32(max_antidiag_v); 
}


FORCE_NO_INLINE bool wavefront_extend_matches_packed_endsfree_avx512(
    wavefront_aligner_t* const wf_aligner,

    wavefront_t* const mwavefront,
    const int score,
    const int lo,
    const int hi) {

    // Parameters
  wf_offset_t* const offsets = mwavefront->offsets;
  int k_min = lo;
  int k_max = hi;
  const char* pattern = wf_aligner->sequences.pattern;
  const char* text = wf_aligner->sequences.text;
  // const __m512i zero_vector = _mm512_setzero_si512();
  // const __m512i vector_null = _mm512_set1_epi32(-1);
  // const __m512i sixteens    = _mm512_set1_epi32(16);
  // const __m512i vecShuffle  = _mm512_set_epi8(60,61,62,63,56,57,58,59,
  //                                            52,53,54,55,48,49,50,51,
  //                                            44,45,46,47,40,41,42,43,
  //                                            36,37,38,39,32,33,34,35,
  //                                            28,29,30,31,24,25,26,27,
  //                                            20,21,22,23,16,17,18,19,
  //                                            12,13,14,15,8,9,10,11,
  //                                            4,5,6,7,0,1,2,3);
  const int elems_per_register = 16;
    
  int num_of_diagonals = k_max - k_min + 1;
  int loop_peeling_iters = num_of_diagonals % elems_per_register;
  int k;

  for (k=k_min;k<k_min+loop_peeling_iters;k++) {
    wf_offset_t offset = offsets[k];
    if (offset < 0) continue;
    offset = wavefront_extend_matches_packed_kernel(wf_aligner,k,offset);  
    offsets[k] = offset; 
    // Check ends-free reaching boundaries
    if (wavefront_termination_endsfree(wf_aligner,mwavefront,score,k,offset)) {
      return true; // Quit (we are done)
    }
  }

  if (num_of_diagonals < elems_per_register) return false;

  k_min += loop_peeling_iters;
  __m512i ks = _mm512_set_epi32 (
      k_min+15,k_min+14,k_min+13,k_min+12,k_min+11,k_min+10,k_min+9,k_min+8,
      k_min+7,k_min+6,k_min+5,k_min+4,k_min+3,k_min+2,k_min+1,k_min);
    
  for (k=k_min; k<=k_max; k+=elems_per_register) {
    __m512i offsets_vector = _mm512_loadu_si512 ((__m512i*)&offsets[k]);
    __mmask16 mask;
      __m512i debug;
      
    avx_wavefront_extension_iteration(&offsets_vector, &ks, &mask, pattern, text, &debug);
    
    _mm512_storeu_si512((__m512*)&offsets[k],offsets_vector);
    
    if(mask == 0) continue;
    int st  = __builtin_ctz(mask);
    int en =__builtin_clz(mask)-16;
    for (int i=st; i<16-en; i++){
      if (((mask >> i) & 1) == 0) continue;
      const int curr_k = k + i;
      const wf_offset_t offset = offsets[curr_k];
      if (offset >= 0) {
        offsets[curr_k] = wavefront_extend_matches_packed_kernel(wf_aligner,curr_k,offset);
        if (wavefront_termination_endsfree(wf_aligner,mwavefront,score,curr_k,offsets[curr_k])) {
          return true; // Quit (we are done)
        }
      } 
      else {
        offsets[curr_k] = WAVEFRONT_OFFSET_NULL;
      }
    }
  }

  for (k=k_min; k <= k_max; k++) {
    const wf_offset_t offset = offsets[k];
    if (offset < 0) continue;
    // Check ends-free reaching boundaries
    if (wavefront_termination_endsfree(wf_aligner,mwavefront,score,k,offset)) {
      return true; // Quit (we are done)
    }
  }
  return false;  
} 
#endif

#endif // AVX2