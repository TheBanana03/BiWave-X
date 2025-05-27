section .text
global avx512_wavefront_overlap_breakpoint_check


avx512_wavefront_overlap_breakpoint_check:
    ;             rdi         rsi         rdx    rcx      r8               r9         rsp+8
    ; params: &k0_vector, &mask_indices,  tlen, plen, mwf_0->offsets, mwf_1->offsets, &mask1
    vpxord zmm0, zmm0, zmm0
    vmovdqa32 zmm0, [rdi] ; k0_vector

    ; WAVEFRONT_K_INVERSE(k,tlen,plen)  (tlen-plen-k)
    vpbroadcastd zmm8, edx
    sub edx, ecx
    vpbroadcastd zmm2, edx
    vpbroadcastd zmm3, ecx

    vpsubd zmm1, zmm2, zmm0 ; k1_vector values

    kmovd k1, [rsi]
    kmovd k3, k1
    vpxord zmm5, zmm5, zmm5
    vpgatherdd zmm5 {k3}, [r8+zmm0*4]  ; moffsets_0 vector
    vpxord zmm6, zmm6, zmm6
    vpgatherdd zmm6 {k1}, [r9+zmm1*4]  ; moffsets_1 vector

    ; Check (mh_0 + mh_1 >= text_length)
    vpaddd zmm7, zmm5, zmm6  ; mh0 + mh1

    vpcmpltd k2, zmm7, zmm8
    knotd k2, k2
    
    ; pass the mask back
    kmovd [rsi], k2

    ret