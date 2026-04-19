#include "sample.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Global variable */
static SystemState current_state = STATE_IDLE;

/* Global array */
static int data_buffer[MAX_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

/* Callback holder */
static Callback event_callback = NULL;

/* --- Basic I/O & Variables --- */
void system_init(void) {
    /* Local variables - all primitive types */
    int    count    = 0;
    float  ratio    = 0.5f;
    double pi_val   = PI;
    char   ch       = 'A';
    bool   flag     = true;
    long   big_num  = 1000000L;

    printf("Init: count=%d ratio=%.2f pi=%.5f ch=%c flag=%d big=%ld\n",
           count, ratio, pi_val, ch, flag, big_num);

    current_state = STATE_RUNNING;
}

/* --- Getter / Setter --- */
SystemState system_get_state(void) {
    return current_state;
}

void system_set_state(SystemState state) {
    current_state = state;
    if (event_callback != NULL) {
        event_callback(current_state);
    }
}

/* --- Pointer usage --- */
bool sensor_read(Sensor *sensor) {
    if (sensor == NULL) {
        return false;
    }
    sensor->id        = 1;
    sensor->value     = 23.5f;
    sensor->is_active = true;
    strncpy(sensor->name, "TempSensor", sizeof(sensor->name) - 1);
    return true;
}

/* --- Math & macro --- */
f32 calculate_area(f32 radius) {
    return (f32)(PI * SQUARE(radius));
}

/* --- Array & pointer arithmetic --- */
int array_sum(const int *arr, uint size) {
    int    sum = 0;
    uint   i;
    for (i = 0; i < size; i++) {
        sum += *(arr + i);
    }
    return sum;
}

/* --- Function pointer & callback --- */
void register_callback(Callback cb) {
    event_callback = cb;
}

/* --- Pointer swap --- */
void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/* --- Recursion --- */
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

/* --- Main: all constructs demonstrated --- */
int main(void) {
    /* 1. Init & state machine */
    system_init();

    /* 2. Enum & switch */
    SystemState st = system_get_state();
    switch (st) {
        case STATE_IDLE:    printf("Idle\n");    break;
        case STATE_RUNNING: printf("Running\n"); break;
        case STATE_ERROR:   printf("Error\n");   break;
        case STATE_DONE:    printf("Done\n");    break;
        default:            printf("Unknown\n"); break;
    }

    /* 3. Struct */
    Sensor sensor;
    if (sensor_read(&sensor)) {
        printf("Sensor: %s val=%.2f\n", sensor.name, sensor.value);
    }

    /* 4. Union */
    Register reg;
    reg.raw = 0xDEADBEEF;
    printf("Union bytes: %02X %02X %02X %02X\n",
           reg.bytes.byte0, reg.bytes.byte1,
           reg.bytes.byte2, reg.bytes.byte3);

    /* 5. Array & pointer */
    int sum = array_sum(data_buffer, MAX_SIZE);
    printf("Array sum: %d\n", sum);

    /* 6. While loop */
    int i = 0;
    while (i < 3) {
        printf("while i=%d\n", i);
        i++;
    }

    /* 7. Do-while loop */
    int j = 0;
    do {
        printf("do-while j=%d\n", j);
        j++;
    } while (j < 3);

    /* 8. For loop with break/continue */
    for (int k = 0; k < MAX_SIZE; k++) {
        if (k == 2) continue;
        if (k == 5) break;
        printf("for k=%d\n", k);
    }

    /* 9. Swap via pointers */
    int x = 10, y = 20;
    swap(&x, &y);
    printf("After swap: x=%d y=%d\n", x, y);

    /* 10. Recursion */
    printf("5! = %d\n", factorial(5));

    /* 11. Area via macro */
    printf("Area r=3: %.2f\n", calculate_area(3.0f));

    /* 12. Dynamic memory */
    int *heap_arr = (int *)malloc(5 * sizeof(int));
    if (heap_arr != NULL) {
        for (int m = 0; m < 5; m++) heap_arr[m] = m * 2;
        printf("Heap: ");
        for (int m = 0; m < 5; m++) printf("%d ", heap_arr[m]);
        printf("\n");
        free(heap_arr);
    }

    /* 13. Ternary operator */
    int val = 42;
    printf("val is %s\n", (val > 0) ? "positive" : "non-positive");

    /* 14. Bitwise operations */
    uint8_t flags = 0x00;
    flags |= (1 << 2);   /* set bit 2 */
    flags &= ~(1 << 1);  /* clear bit 1 */
    flags ^= (1 << 3);   /* toggle bit 3 */
    printf("flags: 0x%02X\n", flags);

    /* 15. goto */
    int retry = 0;
retry_label:
    if (retry < 2) {
        printf("retry=%d\n", retry);
        retry++;
        goto retry_label;
    }

    /* 16. Callback */
    register_callback(NULL);
    system_set_state(STATE_DONE);

    return 0;
}
