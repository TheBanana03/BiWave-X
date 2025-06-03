section .data
align 64
vecShuffle:
    db 3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12
    db 19,18,17,16,23,22,21,20,27,26,25,24,31,30,29,28
    db 35,34,33,32,39,38,37,36,43,42,41,40,47,46,45,44
    db 51,50,49,48,55,54,53,52,59,58,57,56,63,62,61,60


section .text
global avx_wavefront_extension_iteration
global load_avx2_sequence
default rel

avx_wavefront_extension_iteration:
    xor rax, rax

    vpxord zmm6, zmm6, zmm6
    vpternlogd zmm5, zmm6, zmm6, 0xFF
    vmovdqa64 zmm10, [rel vecShuffle]

    vmovdqa32 zmm0, [rdi]
    vmovdqa32 zmm1, zmm0
    vmovdqa32 zmm2, [rsi]
    vpsubd zmm4, zmm0, zmm2

    vpcmpgtd k1, zmm0, zmm5
    kmovd k3, k1
    vmovdqa32 [r9], zmm5

    vpxord zmm7, zmm7, zmm7
    vpgatherdd zmm7{k3}, [rcx+zmm4*1]
    kmovd k3, k1

    vpxord zmm8, zmm8, zmm8
    vpgatherdd zmm8{k3}, [r8+zmm1*1]

    vpcmpeqd k2{k1}, zmm7, zmm8

    kmovd [rdx], k2

    vpxord zmm9, zmm7, zmm8
    vpshufb zmm9, zmm9, zmm10

    vplzcntd zmm11{k1}{z}, zmm9
    
    vpsrld zmm12, zmm11, 3

    vpaddd zmm12, zmm0, zmm12
    vpxord zmm0, zmm0, zmm0
    vmovdqa32 zmm0{k1}{z}, zmm12

    ; sub rsp, 64
    ; mov rax, rsp
    ; add rsp, 64
    ; lea rax, [rsp - 64]

    mov dword [rsp - 64], 0x10
    vpbroadcastd zmm3, [rsp - 64]
    vpaddd zmm4, zmm2, zmm3
    vmovdqa32 [rsi], zmm4

    vmovdqa32 [rdi], zmm0

    ret