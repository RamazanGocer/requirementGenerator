#include "sample.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <errno.h>

/* =========================================================
 * Storage class: file-scope static (internal linkage)
 * ========================================================= */
static SystemState  current_state  = STATE_IDLE;
static int          data_buffer[MAX_SIZE] = {1,2,3,4,5,6,7,8,9,10};
static Callback     event_callback = NULL;

/* extern declaration — defined in another TU in a real project */
extern int          external_counter;   /* declaration only */
int                 external_counter = 0;   /* definition */

/* volatile — signals compiler: don't optimize away reads/writes */
volatile uint32_t   hw_register = 0x00;

/* const — read-only */
const int           VERSION_MAJOR = 1;
const char * const  PRODUCT_NAME  = "ReqGen";

/* =========================================================
 * system_init — local variable types & qualifiers
 * ========================================================= */
void system_init(void) {
    /* auto (implicit) */
    int         count   = 0;
    float       ratio   = 0.5f;
    double      pi_val  = PI;
    char        ch      = 'A';
    bool        flag    = true;
    long        big     = 1000000L;
    long long   bigger  = 9000000000LL;
    unsigned    u       = 42u;
    short       s       = 32000;

    /* register hint (deprecated C17, still valid) */
    register int reg_var = 7;

    /* static local — retains value across calls */
    static int call_count = 0;
    call_count++;

    /* sizeof */
    printf("sizeof(int)=%zu sizeof(double)=%zu sizeof(Sensor)=%zu\n",
           sizeof(int), sizeof(double), sizeof(Sensor));

    /* explicit cast */
    double d_val = 9.99;
    int    i_val = (int)d_val;

    /* compound literal (C99) */
    Sensor tmp = (Sensor){ .id = 0, .value = 0.0f, .is_active = false };

    /* designated initializer (C99) for array */
    int arr[5] = { [0]=10, [2]=20, [4]=30 };

    printf("Init #%d: count=%d ratio=%.2f pi=%.5f ch=%c flag=%d big=%ld\n",
           call_count, count, ratio, pi_val, ch, flag, big);
    printf("bigger=%lld u=%u s=%d reg_var=%d cast=%d\n",
           bigger, u, s, reg_var, i_val);
    printf("compound: id=%d arr[2]=%d\n", tmp.id, arr[2]);

    /* assert */
    assert(MAX_SIZE > 0);

    /* LOG macro */
    LOG("system_init complete");

    current_state = STATE_RUNNING;
    (void)big; (void)bigger; (void)u; (void)s;
}

/* =========================================================
 * State machine — getter/setter
 * ========================================================= */
SystemState system_get_state(void) { return current_state; }

void system_set_state(SystemState state) {
    current_state = state;
    if (event_callback) event_callback(current_state);
}

/* =========================================================
 * Pointer to struct
 * ========================================================= */
bool sensor_read(Sensor *sensor) {
    if (!sensor) return false;
    sensor->id        = 1;
    sensor->value     = 23.5f;
    sensor->is_active = true;
    strncpy(sensor->name, "TempSensor", sizeof(sensor->name) - 1);
    sensor->name[sizeof(sensor->name)-1] = '\0';
    return true;
}

/* =========================================================
 * Macro: SQUARE, PI
 * ========================================================= */
f32 calculate_area(f32 radius) {
    return (f32)(PI * SQUARE(radius));
}

/* =========================================================
 * restrict pointer + pointer arithmetic
 * ========================================================= */
int array_sum(const int * restrict arr, uint size) {
    int sum = 0;
    for (uint i = 0; i < size; i++) sum += *(arr + i);
    return sum;
}

/* =========================================================
 * Pointer swap
 * ========================================================= */
void swap(int *a, int *b) {
    int t = *a; *a = *b; *b = t;
}

/* =========================================================
 * Multi-dimensional array (VLA style, C99)
 * ========================================================= */
void matrix_fill(int rows, int cols, int matrix[rows][cols]) {
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            matrix[r][c] = r * cols + c;
}

/* =========================================================
 * Double pointer
 * ========================================================= */
void str_duplicate(const char *src, char **out) {
    size_t len = strlen(src) + 1;
    *out = (char *)malloc(len);
    if (*out) memcpy(*out, src, len);
}

/* =========================================================
 * void * generic pointer
 * ========================================================= */
void generic_print(const void *data, const char *type) {
    if (strcmp(type, "int") == 0)
        printf("generic int: %d\n", *(const int *)data);
    else if (strcmp(type, "float") == 0)
        printf("generic float: %.2f\n", *(const float *)data);
}

