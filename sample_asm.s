/* =========================================================
 * sample_asm.s  —  Standalone x86-64 assembly (GAS / AT&T syntax)
 *
 * Assemble:  gcc -c sample_asm.s -o sample_asm.o
 * ========================================================= */

    .intel_syntax noprefix      /* switch to Intel syntax */
    .text

/* ---------------------------------------------------------
 * 1. add_integers(int a, int b) -> int
 *    Arguments: rdi=a, rsi=b   (System V AMD64 ABI)
 *    Return:    rax
 * --------------------------------------------------------- */
    .globl  add_integers
    .type   add_integers, @function
add_integers:
    mov     eax, edi        /* eax = a */
    add     eax, esi        /* eax = a + b */
    ret
    .size   add_integers, . - add_integers


/* ---------------------------------------------------------
 * 2. sub_integers(int a, int b) -> int
 * --------------------------------------------------------- */
    .globl  sub_integers
    .type   sub_integers, @function
sub_integers:
    mov     eax, edi
    sub     eax, esi
    ret
    .size   sub_integers, . - sub_integers


/* ---------------------------------------------------------
 * 3. mul_integers(int a, int b) -> int
 * --------------------------------------------------------- */
    .globl  mul_integers
    .type   mul_integers, @function
mul_integers:
    mov     eax, edi
    imul    eax, esi        /* signed multiply */
    ret
    .size   mul_integers, . - mul_integers


/* ---------------------------------------------------------
 * 4. div_integers(int a, int b) -> int  (quotient)
 *    Uses idiv: rdx:rax / rsi
 * --------------------------------------------------------- */
    .globl  div_integers
    .type   div_integers, @function
div_integers:
    mov     eax, edi        /* dividend low  */
    cdq                     /* sign-extend eax → edx:eax */
    idiv    esi             /* eax = quotient, edx = remainder */
    ret
    .size   div_integers, . - div_integers


/* ---------------------------------------------------------
 * 5. mod_integers(int a, int b) -> int  (remainder)
 * --------------------------------------------------------- */
    .globl  mod_integers
    .type   mod_integers, @function
mod_integers:
    mov     eax, edi
    cdq
    idiv    esi
    mov     eax, edx        /* remainder in edx */
    ret
    .size   mod_integers, . - mod_integers


/* ---------------------------------------------------------
 * 6. bitwise_and(uint32_t a, uint32_t b) -> uint32_t
 * --------------------------------------------------------- */
    .globl  bitwise_and
    .type   bitwise_and, @function
bitwise_and:
    mov     eax, edi
    and     eax, esi
    ret
    .size   bitwise_and, . - bitwise_and


/* ---------------------------------------------------------
 * 7. bitwise_or(uint32_t a, uint32_t b) -> uint32_t
 * --------------------------------------------------------- */
    .globl  bitwise_or
    .type   bitwise_or, @function
bitwise_or:
    mov     eax, edi
    or      eax, esi
    ret
    .size   bitwise_or, . - bitwise_or


/* ---------------------------------------------------------
 * 8. bitwise_xor(uint32_t a, uint32_t b) -> uint32_t
 * --------------------------------------------------------- */
    .globl  bitwise_xor
    .type   bitwise_xor, @function
bitwise_xor:
    mov     eax, edi
    xor     eax, esi
    ret
    .size   bitwise_xor, . - bitwise_xor


/* ---------------------------------------------------------
 * 9. bitwise_not(uint32_t a) -> uint32_t
 * --------------------------------------------------------- */
    .globl  bitwise_not
    .type   bitwise_not, @function
bitwise_not:
    mov     eax, edi
    not     eax
    ret
    .size   bitwise_not, . - bitwise_not


/* ---------------------------------------------------------
 * 10. shift_left(uint32_t val, int n) -> uint32_t
 * --------------------------------------------------------- */
    .globl  shift_left
    .type   shift_left, @function
shift_left:
    mov     eax, edi
    mov     ecx, esi        /* shift count must be in cl */
    shl     eax, cl
    ret
    .size   shift_left, . - shift_left


/* ---------------------------------------------------------
 * 11. shift_right(uint32_t val, int n) -> uint32_t  (logical)
 * --------------------------------------------------------- */
    .globl  shift_right
    .type   shift_right, @function
shift_right:
    mov     eax, edi
    mov     ecx, esi
    shr     eax, cl         /* logical (unsigned) shift right */
    ret
    .size   shift_right, . - shift_right


/* ---------------------------------------------------------
 * 12. arith_shift_right(int32_t val, int n) -> int32_t
 * --------------------------------------------------------- */
    .globl  arith_shift_right
    .type   arith_shift_right, @function
arith_shift_right:
    mov     eax, edi
    mov     ecx, esi
    sar     eax, cl         /* arithmetic (signed) shift right */
    ret
    .size   arith_shift_right, . - arith_shift_right


/* ---------------------------------------------------------
 * 13. rotate_left(uint32_t val, int n) -> uint32_t
 * --------------------------------------------------------- */
    .globl  rotate_left
    .type   rotate_left, @function
rotate_left:
    mov     eax, edi
    mov     ecx, esi
    rol     eax, cl
    ret
    .size   rotate_left, . - rotate_left


/* ---------------------------------------------------------
 * 14. rotate_right(uint32_t val, int n) -> uint32_t
 * --------------------------------------------------------- */
    .globl  rotate_right
    .type   rotate_right, @function
