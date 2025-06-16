section .data
align 64
vecShuffle:
    db 3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12
    db 19,18,17,16,23,22,21,20,27,26,25,24,31,30,29,28
    db 35,34,33,32,39,38,37,36,43,42,41,40,47,46,45,44
    db 51,50,49,48,55,54,53,52,59,58,57,56,63,62,61,60


section .text
global avx_wavefront_extension_iteration
default rel

avx_wavefront_extension_iteration:
    xor rax, rax
    vzeroall

    mov eax, 0x10
    vpbroadcastd zmm3, eax

    mov dword [rsp - 64], 0xC0000000
    vpbroadcastd zmm13, [rsp - 64]

    mov dword [rsp - 64], 0x4
    vpbroadcastd zmm14, [rsp - 64]

    ; vpxord zmm6, zmm6, zmm6

    vpternlogd zmm15, zmm6, zmm6, 0xFF
    vmovdqa64 zmm10, [rel vecShuffle]

    vmovdqa32 zmm0, [rdi]
    vmovdqa32 zmm1, zmm0
    vmovdqa32 zmm2, [rsi]
    vpsubd zmm4, zmm0, zmm2

    vpcmpgtd k1, zmm0, zmm15
    kmovd k3, k1

    vpxord zmm7, zmm7, zmm7

    vpgatherdd zmm7{k3}, [rcx+zmm4*1]
    kmovd k3, k1

    vpxord zmm8, zmm8, zmm8
    vpgatherdd zmm8{k3}, [r8+zmm1*1]

    vpcmpeqd k2{k1}, zmm7, zmm8

    vpxord zmm9, zmm7, zmm8
    vpshufb zmm9, zmm9, zmm10

    vplzcntd zmm11{k1}{z}, zmm9

    vpsrld zmm12, zmm11, 3

    vpaddd zmm12, zmm0, zmm12

    vpxord zmm0, zmm0, zmm0

    vmovdqa32 zmm0{k1}{z}, zmm12
    vmovdqa32 [rdi], zmm0

    vpaddd zmm5, zmm2, zmm3
    vmovdqa32 [rsi], zmm5

    kortestw k2, k2
    jz      .skip_iter

    ; vmovdqa32 [rdi], zmm0
    ; kmovd [r9], k1

    vpcmpd k1{k2}, zmm0, zmm6, 0x1
    knotw k3, k1
    kandw k4, k3, k2
    kandw k3, k1, k2

    vmovdqa32 zmm12{k4}, zmm0

    mov rax, 0x4

    lea r10, [rax+rcx]
    lea r11, [rax+r8]

    kmovd k5, k4
    vpgatherdd zmm7{k5}, [r10+zmm4*1]

    kmovd k5, k4
    vpgatherdd zmm8{k5}, [r11+zmm1*1]

    vpcmpeqd k5{k4}, zmm7, zmm8
    kortestw k5, k5
    jz .end_extend_loop

    .extend_loop:
        add rax, 0x4
        lea r10, [rax+rcx]
        lea r11, [rax+r8]
    
        ; vpaddd zmm12, zmm12, zmm14
        vpxord zmm9, zmm7, zmm8
        vpshufb zmm9, zmm9, zmm10
    
        vplzcntd zmm11{k4}{z}, zmm9
    
        vpsrld zmm12, zmm11, 3
        vpaddd zmm12, zmm0, zmm12
        vmovdqa32 zmm0{k4}, zmm12
    
        kmovd k5, k4
        vpgatherdd zmm7{k5}, [r10+zmm4*1]
    
        kmovd k5, k4
        vpgatherdd zmm8{k5}, [r11+zmm1*1]
    
        vpcmpeqd k5{k4}, zmm7, zmm8
        kortestw k5, k5
        jnz .extend_loop

    .end_extend_loop:
        vpxord zmm9, zmm7, zmm8
        vpshufb zmm9, zmm9, zmm10
    
        vplzcntd zmm11{k4}{z}, zmm9
    
        vpsrld zmm12, zmm11, 3
        vpaddd zmm12, zmm0, zmm12
        vmovdqa32 zmm0{k4}, zmm12

        vpblendmd zmm0{k3}, zmm0, zmm13
        vmovdqa32 [rdi], zmm0

    .skip_iter:

    ret