/* =========================================================
 * Linked list
 * ========================================================= */
Node *list_push(Node *head, int val) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (!n) return head;
    n->data = val;
    n->next = head;
    return n;
}

void list_free(Node *head) {
    while (head) { Node *t = head->next; free(head); head = t; }
}

/* =========================================================
 * Flexible array member
 * ========================================================= */
FlexArray *flex_array_create(uint32_t length) {
    FlexArray *fa = (FlexArray *)malloc(sizeof(FlexArray) + length * sizeof(int));
    if (!fa) return NULL;
    fa->length = length;
    for (uint32_t i = 0; i < length; i++) fa->data[i] = (int)i;
    return fa;
}

/* =========================================================
 * Callback registration
 * ========================================================= */
void register_callback(Callback cb) { event_callback = cb; }

int int_comparator(const void *a, const void *b) {
    return (*(const int *)a) - (*(const int *)b);
}

/* =========================================================
 * Recursion
 * ========================================================= */
int factorial(int n) { return n <= 1 ? 1 : n * factorial(n - 1); }

int fibonacci(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fibonacci(n-1) + fibonacci(n-2);
}

/* =========================================================
 * Variadic function
 * ========================================================= */
int sum_variadic(int count, ...) {
    va_list args;
    va_start(args, count);
    int total = 0;
    for (int i = 0; i < count; i++) total += va_arg(args, int);
    va_end(args);
    return total;
}

void log_message(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

/* =========================================================
 * _Noreturn (C11)
 * ========================================================= */
#ifdef HAS_C11
_Noreturn void fatal_error(const char *msg) {
    fprintf(stderr, "FATAL: %s (errno=%d)\n", msg, errno);
    exit(EXIT_FAILURE);
}
#endif

/* =========================================================
 * setjmp / longjmp
 * ========================================================= */
static jmp_buf jmp_env;

void setjmp_demo(void) {
    int code = setjmp(jmp_env);
    if (code == 0) {
        printf("setjmp: first call\n");
        longjmp(jmp_env, 42);
    } else {
        printf("setjmp: returned via longjmp, code=%d\n", code);
    }
}

/* =========================================================
 * File I/O
 * ========================================================= */
void file_io_demo(void) {
    const char *path = "demo_output.txt";

    /* write */
    FILE *fp = fopen(path, "w");
    if (!fp) { perror("fopen"); return; }
    fprintf(fp, "requirementGenerator file I/O demo\n");
    fputs("second line\n", fp);
    fclose(fp);

    /* read */
    fp = fopen(path, "r");
    if (!fp) { perror("fopen"); return; }
    char line[128];
    while (fgets(line, sizeof(line), fp)) printf("[file] %s", line);
    fclose(fp);

    /* binary write/read */
    int nums[3] = {10, 20, 30};
    fp = fopen("demo_bin.bin", "wb");
    if (fp) { fwrite(nums, sizeof(int), 3, fp); fclose(fp); }

    int read_nums[3] = {0};
    fp = fopen("demo_bin.bin", "rb");
    if (fp) { fread(read_nums, sizeof(int), 3, fp); fclose(fp); }
    printf("[bin] %d %d %d\n", read_nums[0], read_nums[1], read_nums[2]);
}

/* =========================================================
 * Inline assembly (GCC / MinGW — x86-64)
 * ========================================================= */
void asm_demo(void) {
    /* 1. Basic inline asm — NOP (no-operation) */
    __asm__ __volatile__ ("nop");
    printf("[asm] NOP executed\n");

    /* 2. Extended inline asm — add two integers */
    int a = 15, b = 27, result = 0;
    __asm__ __volatile__ (
        "addl %2, %0"          /* result = result + b  (AT&T syntax) */
        : "=r" (result)        /* output: result in any register */
        : "0"  (a), "r" (b)   /* inputs: result←a, b in register  */
        :                      /* clobbers: none */
    );
    printf("[asm] %d + %d = %d\n", a, b, result);

    /* 3. Inline asm — read RFLAGS (x86-64 only) */
    uint64_t rflags = 0;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "popq %0"
        : "=r" (rflags)
        :
        : "memory"
    );
    printf("[asm] RFLAGS=0x%016llX\n", (unsigned long long)rflags);

    /* 4. Inline asm — CPUID: vendor string */
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );
    char vendor[13];
    memcpy(vendor,     &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    vendor[12] = '\0';
    printf("[asm] CPU vendor: %s\n", vendor);

    /* 5. Memory barrier */
    __asm__ __volatile__ ("mfence" ::: "memory");
    printf("[asm] memory fence done\n");

    /* 6. Bit scan — find lowest set bit position */
    uint32_t val = 0b10110100;
    uint32_t bit_pos = 0;
    __asm__ __volatile__ (
        "bsfl %1, %0"
        : "=r" (bit_pos)
        : "r"  (val)
    );
    printf("[asm] BSF(0x%02X)=%u\n", val, bit_pos);

    /* 7. XCHG — atomic swap */
    int x = 100, y = 200;
    __asm__ __volatile__ (
        "xchgl %0, %1"
        : "+r"(x), "+m"(y)
    );
    printf("[asm] XCHG: x=%d y=%d\n", x, y);

    /* 8. Rotate left (ROL) */
    uint32_t rotval = 0x01, rotated = 0;
    __asm__ __volatile__ (
        "roll $4, %0"
        : "=r"(rotated)
        : "0"(rotval)
    );
    printf("[asm] ROL(0x%08X, 4)=0x%08X\n", rotval, rotated);
}

