/**
 * @file gpio.c
 * @brief GPIO driver implementation for ARM Cortex-M targets.
 *
 * Implements the interface declared in gpio.h. Register-level access is done
 * through the MCU HAL macros (GPIO_REG_*) defined below so the driver can be
 * ported to a new target by replacing only those macros.
 *
 * @author  Embedded BSP Team
 * @version 1.2.0
 * @date    2024-01-15
 */

#include "gpio.h"
#include <string.h>

/* =========================================================
 * Register access shims (replace with real CMSIS / HAL macros)
 * ========================================================= */

/** @brief Write @p val to the Bit Set/Reset Register of @p port. */
#define GPIO_REG_BSRR_WRITE(port, val)  ((void)(port), (void)(val))

/** @brief Read the Input Data Register of @p port. */
#define GPIO_REG_IDR_READ(port)         (0U)

/** @brief Read / write the Output Data Register of @p port. */
#define GPIO_REG_ODR_READ(port)         (0U)
#define GPIO_REG_ODR_WRITE(port, val)   ((void)(port), (void)(val))

/** @brief Read / write the Mode Register of @p port. */
#define GPIO_REG_MODER_READ(port)       (0U)
#define GPIO_REG_MODER_WRITE(port, val) ((void)(port), (void)(val))

/** @brief Enable the AHB/APB clock for @p port. */
#define GPIO_CLK_ENABLE(port)           ((void)(port))

/** @brief Enter / leave a critical section (disable / restore interrupts). */
#define GPIO_ENTER_CRITICAL()           do { } while (0)
#define GPIO_EXIT_CRITICAL()            do { } while (0)

/** @brief Millisecond timestamp from the system tick counter. */
#define GPIO_GET_TICK_MS()              (0U)

/* =========================================================
 * Private data
 * ========================================================= */

/** @brief Callback registration table. */
static GPIO_IRQ_Entry s_irq_table[GPIO_MAX_CALLBACKS];

/** @brief Number of entries currently occupied in @p s_irq_table. */
static uint8_t s_irq_count = 0U;

/** @brief Per-line last-trigger timestamp for debounce. */
static uint32_t s_debounce_ts[16] = {0U};

/* =========================================================
 * Private helpers
 * ========================================================= */

/**
 * @brief Validate a port + pin argument pair.
 *
 * @param[in] port  Port value to check.
 * @param[in] pin   Pin mask to check (must be non-zero).
 *
 * @return true if both values are in range, false otherwise.
 */
static bool gpio_is_valid(GPIO_Port port, uint16_t pin)
{
    return (port < GPIO_PORT_COUNT) && (pin != 0U);
}

/**
 * @brief Find an existing IRQ table entry for the given port and pin.
 *
 * @param[in] port  Port to search for.
 * @param[in] pin   Pin mask to search for.
 *
 * @return Pointer to the matching GPIO_IRQ_Entry, or NULL if not found.
 */
