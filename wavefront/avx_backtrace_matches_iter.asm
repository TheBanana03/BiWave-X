section .data
lookup db 0x4d
section .text
global avx_backtrace_matches_iter
default rel

;this function assumes that the loop peeling occurs properly
;params will be (operations, num matches)
;parameter registers: RDI, RSI, RDX, RCX, R8, R9
;can do 64 characters at once
avx_backtrace_matches_iter:
    ;version1 | no shr in assembly for now
    ;sub rdi, 64; equivalent to mem_address - 64
    ;add rdi, 8
    vmovdqu64 zmm0, [RDI];offset -> -64 and +8
    vpbroadcastb zmm1, [lookup];brsoadcast 'M' ASCII to the registers
    vmovdqu64 zmm0, zmm1;overwrite the zmm0 with 'M' characters
    vmovdqu64 [RDI], zmm0
    ret