/**
 * @file spi.h
 * @brief SPI (Serial Peripheral Interface) master driver interface.
 *
 * Provides full-duplex and half-duplex SPI master operations with support
 * for blocking, interrupt-driven, and DMA transfer modes. Chip-select
 * management is handled by the caller via the GPIO driver or by configuring
 * the NSS hardware signal.
 *
 * @author  Embedded BSP Team
 * @version 1.1.0
 * @date    2024-02-20
 */

#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================
 * Compile-time configuration
 * ========================================================= */

/** @brief Maximum number of SPI peripheral instances on the target. */
#define SPI_INSTANCE_COUNT      3U

/** @brief Default blocking transfer timeout in milliseconds. */
#define SPI_TIMEOUT_DEFAULT_MS  100U

/** @brief Maximum clock prescaler supported by the peripheral (PCLK / 256). */
#define SPI_PRESCALER_MAX       256U

/* =========================================================
 * Enumerations
 * ========================================================= */

/**
 * @brief SPI peripheral instance selector.
 */
typedef enum
{
    SPI_INSTANCE_1 = 0, /**< SPI1 – high-speed, connected to APB2. */
    SPI_INSTANCE_2,     /**< SPI2 – connected to APB1.              */
    SPI_INSTANCE_3,     /**< SPI3 – connected to APB1.              */
} SPI_Instance;

/**
 * @brief SPI clock polarity (CPOL).
 *
 * Defines the idle state of the clock line between transfers.
 */
typedef enum
{
    SPI_CPOL_LOW  = 0, /**< Clock idles low  (CPOL = 0). */
    SPI_CPOL_HIGH = 1  /**< Clock idles high (CPOL = 1). */
} SPI_ClockPolarity;

/**
 * @brief SPI clock phase (CPHA).
 *
 * Defines on which clock edge data is sampled.
 */
typedef enum
{
    SPI_CPHA_1EDGE = 0, /**< Data sampled on the first  clock edge (CPHA = 0). */
    SPI_CPHA_2EDGE = 1  /**< Data sampled on the second clock edge (CPHA = 1). */
} SPI_ClockPhase;

/**
 * @brief Combined SPI mode (CPOL + CPHA).
 *
 * The four standard SPI modes derived from polarity and phase combinations.
 */
typedef enum
{
    SPI_MODE_0 = 0, /**< CPOL=0, CPHA=0 – most common; idles low, sample on rising edge.  */
    SPI_MODE_1,     /**< CPOL=0, CPHA=1 – idles low, sample on falling edge.               */
    SPI_MODE_2,     /**< CPOL=1, CPHA=0 – idles high, sample on falling edge.              */
    SPI_MODE_3      /**< CPOL=1, CPHA=1 – idles high, sample on rising edge.               */
} SPI_Mode;

/**
 * @brief Data frame size.
 */
typedef enum
{
    SPI_DATASIZE_8BIT  = 8,  /**< 8-bit frames  – standard byte-oriented transfers. */
    SPI_DATASIZE_16BIT = 16  /**< 16-bit frames – DAC / ADC word transfers.          */
} SPI_DataSize;

/**
 * @brief Bit transmission order.
 */
typedef enum
{
    SPI_BITORDER_MSB = 0, /**< Most-significant bit transmitted first (standard). */
    SPI_BITORDER_LSB      /**< Least-significant bit transmitted first.            */
} SPI_BitOrder;

/**
 * @brief NSS (chip-select) management strategy.
 */
typedef enum
{
    SPI_NSS_SOFT = 0, /**< NSS managed in software; caller drives a GPIO CS pin.    */
    SPI_NSS_HARD_IN,  /**< NSS input drives the peripheral (multi-master).           */
    SPI_NSS_HARD_OUT  /**< NSS output automatically asserted during transfers.        */
} SPI_NSSMode;

/**
 * @brief Status flags – can be OR'd together.
 */
