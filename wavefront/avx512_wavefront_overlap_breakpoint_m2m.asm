section .text
global avx512_wavefront_overlap_breakpoint_m2m

avx512_wavefront_overlap_breakpoint_m2m:
    ;             rdi         rsi         rdx          rcx       r8    r9      rsp+16           +24          +32
    ; params: &k0_vector, &k1_vector,  &moffset_0, &moffset_1, tlen, plen, mwf_0->offsets, mwf_1->offsets, &mask
    vmovdqu32 zmm0, [rdi] ; k0_vector

    ; WAVEFRONT_K_INVERSE(tlen-plen-k)
    mov eax, r8d  ; tlen
    mov ebx, r9d  ; plen
    mov ecx, eax
    sub ecx, ebx
    vpbroadcastd zmm2, eax
    vpbroadcastd zmm3, ebx

    vsubpd zmm1, zmm2, zmm0 ; k1_vector values
    ; vmovdqu32 [rsi], zmm1    prolly not needed outside, un-comment otherwise


    ; Gather offsets at indices stored in zmm4
    mov rax, [rsp+16]
    mov rbx, [rsp+24]
    vgatherdps zmm5, [rax+zmm0*4]  ; moffsets_0 vector
    vgatherdps zmm6, [rbx+zmm1*4]  ; moffsets_1 vector
    vmovdqu32 [rdx], zmm5
    vmovdqu32 [rcx], zmm6


    ; Check (mh_0 + mh_1 >= text_length)  ;;  condition (score_0 + score_1 < breakpoint->score) is checked outside
    ; #define WAVEFRONT_H(k,offset) => returns (offset)
    ; therefore; mh_0 and mh_1 are just moffset_0 and moffset_1
    vpaddd zmm7, zmm5, zmm6  ; mh0 + mh1
    vpcmpgtd k1, zmm7, zmm2
    ;; pass the mask back


    ret