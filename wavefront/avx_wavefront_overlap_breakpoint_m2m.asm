section .text
global avx_wavefront_overlap_breakpoint_m2m

avx_wavefront_overlap_breakpoint_m2m:
    vsubpd zmm2, zmm1, zmm0 ; k1_vec = tlen_vec - plen_vec (zmm1 = rsi, zmm0 = rdi)
    vsubpd zmm2, zmm2, zmm0 ; k1_vec = k1_vec - k0_vec (zmm0 = rdi)

    vmovdqu32 zmm3, [rsp+32] ; address of mwf_0->offsets
    vmovdqu32 zmm4, [rsp+40] ; address of mwf_1->offsets

    vpgatherdd zmm0, [zmm3 + k0_vec*4], k0_mask ; moffset0_vec = mwf_0->offsets[k0_vec]
    vpgatherdd zmm1, [zmm4 + zmm2*4], k0_mask ; moffset1_vec = mwf_1->offsets[k1_vec]

    vpaddd zmm5, zmm0, zmm1 ; mh_sum_vec = moffset0_vec + moffset1_vec
    vpcmpd k1, zmm5, [rsp+48], 5 ; mh_check_mask = mh_sum_vec >= text_length (5 = _MM_CMPINT_GE)

    vpaddd zmm6, rcx, r8 ; score_sum_vec = score0_vec + score1_vec
    vpcmpd k2, zmm6, r9, 1 ; score_check_mask = score_sum_vec < breakpoint_score_vec (1 = _MM_CMPINT_LT)

    kandw k3, k1, k2 ; combined_mask = mh_check_mask & score_check_mask
    mov rax, k3 ; return combined_mask

    ret
