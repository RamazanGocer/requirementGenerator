#ifndef SAMPLE_H
#define SAMPLE_H

/* =========================================================
 * Preprocessor directives
 * ========================================================= */
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <setjmp.h>

#define C_VERSION   11       /* target C standard */

/* Conditional compilation */
#if C_VERSION >= 11
#  define HAS_C11   1
#elif C_VERSION >= 99
#  define HAS_C11   0
#else
#  error "C99 or later required"
#endif

/* Object-like macro */
#define MAX_SIZE        10
#define PI              3.14159265358979323846

/* Function-like macro */
#define SQUARE(x)       ((x) * (x))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define ABS(x)          ((x) < 0 ? -(x) : (x))

/* Stringification */
#define STRINGIFY(x)    #x
#define TO_STR(x)       STRINGIFY(x)

/* Token pasting */
#define CONCAT(a, b)    a##b
#define MAKE_VAR(n)     var_##n

/* Predefined macros usage wrapper */
#define LOG(msg)        printf("[%s:%d %s] " msg "\n", __FILE__, __LINE__, __func__)

/* Variadic macro */
#define LOGF(fmt, ...)  printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

/* Pragma */
#pragma once   /* redundant with ifndef guard — shown for demonstration */

/* Undef & redefine */
#ifdef MAX_SIZE
#  undef  MAX_SIZE
#  define MAX_SIZE 10
#endif

/* =========================================================
 * Typedefs - primitive aliases
 * ========================================================= */
typedef unsigned int   uint;
typedef float          f32;
typedef double         f64;
typedef unsigned char  u8;
typedef signed char    s8;
typedef uint16_t       u16;
typedef int32_t        s32;

/* =========================================================
 * Enum
 * ========================================================= */
typedef enum {
    STATE_IDLE    = 0,
    STATE_RUNNING = 1,
    STATE_ERROR   = 2,
    STATE_DONE    = 3
} SystemState;

typedef enum Color { RED, GREEN, BLUE } Color;

/* =========================================================
 * Struct — plain, with bit fields, with flexible array member
 * ========================================================= */
typedef struct {
    uint8_t  id;
    char     name[32];
    f32      value;
    bool     is_active;
} Sensor;

/* Bit fields */
typedef struct {
    uint8_t power   : 1;
    uint8_t mode    : 2;
    uint8_t error   : 1;
    uint8_t reserved: 4;
} StatusReg;

/* Flexible array member (C99) */
typedef struct {
    uint32_t length;
    int      data[];    /* flexible array member — must be last */
} FlexArray;

/* =========================================================
 * Union — named and anonymous
 * ========================================================= */
typedef union {
    uint32_t raw;
    struct {            /* anonymous struct inside union (C11) */
        uint8_t byte0;
        uint8_t byte1;
        uint8_t byte2;
        uint8_t byte3;
    };
} Register;

/* =========================================================
 * Linked list node
 * ========================================================= */
typedef struct Node {
    int          data;
    struct Node *next;
} Node;

/* =========================================================
 * Function pointer typedefs
 * ========================================================= */
typedef void (*Callback)(SystemState state);
typedef int  (*Comparator)(const void *, const void *);

/* =========================================================
 * inline function (C99)
 * ========================================================= */
static inline int clamp(int val, int lo, int hi) {
    return val < lo ? lo : (val > hi ? hi : val);
}

/* =========================================================
 * _Static_assert (C11)
 * ========================================================= */
#ifdef HAS_C11
_Static_assert(sizeof(int) >= 2, "int must be at least 16-bit");
#endif

/* =========================================================
 * _Noreturn (C11)
 * ========================================================= */
#ifdef HAS_C11
_Noreturn void fatal_error(const char *msg);
#endif

/* =========================================================
 * Function declarations
 * ========================================================= */
/* Basic */
void        system_init(void);
SystemState system_get_state(void);
void        system_set_state(SystemState state);

/* Pointer & array */
bool        sensor_read(Sensor *sensor);
f32         calculate_area(f32 radius);
int         array_sum(const int * restrict arr, uint size);
void        swap(int *a, int *b);
void        matrix_fill(int rows, int cols, int matrix[rows][cols]);

/* Double pointer */
void        str_duplicate(const char *src, char **out);

/* void pointer */
void        generic_print(const void *data, const char *type);

/* Dynamic memory */
Node       *list_push(Node *head, int val);
void        list_free(Node *head);
FlexArray  *flex_array_create(uint32_t length);

/* Callbacks / qsort */
void        register_callback(Callback cb);
int         int_comparator(const void *a, const void *b);

/* Recursion */
int         factorial(int n);
int         fibonacci(int n);

/* Variadic */
int         sum_variadic(int count, ...);
void        log_message(const char *fmt, ...);

/* setjmp/longjmp */
void        setjmp_demo(void);

/* File I/O */
void        file_io_demo(void);

/* Inline assembly */
void        asm_demo(void);

#endif /* SAMPLE_H */