typedef enum
{
    SPI_STATUS_OK       = 0x00U, /**< No error; peripheral idle.          */
    SPI_STATUS_TX_BUSY  = 0x01U, /**< Transmit in progress.               */
    SPI_STATUS_RX_BUSY  = 0x02U, /**< Receive in progress.                */
    SPI_STATUS_OVERRUN  = 0x04U, /**< Receive overrun detected.           */
    SPI_STATUS_MODE_ERR = 0x08U, /**< Mode fault (multi-master conflict). */
    SPI_STATUS_CRC_ERR  = 0x10U, /**< CRC mismatch detected.              */
    SPI_STATUS_TIMEOUT  = 0x20U  /**< Transfer timed out.                 */
} SPI_StatusFlag;

/**
 * @brief Driver return codes.
 */
typedef enum
{
    SPI_OK          =  0, /**< Operation succeeded.                         */
    SPI_ERR_PARAM   = -1, /**< Invalid parameter.                           */
    SPI_ERR_BUSY    = -2, /**< Peripheral busy with another transfer.       */
    SPI_ERR_TIMEOUT = -3, /**< Transfer timed out.                          */
    SPI_ERR_OVERRUN = -4, /**< Receive overrun during full-duplex transfer. */
    SPI_ERR_CRC     = -5, /**< CRC verification failed.                     */
    SPI_ERR_HW      = -6  /**< Unrecoverable hardware error.                */
} SPI_Status;

/* =========================================================
 * Callback typedefs
 * ========================================================= */

/**
 * @brief Callback invoked when a non-blocking transfer (IT or DMA) completes.
 *
 * @param instance    SPI peripheral instance that completed the transfer.
 * @param tx_data     Pointer to the transmit buffer supplied by the caller.
 * @param rx_data     Pointer to the receive  buffer supplied by the caller.
 * @param len         Number of data frames actually transferred.
 */
typedef void (*SPI_TransferCallback)(SPI_Instance instance,
                                     const uint8_t *tx_data,
                                     uint8_t *rx_data,
                                     size_t len);

/**
 * @brief Callback invoked when the driver detects an error condition.
 *
 * @param instance  SPI peripheral instance where the error occurred.
 * @param flags     Bitwise OR of SPI_StatusFlag values describing the error.
 */
typedef void (*SPI_ErrorCallback)(SPI_Instance instance, uint32_t flags);

/* =========================================================
 * Configuration & handle structures
 * ========================================================= */

/**
 * @brief SPI peripheral configuration snapshot.
 *
 * Pass a pointer to an initialised instance to SPI_Init().
 * All fields are copied into the handle and must not be modified afterwards.
 */
typedef struct
{
    SPI_Instance   instance;      /**< Peripheral to configure.                         */
    SPI_Mode       mode;          /**< SPI mode (0–3; encodes CPOL + CPHA).             */
    SPI_DataSize   data_size;     /**< Frame size (8- or 16-bit).                        */
    SPI_BitOrder   bit_order;     /**< MSB-first or LSB-first.                           */
    SPI_NSSMode    nss_mode;      /**< Software or hardware CS management.               */
    uint32_t       clock_hz;      /**< Desired SCK frequency in Hz; driver selects the  */
                                  /**  closest achievable prescaler that does not exceed */
                                  /**  this value.                                       */
    bool           crc_enable;    /**< Enable hardware CRC generation and checking.      */
    uint16_t       crc_poly;      /**< CRC polynomial; ignored if crc_enable is false.  */
    SPI_TransferCallback xfer_cb; /**< Transfer-complete callback (may be NULL).        */
    SPI_ErrorCallback    error_cb;/**< Error callback (may be NULL).                    */
} SPI_Config;

/**
 * @brief Runtime handle for a single SPI peripheral instance.
 *
 * Allocate statically or on the heap. Pass a pointer to all SPI API calls.
 * Treat all fields as read-only from application code.
 */
