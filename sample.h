#ifndef SAMPLE_H
#define SAMPLE_H

#include <stdint.h>
#include <stdbool.h>

/* Macros */
#define MAX_SIZE        10
#define SQUARE(x)       ((x) * (x))
#define PI              3.14159265358979323846

/* Typedef - primitive */
typedef unsigned int uint;
typedef float        f32;

/* Enum */
typedef enum {
    STATE_IDLE    = 0,
    STATE_RUNNING = 1,
    STATE_ERROR   = 2,
    STATE_DONE    = 3
} SystemState;

/* Struct */
typedef struct {
    uint8_t  id;
    char     name[32];
    f32      value;
    bool     is_active;
} Sensor;

/* Union */
typedef union {
    uint32_t raw;
    struct {
        uint8_t byte0;
        uint8_t byte1;
        uint8_t byte2;
        uint8_t byte3;
    } bytes;
} Register;

/* Function pointer typedef */
typedef void (*Callback)(SystemState state);

/* Function declarations */
void        system_init(void);
SystemState system_get_state(void);
void        system_set_state(SystemState state);
bool        sensor_read(Sensor *sensor);
f32         calculate_area(f32 radius);
int         array_sum(const int *arr, uint size);
void        register_callback(Callback cb);
void        swap(int *a, int *b);
int         factorial(int n);

#endif /* SAMPLE_H */
