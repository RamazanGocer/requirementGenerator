/**
 * @file gpio.h
 * @brief GPIO (General Purpose Input/Output) driver interface for ARM Cortex-M targets.
 *
 * Provides a hardware-abstraction layer for configuring and controlling GPIO pins.
 * Supports input, output, alternate-function, and analog modes with optional
 * pull-up/pull-down resistors, output speed selection, and interrupt registration.
 *
 * @author  Embedded BSP Team
 * @version 1.2.0
 * @date    2024-01-15
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * Compile-time configuration
 * ========================================================= */

/** @brief Maximum number of IRQ callbacks that can be registered per pin. */
#define GPIO_MAX_CALLBACKS      16U

/** @brief Debounce filter time in milliseconds for interrupt inputs. */
#define GPIO_DEBOUNCE_MS        10U

/** @brief Bit mask for all 16 pins of a GPIO port. */
#define GPIO_PIN_ALL            0xFFFFU

/** @brief Helper macro – set a single pin bit from pin number (0-15). */
#define GPIO_PIN(n)             (1U << (n))

/** @brief Read the logical state of a bitmask pin after GPIO_Read(). */
#define GPIO_IS_SET(val, pin)   (((val) & (pin)) != 0U)

/* =========================================================
 * Enumerations
 * ========================================================= */

/**
 * @brief Available GPIO port identifiers.
 *
 * Maps to the physical port base addresses on the target MCU.
 * Port F and above are only present on devices with ≥144 pins.
 */
typedef enum
{
    GPIO_PORT_A = 0, /**< Port A – typically contains JTAG / SWD pins (PA13/14). */
    GPIO_PORT_B,     /**< Port B. */
    GPIO_PORT_C,     /**< Port C – contains HSE oscillator pins (PC14/15) on most devices. */
    GPIO_PORT_D,     /**< Port D. */
    GPIO_PORT_E,     /**< Port E. */
    GPIO_PORT_F,     /**< Port F – only on 144-pin packages. */
    GPIO_PORT_G,     /**< Port G – only on 144-pin packages. */
    GPIO_PORT_COUNT  /**< Sentinel – not a valid port. */
} GPIO_Port;

/**
 * @brief GPIO pin numbers within a port.
 *
 * Each port exposes 16 physical pins numbered 0–15.
 */
typedef enum
{
    GPIO_PIN_0  = GPIO_PIN(0),  /**< Pin 0.  */
    GPIO_PIN_1  = GPIO_PIN(1),  /**< Pin 1.  */
    GPIO_PIN_2  = GPIO_PIN(2),  /**< Pin 2.  */
    GPIO_PIN_3  = GPIO_PIN(3),  /**< Pin 3.  */
    GPIO_PIN_4  = GPIO_PIN(4),  /**< Pin 4.  */
    GPIO_PIN_5  = GPIO_PIN(5),  /**< Pin 5.  */
    GPIO_PIN_6  = GPIO_PIN(6),  /**< Pin 6.  */
    GPIO_PIN_7  = GPIO_PIN(7),  /**< Pin 7.  */
    GPIO_PIN_8  = GPIO_PIN(8),  /**< Pin 8.  */
    GPIO_PIN_9  = GPIO_PIN(9),  /**< Pin 9.  */
    GPIO_PIN_10 = GPIO_PIN(10), /**< Pin 10. */
    GPIO_PIN_11 = GPIO_PIN(11), /**< Pin 11. */
    GPIO_PIN_12 = GPIO_PIN(12), /**< Pin 12. */
    GPIO_PIN_13 = GPIO_PIN(13), /**< Pin 13. */
    GPIO_PIN_14 = GPIO_PIN(14), /**< Pin 14. */
    GPIO_PIN_15 = GPIO_PIN(15)  /**< Pin 15. */
} GPIO_PinMask;

/**
 * @brief GPIO pin operating modes.
 */
typedef enum
{
    GPIO_MODE_INPUT     = 0, /**< Floating or pulled digital input.            */
    GPIO_MODE_OUTPUT_PP,     /**< Push-pull digital output.                    */
    GPIO_MODE_OUTPUT_OD,     /**< Open-drain digital output (needs external pull-up). */
    GPIO_MODE_AF_PP,         /**< Alternate function push-pull (UART, SPI …).  */
    GPIO_MODE_AF_OD,         /**< Alternate function open-drain (I2C …).       */
    GPIO_MODE_ANALOG,        /**< Analog mode – ADC / DAC channel.             */
    GPIO_MODE_IT_RISING,     /**< Interrupt on rising edge.                    */
    GPIO_MODE_IT_FALLING,    /**< Interrupt on falling edge.                   */
    GPIO_MODE_IT_BOTH        /**< Interrupt on both edges.                     */
} GPIO_Mode;