typedef struct
{
    SPI_Config config;        /**< Configuration snapshot.                       */
    uint32_t   status_flags;  /**< Current status (bitwise OR of SPI_StatusFlag). */
    uint32_t   actual_clk_hz; /**< Achieved SCK frequency after prescaler selection. */
    bool       initialized;  /**< True after a successful SPI_Init() call.       */
} SPI_Handle;

/* =========================================================
 * Public API
 * ========================================================= */

/**
 * @brief Initialise an SPI peripheral and populate a runtime handle.
 *
 * Enables the peripheral clock, selects the baud-rate prescaler closest to
 * @p cfg->clock_hz without exceeding it, programs CPOL/CPHA, data width,
 * bit order, and NSS mode. If @p cfg->crc_enable is set, the CRC polynomial
 * is written to the CRCPR register.
 *
 * @param[out] handle  Caller-allocated SPI_Handle to populate. Must not be NULL.
 * @param[in]  cfg     Desired configuration. Must not be NULL.
 *
 * @return SPI_OK         on success.
 * @return SPI_ERR_PARAM  if either pointer is NULL or cfg has invalid values.
 * @return SPI_ERR_HW     if the peripheral clock cannot be enabled.
 */
SPI_Status SPI_Init(SPI_Handle *handle, const SPI_Config *cfg);

/**
 * @brief Disable the SPI peripheral and invalidate the handle.
 *
 * Waits for any active transfer to drain, then turns off the peripheral
 * clock and zeroes the handle. The handle must be re-initialised before
 * further use.
 *
 * @param[in,out] handle  Handle from a previous successful SPI_Init().
 *
 * @return SPI_OK          on success.
 * @return SPI_ERR_PARAM   if @p handle is NULL or not initialised.
 * @return SPI_ERR_TIMEOUT if the active transfer did not finish.
 */
SPI_Status SPI_DeInit(SPI_Handle *handle);

/**
 * @brief Transmit @p len bytes of data in blocking mode.
 *
 * Sends every byte from @p tx_data and discards the MISO bytes received
 * during the transfer. For full-duplex capture use SPI_TransmitReceive().
 *
 * @param[in] handle      Initialised SPI handle.
 * @param[in] tx_data     Transmit buffer. Must not be NULL.
 * @param[in] len         Number of frames (bytes or 16-bit words) to send. Must be > 0.
 * @param[in] timeout_ms  Maximum time to wait in milliseconds.
 *
 * @return SPI_OK          on success.
 * @return SPI_ERR_PARAM   if any argument is invalid.
 * @return SPI_ERR_BUSY    if a transfer is already in progress.
 * @return SPI_ERR_TIMEOUT if the TX FIFO did not drain within @p timeout_ms.
 */
SPI_Status SPI_Transmit(SPI_Handle *handle,
                         const uint8_t *tx_data,
                         size_t len,
                         uint32_t timeout_ms);

/**
 * @brief Receive @p len bytes of data in blocking mode.
 *
 * Clocks out dummy bytes (0xFF) on MOSI to generate the SPI clock and stores
 * the corresponding MISO data in @p rx_buf.
 *
 * @param[in]  handle      Initialised SPI handle.
 * @param[out] rx_buf      Receive buffer. Must not be NULL.
 * @param[in]  len         Number of frames to receive. Must be > 0.
 * @param[in]  timeout_ms  Maximum time to wait in milliseconds.
 *
 * @return SPI_OK          on success.
 * @return SPI_ERR_PARAM   if any argument is invalid.
 * @return SPI_ERR_BUSY    if a transfer is already in progress.
 * @return SPI_ERR_TIMEOUT if the RX FIFO did not fill within @p timeout_ms.
 * @return SPI_ERR_OVERRUN if the hardware receive buffer overflowed.
 */
SPI_Status SPI_Receive(SPI_Handle *handle,
                        uint8_t *rx_buf,
                        size_t len,
                        uint32_t timeout_ms);