rotate_right:
    mov     eax, edi
    mov     ecx, esi
    ror     eax, cl
    ret
    .size   rotate_right, . - rotate_right


/* ---------------------------------------------------------
 * 15. compare_and_return(int a, int b) -> int
 *     Returns:  1 if a > b,  -1 if a < b,  0 if equal
 * --------------------------------------------------------- */
    .globl  compare_and_return
    .type   compare_and_return, @function
compare_and_return:
    cmp     edi, esi
    jg      .Lgreater
    jl      .Lless
    xor     eax, eax        /* equal → 0 */
    ret
.Lgreater:
    mov     eax, 1
    ret
.Lless:
    mov     eax, -1
    ret
    .size   compare_and_return, . - compare_and_return


/* ---------------------------------------------------------
 * 16. asm_abs(int x) -> int  — absolute value without branch
 * --------------------------------------------------------- */
    .globl  asm_abs
    .type   asm_abs, @function
asm_abs:
    mov     eax, edi
    cdq                     /* edx = sign extension of eax */
    xor     eax, edx        /* eax ^= edx */
    sub     eax, edx        /* eax -= edx  → |x| */
    ret
    .size   asm_abs, . - asm_abs


/* ---------------------------------------------------------
 * 17. asm_max(int a, int b) -> int  — branchless max
 * --------------------------------------------------------- */
    .globl  asm_max
    .type   asm_max, @function
asm_max:
    mov     eax, edi
    cmp     eax, esi
    cmovl   eax, esi        /* conditional move: if a < b → eax = b */
    ret
    .size   asm_max, . - asm_max


/* ---------------------------------------------------------
 * 18. asm_min(int a, int b) -> int  — branchless min
 * --------------------------------------------------------- */
    .globl  asm_min
    .type   asm_min, @function
asm_min:
    mov     eax, edi
    cmp     eax, esi
    cmovg   eax, esi        /* conditional move: if a > b → eax = b */
    ret
    .size   asm_min, . - asm_min


/* ---------------------------------------------------------
 * 19. asm_loop_sum(int *arr, int len) -> int
 *     rdi = arr pointer,  rsi = length
 * --------------------------------------------------------- */
    .globl  asm_loop_sum
    .type   asm_loop_sum, @function
asm_loop_sum:
    xor     eax, eax        /* sum = 0 */
    xor     ecx, ecx        /* i   = 0 */
.Lloop:
    cmp     ecx, esi        /* i < len? */
    jge     .Ldone
    mov     r8d, DWORD PTR [rdi + rcx*4]   /* load arr[i] */
    add     eax, r8d        /* sum += arr[i] */
    inc     ecx             /* i++ */
    jmp     .Lloop
.Ldone:
    ret
    .size   asm_loop_sum, . - asm_loop_sum


/* ---------------------------------------------------------
 * 20. asm_swap(int *a, int *b)  — register swap via xor
 *     rdi = &a,  rsi = &b
 * --------------------------------------------------------- */
    .globl  asm_swap
    .type   asm_swap, @function
asm_swap:
    mov     eax,  DWORD PTR [rdi]   /* eax = *a */
    mov     r8d,  DWORD PTR [rsi]   /* r8d = *b */
    xor     eax,  r8d               /* eax ^= r8d */
    xor     r8d,  eax               /* r8d ^= eax (= original *a) */
    xor     eax,  r8d               /* eax ^= r8d (= original *b) */
    mov     DWORD PTR [rdi], eax
    mov     DWORD PTR [rsi], r8d
    ret
    .size   asm_swap, . - asm_swap


/* ---------------------------------------------------------
 * 21. asm_factorial(int n) -> int  (iterative, n<=12)
 * --------------------------------------------------------- */
    .globl  asm_factorial
    .type   asm_factorial, @function
asm_factorial:
    mov     ecx, edi        /* counter = n */
    mov     eax, 1          /* result  = 1 */
.Lfact_loop:
    cmp     ecx, 1
    jle     .Lfact_done
    imul    eax, ecx
    dec     ecx
    jmp     .Lfact_loop
.Lfact_done:
    ret
    .size   asm_factorial, . - asm_factorial


/* ---------------------------------------------------------
 * 22. asm_memset(void *dst, int c, size_t n)
 *     rdi=dst, rsi=c, rdx=n
 * --------------------------------------------------------- */
    .globl  asm_memset
    .type   asm_memset, @function
asm_memset:
    mov     rax, rdi        /* save return value (dst) */
    mov     al,  sil        /* byte to fill */
    mov     rcx, rdx        /* count */
    rep stosb               /* fill rcx bytes at [rdi] with al */
    mov     rax, rdi
    ret
    .size   asm_memset, . - asm_memset


/* ---------------------------------------------------------
 * .data section — initialized global variable
 * --------------------------------------------------------- */
    .data
    .globl  asm_global_var
asm_global_var:
    .long   0xCAFEBABE      /* 32-bit initialized data */

/* ---------------------------------------------------------
 * .bss section — zero-initialized buffer
 * --------------------------------------------------------- */
    .bss
    .globl  asm_buffer
asm_buffer:
    .zero   64              /* 64 bytes, zero-initialized */

/* ---------------------------------------------------------
 * .rodata section — read-only string
 * --------------------------------------------------------- */
    .section .rodata
asm_hello:
    .string "Hello from assembly!\n"

    .end