/**
 * @brief Internal pull resistor configuration.
 */
typedef enum
{
    GPIO_PULL_NONE = 0, /**< No pull resistor – pin floats when undriven. */
    GPIO_PULL_UP,       /**< Internal pull-up resistor enabled (~40 kΩ).  */
    GPIO_PULL_DOWN      /**< Internal pull-down resistor enabled (~40 kΩ). */
} GPIO_Pull;

/**
 * @brief Output drive speed (slew rate) setting.
 *
 * Higher speeds increase EMI; use the lowest speed that meets timing.
 */
typedef enum
{
    GPIO_SPEED_LOW    = 0, /**< ~2 MHz  – lowest EMI, suited for static signals. */
    GPIO_SPEED_MEDIUM,     /**< ~10 MHz – general-purpose outputs.              */
    GPIO_SPEED_HIGH,       /**< ~50 MHz – fast bus signals.                     */
    GPIO_SPEED_VERY_HIGH   /**< ~100 MHz – only for SDIO / FMC / high-speed SPI. */
} GPIO_Speed;

/**
 * @brief Logical pin state.
 */
typedef enum
{
    GPIO_STATE_LOW  = 0, /**< Pin driven or read as logic 0. */
    GPIO_STATE_HIGH = 1  /**< Pin driven or read as logic 1. */
} GPIO_State;

/**
 * @brief Driver return codes.
 */
typedef enum
{
    GPIO_OK          =  0, /**< Operation completed successfully.      */
    GPIO_ERR_PARAM   = -1, /**< One or more parameters are invalid.    */
    GPIO_ERR_BUSY    = -2, /**< Peripheral is busy with another operation. */
    GPIO_ERR_TIMEOUT = -3, /**< Operation timed out.                   */
    GPIO_ERR_HW      = -4  /**< Hardware fault detected.               */
} GPIO_Status;

/* =========================================================
 * Typedefs & structures
 * ========================================================= */

/**
 * @brief Callback invoked from the GPIO EXTI interrupt handler.
 *
 * @param port  Port that generated the interrupt.
 * @param pin   Pin mask (single bit) that triggered the interrupt.
 * @param state Current pin state sampled inside the ISR.
 */
typedef void (*GPIO_IRQ_Callback)(GPIO_Port port, uint16_t pin, GPIO_State state);

/**
 * @brief Full configuration descriptor for a GPIO pin or pin group.
 *
 * Pass a pointer to an initialised instance of this struct to GPIO_Init().
 * All fields must be set; there are no hidden defaults.
 */
typedef struct
{
    GPIO_Port  port;    /**< Target port (GPIO_PORT_A … GPIO_PORT_G).          */
    uint16_t   pin;     /**< Pin mask – OR multiple GPIO_PINx values together. */
    GPIO_Mode  mode;    /**< Operating mode (input / output / AF / analog / IT). */
    GPIO_Pull  pull;    /**< Pull-up / pull-down / none.                        */
    GPIO_Speed speed;   /**< Output slew rate; ignored for input / analog.      */
    uint8_t    af;      /**< Alternate function number (0–15); 0 for non-AF.   */
} GPIO_Config;

/**
 * @brief IRQ registration entry for a single pin.
 *
 * Internal use – populated by GPIO_RegisterIRQ() and stored in the driver's
 * callback table.
 */
typedef struct
{
    GPIO_Port        port;     /**< Port this entry belongs to.         */
    uint16_t         pin;      /**< Pin mask this entry services.       */
    GPIO_IRQ_Callback callback; /**< Function pointer called on trigger. */
    bool             enabled;  /**< Whether the callback is active.     */
} GPIO_IRQ_Entry;

/* =========================================================
 * Public API
 * ========================================================= */

/**
 * @brief Initialise one or more GPIO pins according to the supplied config.
 *
 * Enables the port clock if not already running, applies the MODER, OTYPER,
 * OSPEEDR, and PUPDR register fields, and – for interrupt modes – configures
 * SYSCFG EXTICR and enables the corresponding NVIC line.
 *
 * @param[in] cfg  Pointer to a fully populated GPIO_Config structure.
 *                 Must not be NULL.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if @p cfg is NULL or contains out-of-range values.
 * @return GPIO_ERR_HW     if the port clock fails to start.
 */
GPIO_Status GPIO_Init(const GPIO_Config *cfg);