/**
 * @brief Perform a simultaneous full-duplex transmit and receive.
 *
 * Sends @p len frames from @p tx_data while capturing the same number of
 * frames into @p rx_buf. @p tx_data and @p rx_buf may point to the same
 * buffer (in-place operation).
 *
 * @param[in]  handle      Initialised SPI handle.
 * @param[in]  tx_data     Transmit buffer. Must not be NULL.
 * @param[out] rx_buf      Receive buffer. Must not be NULL.
 * @param[in]  len         Number of frames. Must be > 0.
 * @param[in]  timeout_ms  Maximum time to wait in milliseconds.
 *
 * @return SPI_OK          on success.
 * @return SPI_ERR_PARAM   if any argument is invalid.
 * @return SPI_ERR_BUSY    if a transfer is already in progress.
 * @return SPI_ERR_TIMEOUT if the transfer timed out.
 * @return SPI_ERR_OVERRUN on a receive overrun during the transfer.
 * @return SPI_ERR_CRC     if CRC checking is enabled and the CRC mismatches.
 */
SPI_Status SPI_TransmitReceive(SPI_Handle *handle,
                                const uint8_t *tx_data,
                                uint8_t *rx_buf,
                                size_t len,
                                uint32_t timeout_ms);

/**
 * @brief Start a non-blocking interrupt-driven full-duplex transfer.
 *
 * Returns immediately after enabling the SPI interrupts. The registered
 * SPI_TransferCallback is called from the SPI ISR when the last frame
 * has been shifted out / in.
 *
 * @param[in]  handle   Initialised SPI handle.
 * @param[in]  tx_data  Transmit buffer; must remain valid until callback fires.
 *                      Pass NULL to clock out dummy bytes.
 * @param[out] rx_buf   Receive buffer; must remain valid until callback fires.
 *                      Pass NULL to discard incoming data.
 * @param[in]  len      Number of frames. Must be > 0.
 *
 * @return SPI_OK         on success.
 * @return SPI_ERR_PARAM  if @p handle is NULL or @p len is 0.
 * @return SPI_ERR_BUSY   if a transfer is already in progress.
 */
SPI_Status SPI_TransferIT(SPI_Handle *handle,
                           const uint8_t *tx_data,
                           uint8_t *rx_buf,
                           size_t len);

/**
 * @brief Abort an in-progress non-blocking transfer.
 *
 * Disables SPI interrupts / DMA requests, de-asserts NSS (if hardware mode),
 * and resets the handle status. No callback is fired.
 *
 * @param[in,out] handle  Initialised SPI handle.
 *
 * @return SPI_OK         on success.
 * @return SPI_ERR_PARAM  if @p handle is NULL or not initialised.
 */
SPI_Status SPI_AbortTransfer(SPI_Handle *handle);

/**
 * @brief Query the current peripheral status flags.
 *
 * @param[in]  handle  Initialised SPI handle.
 * @param[out] flags   Receives bitwise OR of SPI_StatusFlag values.
 *                     Must not be NULL.
 *
 * @return SPI_OK         on success.
 * @return SPI_ERR_PARAM  if either argument is invalid.
 */
SPI_Status SPI_GetStatus(const SPI_Handle *handle, uint32_t *flags);

/**
 * @brief Return the actual SCK frequency achieved after prescaler selection.
 *
 * The returned value may be less than the requested @p clock_hz because
 * only power-of-two prescalers are available.
 *
 * @param[in] handle  Initialised SPI handle.
 *
 * @return Actual SCK frequency in Hz, or 0 if the handle is not initialised.
 */
uint32_t SPI_GetActualClock(const SPI_Handle *handle);

/**
 * @brief Internal SPI interrupt service routine handler.
 *
 * Called from the MCU vector (SPI1_IRQHandler, SPI2_IRQHandler, etc.).
 * Handles TXE, RXNE, OVR, MODF, and CRCERR flags, drives the transfer
 * state machine, and fires registered callbacks. Do not call from
 * application code.
 *
 * @param[in,out] handle  Handle for the SPI peripheral that raised the interrupt.
 */
void SPI_IRQHandler(SPI_Handle *handle);

#endif /* SPI_H */