/* =========================================================
 * main — all constructs exercised
 * ========================================================= */
int main(void) {

    /* --- 1. Init & state machine --- */
    system_init();
    system_init();  /* static local call_count increments */

    /* --- 2. Enum & switch --- */
    SystemState st = system_get_state();
    switch (st) {
        case STATE_IDLE:    puts("Idle");    break;
        case STATE_RUNNING: puts("Running"); break;
        case STATE_ERROR:   puts("Error");   break;
        case STATE_DONE:    puts("Done");    break;
        default:            puts("Unknown"); break;
    }

    /* --- 3. Struct & pointer to struct --- */
    Sensor sensor;
    memset(&sensor, 0, sizeof(sensor));
    if (sensor_read(&sensor))
        printf("Sensor: %s val=%.2f\n", sensor.name, sensor.value);

    /* --- 4. Bit field struct --- */
    StatusReg sr = { .power=1, .mode=2, .error=0 };
    printf("BitField: power=%d mode=%d error=%d\n", sr.power, sr.mode, sr.error);

    /* --- 5. Union with anonymous struct (C11) --- */
    Register reg = { .raw = 0xDEADBEEF };
    printf("Union: raw=0x%08X bytes=%02X %02X %02X %02X\n",
           reg.raw, reg.byte0, reg.byte1, reg.byte2, reg.byte3);

    /* --- 6. Array of structs --- */
    Sensor sensors[3];
    for (int i = 0; i < 3; i++) {
        sensors[i].id    = (uint8_t)i;
        sensors[i].value = (f32)i * 1.5f;
        sensors[i].is_active = true;
    }
    printf("Sensors[1].value=%.2f\n", sensors[1].value);

    /* --- 7. Multi-dimensional array --- */
    int matrix[3][4];
    matrix_fill(3, 4, matrix);
    printf("matrix[2][3]=%d\n", matrix[2][3]);

    /* --- 8. VLA (C99) --- */
    int n = 5;
    int vla[n];
    for (int i = 0; i < n; i++) vla[i] = i * i;
    printf("VLA[4]=%d\n", vla[4]);

    /* --- 9. Array of pointers --- */
    const char *words[] = {"hello", "world", "C"};
    for (int i = 0; i < 3; i++) printf("word[%d]=%s\n", i, words[i]);

    /* --- 10. Pointer to array --- */
    int (*ptr_arr)[MAX_SIZE] = &data_buffer;
    printf("ptr_arr[0][0]=%d\n", (*ptr_arr)[0]);

    /* --- 11. Double pointer --- */
    char *dup = NULL;
    str_duplicate("Hello Double Pointer", &dup);
    printf("dup=%s\n", dup);
    free(dup);

    /* --- 12. void * generic --- */
    int   gint   = 99;
    float gfloat = 3.14f;
    generic_print(&gint,   "int");
    generic_print(&gfloat, "float");

    /* --- 13. const pointer vs pointer to const --- */
    const int  ci = 42;
    const int *pc   = &ci;          /* pointer to const int */
    int       *const cp = &gint;    /* const pointer to int */
    printf("pc=%d cp=%d\n", *pc, *cp);

    /* --- 14. volatile --- */
    hw_register = 0xFF;
    printf("hw_reg=0x%02X\n", hw_register);

    /* --- 15. while loop --- */
    int i = 0;
    while (i < 3) { printf("while i=%d\n", i); i++; }

    /* --- 16. do-while --- */
    int j = 0;
    do { printf("do j=%d\n", j); j++; } while (j < 3);

    /* --- 17. for with break/continue --- */
    for (int k = 0; k < 10; k++) {
        if (k == 2) continue;
        if (k == 5) break;
        printf("for k=%d\n", k);
    }

    /* --- 18. Nested loops with label-goto --- */
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            if (r == 1 && c == 1) goto nested_done;
nested_done:
    puts("nested break via goto");

    /* --- 19. Swap --- */
    int x = 10, y = 20;
    swap(&x, &y);
    printf("swap: x=%d y=%d\n", x, y);

    /* --- 20. Recursion --- */
    printf("6!=%d fib(8)=%d\n", factorial(6), fibonacci(8));

    /* --- 21. Variadic function --- */
    printf("sum(1..5)=%d\n", sum_variadic(5, 1, 2, 3, 4, 5));
    log_message("variadic log: val=%d name=%s", 7, "test");

    /* --- 22. Array sum & qsort --- */
    int sort_arr[] = {5, 3, 9, 1, 7, 2, 8, 4, 6, 0};
    qsort(sort_arr, 10, sizeof(int), int_comparator);
    printf("sorted: ");
    for (int k = 0; k < 10; k++) printf("%d ", sort_arr[k]);
    printf("\n");

    /* --- 23. calloc / realloc --- */
    int *carr = (int *)calloc(5, sizeof(int));
    printf("calloc[0]=%d\n", carr[0]);  /* must be 0 */
    carr = (int *)realloc(carr, 10 * sizeof(int));
    carr[9] = 99;
    printf("realloc[9]=%d\n", carr[9]);
    free(carr);

    /* --- 24. memset / memcpy / memmove --- */
    char buf1[16], buf2[16];
    memset(buf1, 'A', sizeof(buf1));
    buf1[15] = '\0';
    memcpy(buf2, buf1, sizeof(buf1));
    memmove(buf1 + 2, buf1, 10);
    printf("memcpy=%s\n", buf2);

    /* --- 25. Macro: STRINGIFY, CONCAT --- */
    printf("STRINGIFY(MAX_SIZE)=%s\n", TO_STR(MAX_SIZE));
    int MAKE_VAR(42) = 9;        /* expands to var_42 */
    printf("CONCAT var: %d\n", var_42);

    /* --- 26. inline clamp --- */
    printf("clamp(15,0,10)=%d\n", clamp(15, 0, 10));

    /* --- 27. Ternary --- */
    int val = -5;
    printf("ABS(-5)=%d\n", ABS(val));
    printf("MAX(3,7)=%d\n", MAX(3, 7));

    /* --- 28. Bitwise ops --- */
    uint8_t flags = 0x00;
    flags |=  (1 << 2);   /* set   bit 2 */
    flags &= ~(1 << 1);   /* clear bit 1 */
    flags ^=  (1 << 3);   /* toggle bit 3 */
    printf("flags=0x%02X\n", flags);

    /* --- 29. Bit shift --- */
    uint32_t shifted = 1u << 16;
    printf("1<<16=0x%08X\n", shifted);

    /* --- 30. Linked list --- */
    Node *list = NULL;
    for (int k = 1; k <= 5; k++) list = list_push(list, k * 10);
    printf("list: ");
    for (Node *cur = list; cur; cur = cur->next) printf("%d ", cur->data);
    printf("\n");
    list_free(list);

    /* --- 31. Flexible array member --- */
    FlexArray *fa = flex_array_create(4);
    printf("flex[3]=%d\n", fa->data[3]);
    free(fa);

    /* --- 32. Function pointer --- */
    Callback cb = NULL;
    register_callback(cb);
    system_set_state(STATE_DONE);

    /* --- 33. Comparator as function pointer --- */
    Comparator cmp = int_comparator;
    int v1 = 3, v2 = 7;
    printf("cmp(3,7)=%d\n", cmp(&v1, &v2));

    /* --- 34. time.h --- */
    time_t now = time(NULL);
    printf("time=%ld\n", (long)now);

    /* --- 35. errno --- */
    FILE *bad = fopen("nonexistent_file_xyz.bin", "r");
    if (!bad) printf("errno=%d\n", errno);

    /* --- 36. setjmp/longjmp --- */
    setjmp_demo();

    /* --- 37. File I/O --- */
    file_io_demo();

    /* --- 38. _Static_assert (C11) --- */
#ifdef HAS_C11
    _Static_assert(MAX_SIZE == 10, "MAX_SIZE must be 10");
#endif

    /* --- 39. Inline assembly --- */
    asm_demo();

    /* --- 40. LOGF variadic macro --- */
    LOGF("done. version=%d.0", VERSION_MAJOR);

    return 0;
}