/**
 * @brief Reset a pin or pin group to the power-on default (input, no pull).
 *
 * Removes any registered IRQ callbacks for the affected pins and disables
 * the corresponding EXTI lines. Does NOT disable the port clock because
 * other pins on the same port may still be in use.
 *
 * @param[in] port  Port identifier.
 * @param[in] pin   Pin mask to de-initialise.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port or pin is out of range.
 */
GPIO_Status GPIO_DeInit(GPIO_Port port, uint16_t pin);

/**
 * @brief Drive an output pin or pin group to the requested state.
 *
 * Uses the atomic BSRR register so the operation is interrupt-safe without
 * requiring a critical section.
 *
 * @param[in] port   Port identifier.
 * @param[in] pin    Pin mask to write. All set bits are driven simultaneously.
 * @param[in] state  GPIO_STATE_HIGH or GPIO_STATE_LOW.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port, pin, or state is out of range.
 */
GPIO_Status GPIO_Write(GPIO_Port port, uint16_t pin, GPIO_State state);

/**
 * @brief Sample the current logical state of an input or output pin.
 *
 * Reads from the IDR (input data register) for input-mode pins, or from the
 * ODR for output-mode pins, and returns the subset of bits requested by @p pin.
 *
 * @param[in]  port    Port identifier.
 * @param[in]  pin     Pin mask to read.
 * @param[out] state   Pointer to a uint16_t that receives the masked pin states.
 *                     Each bit position corresponds to the matching GPIO_PIN_x.
 *                     Must not be NULL.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if any argument is invalid.
 */
GPIO_Status GPIO_Read(GPIO_Port port, uint16_t pin, uint16_t *state);

/**
 * @brief Toggle the output state of a pin or pin group.
 *
 * Performs a read-modify-write on ODR inside a critical section to ensure
 * atomicity on multi-core targets.
 *
 * @param[in] port  Port identifier.
 * @param[in] pin   Pin mask to toggle.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port or pin is out of range.
 */
GPIO_Status GPIO_Toggle(GPIO_Port port, uint16_t pin);

/**
 * @brief Change the operating mode of an already-initialised pin at runtime.
 *
 * Useful for bidirectional bus lines that must switch between output drive and
 * high-impedance input. The pin must have been initialised with GPIO_Init()
 * beforehand; this function does not re-apply pull or speed settings.
 *
 * @param[in] port  Port identifier.
 * @param[in] pin   Pin mask (single pin recommended; multi-pin allowed).
 * @param[in] mode  New operating mode.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if any argument is invalid.
 */
GPIO_Status GPIO_SetMode(GPIO_Port port, uint16_t pin, GPIO_Mode mode);

/**
 * @brief Register a callback to be invoked when a pin interrupt fires.
 *
 * The pin must have been configured with one of the GPIO_MODE_IT_* modes.
 * A maximum of GPIO_MAX_CALLBACKS entries can be registered at once across
 * all ports. Re-registering the same port+pin overwrites the existing entry.
 *
 * @param[in] port      Port identifier.
 * @param[in] pin       Single pin mask (exactly one bit must be set).
 * @param[in] callback  Function to call from the EXTI ISR. Must not be NULL.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port, pin (multiple bits set), or callback is invalid.
 * @return GPIO_ERR_BUSY   if the callback table is full.
 */
GPIO_Status GPIO_RegisterIRQ(GPIO_Port port, uint16_t pin, GPIO_IRQ_Callback callback);

/**
 * @brief Unregister a previously registered interrupt callback.
 *
 * If no callback is registered for the given port/pin the function returns
 * GPIO_OK without error.
 *
 * @param[in] port  Port identifier.
 * @param[in] pin   Single pin mask for which the callback should be removed.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port or pin is out of range.
 */
GPIO_Status GPIO_UnregisterIRQ(GPIO_Port port, uint16_t pin);

/**
 * @brief Enable or disable the EXTI interrupt line for a configured pin.
 *
 * @param[in] port    Port identifier.
 * @param[in] pin     Single pin mask.
 * @param[in] enable  true to enable the interrupt, false to mask it.
 *
 * @return GPIO_OK         on success.
 * @return GPIO_ERR_PARAM  if port or pin is out of range.
 */
GPIO_Status GPIO_SetIRQEnabled(GPIO_Port port, uint16_t pin, bool enable);

/**
 * @brief Internal EXTI interrupt service routine dispatcher.
 *
 * Called by the MCU vector table handlers (EXTI0_IRQHandler … EXTI15_10_IRQHandler).
 * Clears the pending bit, applies debounce filtering, and dispatches registered
 * callbacks. Do not call this function directly from application code.
 *
 * @param[in] line  EXTI line number (0–15) that fired.
 */
void GPIO_EXTI_IRQHandler(uint8_t line);

#endif /* GPIO_H */
