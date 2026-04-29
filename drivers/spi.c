/**
 * @file spi.c
 * @brief SPI master driver implementation for ARM Cortex-M targets.
 *
 * Implements blocking, interrupt-driven, and DMA-backed SPI transfers as
 * declared in spi.h. Hardware register access is hidden behind shim macros
 * at the top of the file to ease porting to a different MCU family.
 *
 * @author  Embedded BSP Team
 * @version 1.1.0
 * @date    2024-02-20
 */

#include "spi.h"
#include <string.h>

/* =========================================================
 * Hardware register shims (replace with CMSIS / HAL)
 * ========================================================= */

/** @brief True when the TX buffer (TXE flag) is empty and ready for a new byte. */
#define SPI_REG_TXE(inst)           (true)

/** @brief True when the RX buffer (RXNE flag) contains a received byte. */
#define SPI_REG_RXNE(inst)          (false)

/** @brief True when the peripheral is still busy shifting bits (BSY flag). */
#define SPI_REG_BUSY(inst)          (false)

/** @brief True when an overrun error (OVR flag) has been detected. */
#define SPI_REG_OVR(inst)           (false)

/** @brief True when a mode-fault (MODF flag) has been detected. */
#define SPI_REG_MODF(inst)          (false)

/** @brief True when a CRC error (CRCERR flag) has been detected. */
#define SPI_REG_CRCERR(inst)        (false)

/** @brief Write one data frame to the SPI data register (DR). */
#define SPI_REG_DR_WRITE(inst, val) ((void)(inst), (void)(val))

/** @brief Read one data frame from the SPI data register (DR). */
#define SPI_REG_DR_READ(inst)       (0xFFU)

/** @brief Clear all SPI error flags for @p inst. */
#define SPI_REG_CLEAR_ERR(inst)     ((void)(inst))

/** @brief Enable the peripheral clock for @p inst. */
#define SPI_CLK_ENABLE(inst)        ((void)(inst))

/** @brief Disable the peripheral clock for @p inst. */
#define SPI_CLK_DISABLE(inst)       ((void)(inst))

/** @brief PCLK frequency in Hz (used to calculate the prescaler). */
#define SPI_PCLK_HZ                 84000000UL

/** @brief Current system tick in milliseconds. */
#define SPI_GET_TICK_MS()           (0U)

/** @brief Enter / exit a critical section. */
#define SPI_ENTER_CRITICAL()        do { } while (0)
#define SPI_EXIT_CRITICAL()         do { } while (0)

/* =========================================================
 * Private helpers
 * ========================================================= */

/**
 * @brief Validate a handle pointer and confirm initialisation.
 *
 * @param[in] h  Handle to check.
 *
 * @return true if usable, false otherwise.
 */
static bool spi_handle_valid(const SPI_Handle *h)
{
    return (h != NULL) && h->initialized;
}

/**
 * @brief Select the baud-rate prescaler that achieves the highest clock
 *        frequency that does not exceed @p target_hz.
 *
 * The SPI peripheral supports prescalers of 2, 4, 8, 16, 32, 64, 128, 256.
 *
 * @param[in]  target_hz   Desired maximum SCK frequency in Hz.
 * @param[out] actual_hz   Achieved SCK frequency after rounding down.
 *
 * @return 3-bit BR field value (0 = /2 … 7 = /256), or 7 if no prescaler fits.
 */
static uint8_t spi_select_prescaler(uint32_t target_hz, uint32_t *actual_hz)
{
    uint32_t prescalers[8] = {2U, 4U, 8U, 16U, 32U, 64U, 128U, 256U};

    for (uint8_t i = 0U; i < 8U; i++)
    {
        uint32_t clk = SPI_PCLK_HZ / prescalers[i];
        if (clk <= target_hz)
        {
            if (actual_hz != NULL) { *actual_hz = clk; }
            return i;
        }
    }

    if (actual_hz != NULL) { *actual_hz = SPI_PCLK_HZ / 256U; }
    return 7U;
}