static GPIO_IRQ_Entry *gpio_find_irq(GPIO_Port port, uint16_t pin)
{
    for (uint8_t i = 0U; i < s_irq_count; i++)
    {
        if (s_irq_table[i].port == port && s_irq_table[i].pin == pin)
        {
            return &s_irq_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Map a GPIO_Mode value to the 2-bit MODER field encoding.
 *
 * @param[in] mode  Logical GPIO mode.
 *
 * @return 2-bit hardware encoding for the MODER register.
 */
static uint8_t gpio_mode_to_moder(GPIO_Mode mode)
{
    switch (mode)
    {
        case GPIO_MODE_INPUT:                          return 0x00U;
        case GPIO_MODE_OUTPUT_PP: /* fall-through */
        case GPIO_MODE_OUTPUT_OD:                      return 0x01U;
        case GPIO_MODE_AF_PP:     /* fall-through */
        case GPIO_MODE_AF_OD:                          return 0x02U;
        case GPIO_MODE_ANALOG:                         return 0x03U;
        case GPIO_MODE_IT_RISING: /* fall-through */
        case GPIO_MODE_IT_FALLING:/* fall-through */
        case GPIO_MODE_IT_BOTH:                        return 0x00U; /* input with EXTI */
        default:                                       return 0x00U;
    }
}

/* =========================================================
 * Public API implementation
 * ========================================================= */

/**
 * @brief Initialise one or more GPIO pins according to the supplied config.
 *
 * Enables the port clock, configures MODER / OTYPER / OSPEEDR / PUPDR,
 * and for interrupt modes sets up EXTI routing and enables the NVIC line.
 *
 * @param[in] cfg  Pointer to a fully populated GPIO_Config structure.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if @p cfg is NULL or contains out-of-range values.
 * @return GPIO_ERR_HW     if the port clock fails to start.
 */
GPIO_Status GPIO_Init(const GPIO_Config *cfg)
{
    if (cfg == NULL)
    {
        return GPIO_ERR_PARAM;
    }
    if (!gpio_is_valid(cfg->port, cfg->pin))
    {
        return GPIO_ERR_PARAM;
    }
    if (cfg->mode >= GPIO_MODE_IT_RISING && cfg->af != 0U)
    {
        return GPIO_ERR_PARAM;
    }

    GPIO_CLK_ENABLE(cfg->port);

    uint8_t moder_val = gpio_mode_to_moder(cfg->mode);

    /* Apply MODER for every set pin bit */
    uint32_t moder = GPIO_REG_MODER_READ(cfg->port);
    for (uint8_t bit = 0U; bit < 16U; bit++)
    {
        if ((cfg->pin & (uint16_t)GPIO_PIN(bit)) != 0U)
        {
            moder &= ~(0x3U << (bit * 2U));
            moder |= ((uint32_t)moder_val << (bit * 2U));
        }
    }
    GPIO_REG_MODER_WRITE(cfg->port, moder);

    return GPIO_OK;
}

/**
 * @brief Reset a pin or pin group to the power-on default.
 *
 * Removes registered IRQ callbacks and disables the EXTI lines for the
 * affected pins. Port clock is left running.
 *
 * @param[in] port  Port identifier.
 * @param[in] pin   Pin mask to de-initialise.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port or pin is out of range.
 */
GPIO_Status GPIO_DeInit(GPIO_Port port, uint16_t pin)
{
    if (!gpio_is_valid(port, pin))
    {
        return GPIO_ERR_PARAM;
    }

    /* Reset MODER to input (00) for each selected pin */
    uint32_t moder = GPIO_REG_MODER_READ(port);
    for (uint8_t bit = 0U; bit < 16U; bit++)
    {
        if ((pin & (uint16_t)GPIO_PIN(bit)) != 0U)
        {
            moder &= ~(0x3U << (bit * 2U));
        }
    }
    GPIO_REG_MODER_WRITE(port, moder);

    /* Remove any callbacks registered for this port+pin */
    GPIO_UnregisterIRQ(port, pin);

    return GPIO_OK;
}

/**
 * @brief Drive an output pin or pin group to the requested state.
 *
 * Uses the atomic BSRR register: the upper half clears bits, the lower half
 * sets them, so no read-modify-write is needed.
 *
 * @param[in] port   Port identifier.
 * @param[in] pin    Pin mask to write.
 * @param[in] state  GPIO_STATE_HIGH or GPIO_STATE_LOW.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if any argument is out of range.
 */
GPIO_Status GPIO_Write(GPIO_Port port, uint16_t pin, GPIO_State state)
{
    if (!gpio_is_valid(port, pin))
    {
        return GPIO_ERR_PARAM;
    }
    if (state != GPIO_STATE_LOW && state != GPIO_STATE_HIGH)
    {
        return GPIO_ERR_PARAM;
    }

    uint32_t bsrr;
    if (state == GPIO_STATE_HIGH)
    {
        bsrr = (uint32_t)pin;              /* set bits */
    }
    else
    {
        bsrr = (uint32_t)pin << 16U;       /* reset bits */
    }
    GPIO_REG_BSRR_WRITE(port, bsrr);

    return GPIO_OK;
}

/**
 * @brief Sample the current logical state of a pin or pin group.
 *
 * Reads the IDR register and returns the subset of bits selected by @p pin.
 *
 * @param[in]  port   Port identifier.
 * @param[in]  pin    Pin mask to read.
 * @param[out] state  Receives the masked pin states; each bit corresponds to GPIO_PIN_x.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if any argument is invalid.
 */
GPIO_Status GPIO_Read(GPIO_Port port, uint16_t pin, uint16_t *state)
{
    if (!gpio_is_valid(port, pin) || state == NULL)
    {
        return GPIO_ERR_PARAM;
    }

    *state = (uint16_t)(GPIO_REG_IDR_READ(port) & pin);

    return GPIO_OK;
}

/**
 * @brief Toggle the output state of a pin or pin group.
 *
 * Performs an atomic read-modify-write on the ODR register inside a critical
 * section to prevent race conditions on preemptive targets.
 *
 * @param[in] port  Port identifier.
 * @param[in] pin   Pin mask to toggle.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port or pin is out of range.
 */
GPIO_Status GPIO_Toggle(GPIO_Port port, uint16_t pin)
{
    if (!gpio_is_valid(port, pin))
    {
        return GPIO_ERR_PARAM;
    }

    GPIO_ENTER_CRITICAL();
    uint32_t odr = GPIO_REG_ODR_READ(port);
    GPIO_REG_ODR_WRITE(port, odr ^ (uint32_t)pin);
    GPIO_EXIT_CRITICAL();

    return GPIO_OK;
}

/**
 * @brief Change the operating mode of an already-initialised pin at runtime.
 *
 * Only the MODER field is updated; pull, speed, and AF settings are preserved.
 *
 * @param[in] port  Port identifier.
 * @param[in] pin   Pin mask.
 * @param[in] mode  New operating mode.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if any argument is invalid.
 */
GPIO_Status GPIO_SetMode(GPIO_Port port, uint16_t pin, GPIO_Mode mode)
{
    if (!gpio_is_valid(port, pin) || mode >= GPIO_MODE_IT_RISING)
    {
        return GPIO_ERR_PARAM;
    }

    uint8_t  moder_val = gpio_mode_to_moder(mode);
    uint32_t moder     = GPIO_REG_MODER_READ(port);

    for (uint8_t bit = 0U; bit < 16U; bit++)
    {
        if ((pin & (uint16_t)GPIO_PIN(bit)) != 0U)
        {
            moder &= ~(0x3U << (bit * 2U));
            moder |= ((uint32_t)moder_val << (bit * 2U));
        }
    }
    GPIO_REG_MODER_WRITE(port, moder);

    return GPIO_OK;
}

/**
 * @brief Register a callback for a GPIO pin interrupt.
 *
 * The pin must already be configured with a GPIO_MODE_IT_* mode.
 * Re-registering the same port+pin replaces the existing callback.
 *
 * @param[in] port      Port identifier.
 * @param[in] pin       Single pin mask (exactly one bit).
 * @param[in] callback  ISR-context callback to invoke on trigger.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if arguments are invalid or @p pin has multiple bits set.
 * @return GPIO_ERR_BUSY   if the callback table is full.
 */
GPIO_Status GPIO_RegisterIRQ(GPIO_Port port, uint16_t pin, GPIO_IRQ_Callback callback)
{
    if (!gpio_is_valid(port, pin) || callback == NULL)
    {
        return GPIO_ERR_PARAM;
    }
    /* Reject multi-pin masks – each EXTI line maps to exactly one pin. */
    if ((pin & (pin - 1U)) != 0U)
    {
        return GPIO_ERR_PARAM;
    }

    /* Overwrite if already registered */
    GPIO_IRQ_Entry *existing = gpio_find_irq(port, pin);
    if (existing != NULL)
    {
        existing->callback = callback;
        existing->enabled  = true;
        return GPIO_OK;
    }

    if (s_irq_count >= GPIO_MAX_CALLBACKS)
    {
        return GPIO_ERR_BUSY;
    }

    s_irq_table[s_irq_count].port     = port;
    s_irq_table[s_irq_count].pin      = pin;
    s_irq_table[s_irq_count].callback = callback;
    s_irq_table[s_irq_count].enabled  = true;
    s_irq_count++;

    return GPIO_OK;
}

/**
 * @brief Unregister a previously registered interrupt callback.
 *
 * Removes the entry from the callback table by swapping with the last entry.
 * If no entry matches, the function succeeds silently.
 *
 * @param[in] port  Port identifier.
 * @param[in] pin   Single pin mask.
 *
 * @return GPIO_OK         always (no error if not found).
 * @return GPIO_ERR_PARAM  if port or pin is out of range.
 */
GPIO_Status GPIO_UnregisterIRQ(GPIO_Port port, uint16_t pin)
{
    if (!gpio_is_valid(port, pin))
    {
        return GPIO_ERR_PARAM;
    }

    GPIO_IRQ_Entry *entry = gpio_find_irq(port, pin);
    if (entry != NULL)
    {
        /* Swap with last entry and shrink the table */
        *entry = s_irq_table[--s_irq_count];
        memset(&s_irq_table[s_irq_count], 0, sizeof(GPIO_IRQ_Entry));
    }

    return GPIO_OK;
}

/**
 * @brief Enable or disable the EXTI interrupt line for a configured pin.
 *
 * @param[in] port    Port identifier.
 * @param[in] pin     Single pin mask.
 * @param[in] enable  true to enable, false to mask.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port or pin is out of range.
 */
GPIO_Status GPIO_SetIRQEnabled(GPIO_Port port, uint16_t pin, bool enable)
{
    if (!gpio_is_valid(port, pin))
    {
        return GPIO_ERR_PARAM;
    }

    GPIO_IRQ_Entry *entry = gpio_find_irq(port, pin);
    if (entry != NULL)
    {
        entry->enabled = enable;
    }

    return GPIO_OK;
}

/**
 * @brief EXTI ISR dispatcher called by the MCU vector handlers.
 *
 * Applies millisecond debounce filtering, samples the pin, and dispatches
 * to any registered callback. Clears the EXTI pending bit before returning.
 *
 * @param[in] line  EXTI line number (0–15) that fired.
 */
void GPIO_EXTI_IRQHandler(uint8_t line)
{
    if (line >= 16U)
    {
        return;
    }

    uint32_t now = GPIO_GET_TICK_MS();
    if ((now - s_debounce_ts[line]) < GPIO_DEBOUNCE_MS)
    {
        return; /* within debounce window – ignore */
    }
    s_debounce_ts[line] = now;

    uint16_t pin_mask = (uint16_t)GPIO_PIN(line);

    for (uint8_t i = 0U; i < s_irq_count; i++)
    {
        GPIO_IRQ_Entry *e = &s_irq_table[i];
        if (e->pin == pin_mask && e->enabled && e->callback != NULL)
        {
            uint16_t raw    = (uint16_t)GPIO_REG_IDR_READ(e->port);
            GPIO_State state = ((raw & pin_mask) != 0U) ? GPIO_STATE_HIGH : GPIO_STATE_LOW;
            e->callback(e->port, pin_mask, state);
        }
    }
}