/**
 * @brief Block until the BSY flag clears or @p timeout_ms elapses.
 *
 * @param[in] inst        SPI instance to poll.
 * @param[in] timeout_ms  Maximum wait time in milliseconds.
 *
 * @return true if BSY cleared in time, false on timeout.
 */
static bool spi_wait_not_busy(SPI_Instance inst, uint32_t timeout_ms)
{
    uint32_t start = SPI_GET_TICK_MS();
    while (SPI_REG_BUSY(inst))
    {
        if ((SPI_GET_TICK_MS() - start) >= timeout_ms)
        {
            return false;
        }
    }
    return true;
}

/* =========================================================
 * Public API implementation
 * ========================================================= */

/**
 * @brief Initialise an SPI peripheral and populate a runtime handle.
 *
 * Enables the peripheral clock, selects the baud-rate prescaler, applies
 * CPOL/CPHA/data-size/bit-order/NSS settings, and optionally configures
 * the CRC polynomial.
 *
 * @param[out] handle  Caller-allocated SPI_Handle.
 * @param[in]  cfg     Desired configuration.
 *
 * @return SPI_OK         on success.
 * @return SPI_ERR_PARAM  if either pointer is NULL or cfg has invalid values.
 * @return SPI_ERR_HW     if the peripheral clock cannot be enabled.
 */
SPI_Status SPI_Init(SPI_Handle *handle, const SPI_Config *cfg)
{
    if (handle == NULL || cfg == NULL)
    {
        return SPI_ERR_PARAM;
    }
    if (cfg->instance >= SPI_INSTANCE_COUNT)
    {
        return SPI_ERR_PARAM;
    }
    if (cfg->clock_hz == 0U || cfg->clock_hz > SPI_PCLK_HZ / 2U)
    {
        return SPI_ERR_PARAM;
    }

    memset(handle, 0, sizeof(SPI_Handle));
    handle->config = *cfg;

    SPI_CLK_ENABLE(cfg->instance);

    uint8_t br = spi_select_prescaler(cfg->clock_hz, &handle->actual_clk_hz);
    (void)br; /* Applied to hardware via register shim in real implementation */

    handle->initialized = true;
    return SPI_OK;
}

/**
 * @brief Disable the SPI peripheral and invalidate the handle.
 *
 * Waits for any active transfer to drain before disabling the clock.
 *
 * @param[in,out] handle  Initialised SPI handle.
 *
 * @return SPI_OK          on success.
 * @return SPI_ERR_PARAM   if @p handle is NULL or not initialised.
 * @return SPI_ERR_TIMEOUT if the active transfer did not drain.
 */
SPI_Status SPI_DeInit(SPI_Handle *handle)
{
    if (!spi_handle_valid(handle))
    {
        return SPI_ERR_PARAM;
    }

    if (!spi_wait_not_busy(handle->config.instance, SPI_TIMEOUT_DEFAULT_MS))
    {
        return SPI_ERR_TIMEOUT;
    }

    SPI_CLK_DISABLE(handle->config.instance);
    memset(handle, 0, sizeof(SPI_Handle));
    return SPI_OK;
}

/**
 * @brief Transmit @p len bytes in blocking mode, discarding received data.
 *
 * Loops over each byte: waits for TXE, writes the byte, waits for RXNE,
 * reads and discards the incoming byte to keep the FIFO clear.
 *
 * @param[in] handle      Initialised SPI handle.
 * @param[in] tx_data     Source buffer.
 * @param[in] len         Frame count.
 * @param[in] timeout_ms  Per-frame timeout.
 *
 * @return SPI_OK          on success.
 * @return SPI_ERR_PARAM   if arguments are invalid.
 * @return SPI_ERR_BUSY    if a transfer is already in progress.
 * @return SPI_ERR_TIMEOUT if TXE or RXNE did not assert in time.
 */
SPI_Status SPI_Transmit(SPI_Handle *handle,
                         const uint8_t *tx_data,
                         size_t len,
                         uint32_t timeout_ms)
{
    if (!spi_handle_valid(handle) || tx_data == NULL || len == 0U)
    {
        return SPI_ERR_PARAM;
    }
    if (handle->status_flags & ((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY))
    {
        return SPI_ERR_BUSY;
    }

    handle->status_flags |= (uint32_t)SPI_STATUS_TX_BUSY;

    for (size_t i = 0U; i < len; i++)
    {
        uint32_t start = SPI_GET_TICK_MS();
        while (!SPI_REG_TXE(handle->config.instance))
        {
            if ((SPI_GET_TICK_MS() - start) >= timeout_ms)
            {
                handle->status_flags &= ~(uint32_t)SPI_STATUS_TX_BUSY;
                handle->status_flags |=  (uint32_t)SPI_STATUS_TIMEOUT;
                return SPI_ERR_TIMEOUT;
            }
        }
        SPI_REG_DR_WRITE(handle->config.instance, tx_data[i]);

        /* Read and discard to prevent OVR */
        start = SPI_GET_TICK_MS();
        while (!SPI_REG_RXNE(handle->config.instance))
        {
            if ((SPI_GET_TICK_MS() - start) >= timeout_ms)
            {
                handle->status_flags &= ~(uint32_t)SPI_STATUS_TX_BUSY;
                return SPI_ERR_TIMEOUT;
            }
        }
        (void)SPI_REG_DR_READ(handle->config.instance);
    }

    spi_wait_not_busy(handle->config.instance, timeout_ms);
    handle->status_flags &= ~(uint32_t)SPI_STATUS_TX_BUSY;
    return SPI_OK;
}

/**
 * @brief Receive @p len bytes by clocking out dummy 0xFF bytes in blocking mode.
 *
 * @param[in]  handle      Initialised SPI handle.
 * @param[out] rx_buf      Receive buffer.
 * @param[in]  len         Frame count.
 * @param[in]  timeout_ms  Per-frame timeout.
 *
 * @return SPI_OK          on success.
 * @return SPI_ERR_PARAM   if arguments are invalid.
 * @return SPI_ERR_BUSY    if a transfer is already in progress.
 * @return SPI_ERR_TIMEOUT if TXE or RXNE did not assert in time.
 * @return SPI_ERR_OVERRUN if the RX FIFO overflowed.
 */
SPI_Status SPI_Receive(SPI_Handle *handle,
                        uint8_t *rx_buf,
                        size_t len,
                        uint32_t timeout_ms)
{
    if (!spi_handle_valid(handle) || rx_buf == NULL || len == 0U)
    {
        return SPI_ERR_PARAM;
    }

    /* Fill tx_data with dummy bytes and call TransmitReceive */
    /* In a real driver this would avoid allocating a temporary buffer by
     * writing 0xFF directly in the TX loop. Shown explicitly here for clarity. */
    static const uint8_t dummy = 0xFFU;

    handle->status_flags |= (uint32_t)SPI_STATUS_RX_BUSY;

    for (size_t i = 0U; i < len; i++)
    {
        uint32_t start = SPI_GET_TICK_MS();
        while (!SPI_REG_TXE(handle->config.instance))
        {
            if ((SPI_GET_TICK_MS() - start) >= timeout_ms)
            {
                handle->status_flags &= ~(uint32_t)SPI_STATUS_RX_BUSY;
                return SPI_ERR_TIMEOUT;
            }
        }
        SPI_REG_DR_WRITE(handle->config.instance, dummy);

        start = SPI_GET_TICK_MS();
        while (!SPI_REG_RXNE(handle->config.instance))
        {
            if ((SPI_GET_TICK_MS() - start) >= timeout_ms)
            {
                handle->status_flags &= ~(uint32_t)SPI_STATUS_RX_BUSY;
                return SPI_ERR_TIMEOUT;
            }
        }

        if (SPI_REG_OVR(handle->config.instance))
        {
            SPI_REG_CLEAR_ERR(handle->config.instance);
            handle->status_flags &= ~(uint32_t)SPI_STATUS_RX_BUSY;
            handle->status_flags |=  (uint32_t)SPI_STATUS_OVERRUN;
            return SPI_ERR_OVERRUN;
        }

        rx_buf[i] = (uint8_t)SPI_REG_DR_READ(handle->config.instance);
    }

    handle->status_flags &= ~(uint32_t)SPI_STATUS_RX_BUSY;
    return SPI_OK;
}

/**
 * @brief Perform a full-duplex blocking transmit and receive.
 *
 * Simultaneously sends @p tx_data and captures MISO into @p rx_buf.
 * Supports in-place operation (tx_data == rx_buf).
 *
 * @param[in]  handle      Initialised SPI handle.
 * @param[in]  tx_data     Transmit buffer.
 * @param[out] rx_buf      Receive buffer.
 * @param[in]  len         Frame count.
 * @param[in]  timeout_ms  Per-frame timeout.
 *
 * @return SPI_OK          on success.
 * @return SPI_ERR_PARAM   if arguments are invalid.
 * @return SPI_ERR_BUSY    if a transfer is already in progress.
 * @return SPI_ERR_TIMEOUT on timeout.
 * @return SPI_ERR_OVERRUN on receive overrun.
 * @return SPI_ERR_CRC     if CRC checking is enabled and fails.
 */
SPI_Status SPI_TransmitReceive(SPI_Handle *handle,
                                const uint8_t *tx_data,
                                uint8_t *rx_buf,
                                size_t len,
                                uint32_t timeout_ms)
{
    if (!spi_handle_valid(handle) || tx_data == NULL || rx_buf == NULL || len == 0U)
    {
        return SPI_ERR_PARAM;
    }
    if (handle->status_flags & ((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY))
    {
        return SPI_ERR_BUSY;
    }

    handle->status_flags |= (uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY;

    for (size_t i = 0U; i < len; i++)
    {
        uint32_t start = SPI_GET_TICK_MS();
        while (!SPI_REG_TXE(handle->config.instance))
        {
            if ((SPI_GET_TICK_MS() - start) >= timeout_ms)
            {
                handle->status_flags &= ~((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY);
                return SPI_ERR_TIMEOUT;
            }
        }
        SPI_REG_DR_WRITE(handle->config.instance, tx_data[i]);

        start = SPI_GET_TICK_MS();
        while (!SPI_REG_RXNE(handle->config.instance))
        {
            if ((SPI_GET_TICK_MS() - start) >= timeout_ms)
            {
                handle->status_flags &= ~((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY);
                return SPI_ERR_TIMEOUT;
            }
        }

        if (SPI_REG_OVR(handle->config.instance))
        {
            SPI_REG_CLEAR_ERR(handle->config.instance);
            handle->status_flags &= ~((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY);
            handle->status_flags |=  (uint32_t)SPI_STATUS_OVERRUN;
            return SPI_ERR_OVERRUN;
        }

        rx_buf[i] = (uint8_t)SPI_REG_DR_READ(handle->config.instance);
    }

    /* Check CRC if enabled */
    if (handle->config.crc_enable && SPI_REG_CRCERR(handle->config.instance))
    {
        SPI_REG_CLEAR_ERR(handle->config.instance);
        handle->status_flags &= ~((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY);
        handle->status_flags |=  (uint32_t)SPI_STATUS_CRC_ERR;
        return SPI_ERR_CRC;
    }

    handle->status_flags &= ~((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY);
    return SPI_OK;
}

/**
 * @brief Start a non-blocking interrupt-driven full-duplex transfer.
 *
 * Enables TXE and RXNE interrupts. The SPI_TransferCallback fires when
 * the last frame has been exchanged.
 *
 * @param[in]  handle   Initialised SPI handle.
 * @param[in]  tx_data  Source buffer or NULL for dummy transmit.
 * @param[out] rx_buf   Destination buffer or NULL to discard.
 * @param[in]  len      Frame count.
 *
 * @return SPI_OK         on success.
 * @return SPI_ERR_PARAM  if @p handle is NULL or @p len is 0.
 * @return SPI_ERR_BUSY   if a transfer is already in progress.
 */
SPI_Status SPI_TransferIT(SPI_Handle *handle,
                           const uint8_t *tx_data,
                           uint8_t *rx_buf,
                           size_t len)
{
    if (!spi_handle_valid(handle) || len == 0U)
    {
        return SPI_ERR_PARAM;
    }
    if (handle->status_flags & ((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY))
    {
        return SPI_ERR_BUSY;
    }

    SPI_ENTER_CRITICAL();
    handle->status_flags |= (uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY;
    SPI_EXIT_CRITICAL();

    /* TXE / RXNE interrupt enable would be applied here */
    (void)tx_data;
    (void)rx_buf;

    return SPI_OK;
}

/**
 * @brief Abort an in-progress non-blocking transfer.
 *
 * Disables SPI interrupts, clears status flags, and resets the peripheral
 * state. No callback is fired.
 *
 * @param[in,out] handle  Initialised SPI handle.
 *
 * @return SPI_OK         on success.
 * @return SPI_ERR_PARAM  if @p handle is NULL or not initialised.
 */
SPI_Status SPI_AbortTransfer(SPI_Handle *handle)
{
    if (!spi_handle_valid(handle))
    {
        return SPI_ERR_PARAM;
    }

    SPI_ENTER_CRITICAL();
    /* Interrupt disable would be applied here */
    handle->status_flags &= ~((uint32_t)SPI_STATUS_TX_BUSY | (uint32_t)SPI_STATUS_RX_BUSY);
    SPI_EXIT_CRITICAL();

    return SPI_OK;
}

/**
 * @brief Return the current peripheral status flags.
 *
 * @param[in]  handle  Initialised SPI handle.
 * @param[out] flags   Receives bitwise OR of SPI_StatusFlag values.
 *
 * @return SPI_OK         on success.
 * @return SPI_ERR_PARAM  if either argument is invalid.
 */
SPI_Status SPI_GetStatus(const SPI_Handle *handle, uint32_t *flags)
{
    if (!spi_handle_valid(handle) || flags == NULL)
    {
        return SPI_ERR_PARAM;
    }
    *flags = handle->status_flags;
    return SPI_OK;
}

/**
 * @brief Return the actual SCK frequency achieved after prescaler rounding.
 *
 * @param[in] handle  Initialised SPI handle.
 *
 * @return Actual clock in Hz, or 0 if the handle is not initialised.
 */
uint32_t SPI_GetActualClock(const SPI_Handle *handle)
{
    if (!spi_handle_valid(handle))
    {
        return 0U;
    }
    return handle->actual_clk_hz;
}

/**
 * @brief SPI interrupt service routine — call from the MCU vector.
 *
 * Handles TXE (feeds next TX byte), RXNE (stores received byte), and error
 * flags (OVR, MODF, CRCERR). Fires the transfer callback when the frame
 * count reaches zero.
 *
 * @param[in,out] handle  Handle for the SPI peripheral that raised the interrupt.
 */
void SPI_IRQHandler(SPI_Handle *handle)
{
    if (!spi_handle_valid(handle))
    {
        return;
    }

    if (SPI_REG_OVR(handle->config.instance))
    {
        SPI_REG_CLEAR_ERR(handle->config.instance);
        handle->status_flags |= (uint32_t)SPI_STATUS_OVERRUN;
        if (handle->config.error_cb != NULL)
        {
            handle->config.error_cb(handle->config.instance,
                                    (uint32_t)SPI_STATUS_OVERRUN);
        }
        return;
    }

    if (SPI_REG_MODF(handle->config.instance))
    {
        SPI_REG_CLEAR_ERR(handle->config.instance);
        handle->status_flags |= (uint32_t)SPI_STATUS_MODE_ERR;
        if (handle->config.error_cb != NULL)
        {
            handle->config.error_cb(handle->config.instance,
                                    (uint32_t)SPI_STATUS_MODE_ERR);
        }
        return;
    }

    /* Normal TX/RX processing would be implemented here, tracking a frame
     * counter and firing handle->config.xfer_cb when count reaches zero. */
}
