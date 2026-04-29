/**
 * @file uart.c
 * @brief UART driver implementation for ARM Cortex-M targets.
 *
 * Implements blocking, interrupt-driven, and ring-buffer-backed UART transfers
 * as declared in uart.h. Hardware register access is isolated behind shim
 * macros at the top of this file to ease porting.
 *
 * @author  Embedded BSP Team
 * @version 2.0.1
 * @date    2024-03-10
 */

#include "uart.h"
#include <string.h>

/* =========================================================
 * Hardware register shims (replace with real CMSIS / HAL)
 * ========================================================= */

/** @brief True if the Transmit Data Register is empty. */
#define UART_REG_TXE(inst)          (true)

/** @brief True if the Receive Data Register is not empty. */
#define UART_REG_RXNE(inst)         (false)

/** @brief True if the Transmission Complete flag is set. */
#define UART_REG_TC(inst)           (true)

/** @brief Write one byte to the transmit data register. */
#define UART_REG_TDR_WRITE(inst, b) ((void)(inst), (void)(b))

/** @brief Read one byte from the receive data register. */
#define UART_REG_RDR_READ(inst)     (0U)

/** @brief True if an overrun error flag is set. */
#define UART_REG_ORE(inst)          (false)

/** @brief True if a framing error flag is set. */
#define UART_REG_FE(inst)           (false)

/** @brief Clear all error flags for @p inst. */
#define UART_REG_CLEAR_ERR(inst)    ((void)(inst))

/** @brief Enable the peripheral clock for @p inst. */
#define UART_CLK_ENABLE(inst)       ((void)(inst))

/** @brief Disable the peripheral clock for @p inst. */
#define UART_CLK_DISABLE(inst)      ((void)(inst))

/** @brief Return current system tick in milliseconds. */
#define UART_GET_TICK_MS()          (0U)

/** @brief Enter / exit a critical section. */
#define UART_ENTER_CRITICAL()       do { } while (0)
#define UART_EXIT_CRITICAL()        do { } while (0)

/* =========================================================
 * Private helpers
 * ========================================================= */

/**
 * @brief Validate a handle pointer and confirm it was initialised.
 *
 * @param[in] h  Pointer to check.
 *
 * @return true if the handle is usable, false otherwise.
 */
static bool uart_handle_valid(const UART_Handle *h)
{
    return (h != NULL) && h->initialized;
}

/**
 * @brief Push one byte into the TX ring-buffer.
 *
 * @param[in,out] h    UART handle.
 * @param[in]     byte Byte to enqueue.
 *
 * @return true if the byte was enqueued, false if the buffer is full.
 */
static bool uart_tx_push(UART_Handle *h, uint8_t byte)
{
    uint16_t next = (uint16_t)((h->tx_head + 1U) % UART_TX_BUF_SIZE);
    if (next == h->tx_tail)
    {
        return false; /* buffer full */
    }
    h->tx_buf[h->tx_head] = byte;
    h->tx_head = next;
    return true;
}

/**
 * @brief Pop one byte from the TX ring-buffer.
 *
 * @param[in,out] h     UART handle.
 * @param[out]    byte  Receives the dequeued byte.
 *
 * @return true if a byte was available, false if the buffer is empty.
 */
static bool uart_tx_pop(UART_Handle *h, uint8_t *byte)
{
    if (h->tx_tail == h->tx_head)
    {
        return false; /* buffer empty */
    }
    *byte = h->tx_buf[h->tx_tail];
    h->tx_tail = (uint16_t)((h->tx_tail + 1U) % UART_TX_BUF_SIZE);
    return true;
}

/**
 * @brief Push one byte into the RX ring-buffer.
 *
 * @param[in,out] h    UART handle.
 * @param[in]     byte Received byte to store.
 *
 * @return true if stored successfully, false if the buffer is full (overrun).
 */
static bool uart_rx_push(UART_Handle *h, uint8_t byte)
{
    uint16_t next = (uint16_t)((h->rx_head + 1U) % UART_RX_BUF_SIZE);
    if (next == h->rx_tail)
    {
        h->status_flags |= (uint32_t)UART_STATUS_OVERRUN;
        return false;
    }
    h->rx_buf[h->rx_head] = byte;
    h->rx_head = next;
    return true;
}

/**
 * @brief Pop one byte from the RX ring-buffer.
 *
 * @param[in,out] h     UART handle.
 * @param[out]    byte  Receives the dequeued byte.
 *
 * @return true if a byte was available, false if the buffer is empty.
 */
static bool uart_rx_pop(UART_Handle *h, uint8_t *byte)
{
    if (h->rx_tail == h->rx_head)
    {
        return false;
    }
    *byte = h->rx_buf[h->rx_tail];
    h->rx_tail = (uint16_t)((h->rx_tail + 1U) % UART_RX_BUF_SIZE);
    return true;
}

/* =========================================================
 * Public API implementation
 * ========================================================= */

/**
 * @brief Initialise a UART peripheral and its associated handle.
 *
 * Enables the peripheral clock, configures baud rate, frame format,
 * and flow control. Registers callbacks from @p cfg and resets all
 * internal buffer indices.
 *
 * @param[out] handle  Caller-allocated UART_Handle to populate.
 * @param[in]  cfg     Desired peripheral configuration.
 *
 * @return UART_OK          on success.
 * @return UART_ERR_PARAM   if either pointer is NULL or cfg has invalid values.
 * @return UART_ERR_HW      if peripheral clock or baud-rate cannot be configured.
 */
UART_Status UART_Init(UART_Handle *handle, const UART_Config *cfg)
{
    if (handle == NULL || cfg == NULL)
    {
        return UART_ERR_PARAM;
    }
    if (cfg->instance >= UART_INSTANCE_COUNT)
    {
        return UART_ERR_PARAM;
    }
    if (cfg->baud_rate == 0U)
    {
        return UART_ERR_PARAM;
    }

    memset(handle, 0, sizeof(UART_Handle));
    handle->config = *cfg;

    UART_CLK_ENABLE(cfg->instance);

    /* Hardware configuration would be written here via register shims. */

    handle->initialized = true;
    return UART_OK;
}

/**
 * @brief Disable the UART peripheral and release resources.
 *
 * Waits for any active TX to drain, then turns off the peripheral clock
 * and zeroes the handle.
 *
 * @param[in,out] handle  Initialised UART handle.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if @p handle is NULL or not initialised.
 * @return UART_ERR_TIMEOUT if the active transfer did not drain.
 */
UART_Status UART_DeInit(UART_Handle *handle)
{
    if (!uart_handle_valid(handle))
    {
        return UART_ERR_PARAM;
    }

    /* Wait for TC flag with timeout */
    uint32_t start = UART_GET_TICK_MS();
    while (!UART_REG_TC(handle->config.instance))
    {
        if ((UART_GET_TICK_MS() - start) >= UART_TIMEOUT_DEFAULT_MS)
        {
            return UART_ERR_TIMEOUT;
        }
    }

    UART_CLK_DISABLE(handle->config.instance);
    memset(handle, 0, sizeof(UART_Handle));
    return UART_OK;
}

/**
 * @brief Transmit @p len bytes in blocking mode.
 *
 * Polls TXE and feeds bytes one at a time. Returns after the last byte
 * enters the shift register or @p timeout_ms elapses.
 *
 * @param[in] handle      Initialised UART handle.
 * @param[in] data        Source buffer.
 * @param[in] len         Number of bytes to send.
 * @param[in] timeout_ms  Polling timeout per-byte.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if arguments are invalid.
 * @return UART_ERR_BUSY   if a transfer is already in progress.
 * @return UART_ERR_TIMEOUT if TXE did not clear within @p timeout_ms.
 */
UART_Status UART_Transmit(UART_Handle *handle,
                           const uint8_t *data,
                           size_t len,
                           uint32_t timeout_ms)
{
    if (!uart_handle_valid(handle) || data == NULL || len == 0U)
    {
        return UART_ERR_PARAM;
    }
    if (handle->status_flags & (uint32_t)UART_STATUS_TX_BUSY)
    {
        return UART_ERR_BUSY;
    }

    handle->status_flags |= (uint32_t)UART_STATUS_TX_BUSY;

    for (size_t i = 0U; i < len; i++)
    {
        uint32_t start = UART_GET_TICK_MS();
        while (!UART_REG_TXE(handle->config.instance))
        {
            if ((UART_GET_TICK_MS() - start) >= timeout_ms)
            {
                handle->status_flags &= ~(uint32_t)UART_STATUS_TX_BUSY;
                handle->status_flags |=  (uint32_t)UART_STATUS_TIMEOUT;
                return UART_ERR_TIMEOUT;
            }
        }
        UART_REG_TDR_WRITE(handle->config.instance, data[i]);
    }

    /* Wait for transmission complete */
    uint32_t start = UART_GET_TICK_MS();
    while (!UART_REG_TC(handle->config.instance))
    {
        if ((UART_GET_TICK_MS() - start) >= timeout_ms)
        {
            handle->status_flags &= ~(uint32_t)UART_STATUS_TX_BUSY;
            return UART_ERR_TIMEOUT;
        }
    }

    handle->status_flags &= ~(uint32_t)UART_STATUS_TX_BUSY;
    return UART_OK;
}

/**
 * @brief Receive up to @p len bytes in blocking mode.
 *
 * First drains the software RX ring-buffer, then polls RXNE for remaining
 * bytes until @p len bytes have been collected or @p timeout_ms elapses.
 *
 * @param[in]  handle      Initialised UART handle.
 * @param[out] buf         Destination buffer.
 * @param[in]  len         Desired byte count.
 * @param[in]  timeout_ms  Maximum wait time.
 * @param[out] received    If non-NULL, set to the actual number of bytes read.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if required arguments are invalid.
 * @return UART_ERR_BUSY   if a receive is already in progress.
 * @return UART_ERR_TIMEOUT if not enough bytes arrived in time.
 * @return UART_ERR_FRAMING on a framing error.
 * @return UART_ERR_OVERRUN on a receive overrun.
 */
UART_Status UART_Receive(UART_Handle *handle,
                          uint8_t *buf,
                          size_t len,
                          uint32_t timeout_ms,
                          size_t *received)
{
    if (!uart_handle_valid(handle) || buf == NULL || len == 0U)
    {
        return UART_ERR_PARAM;
    }
    if (handle->status_flags & (uint32_t)UART_STATUS_RX_BUSY)
    {
        return UART_ERR_BUSY;
    }

    handle->status_flags |= (uint32_t)UART_STATUS_RX_BUSY;

    size_t count = 0U;

    /* Drain software ring-buffer first */
    while (count < len)
    {
        if (!uart_rx_pop(handle, &buf[count]))
        {
            break;
        }
        count++;
    }

    /* Poll hardware for remaining bytes */
    uint32_t start = UART_GET_TICK_MS();
    while (count < len)
    {
        if (UART_REG_FE(handle->config.instance))
        {
            UART_REG_CLEAR_ERR(handle->config.instance);
            handle->status_flags |=  (uint32_t)UART_STATUS_FRAMING;
            handle->status_flags &= ~(uint32_t)UART_STATUS_RX_BUSY;
            if (received != NULL) { *received = count; }
            return UART_ERR_FRAMING;
        }
        if (UART_REG_ORE(handle->config.instance))
        {
            UART_REG_CLEAR_ERR(handle->config.instance);
            handle->status_flags |=  (uint32_t)UART_STATUS_OVERRUN;
            handle->status_flags &= ~(uint32_t)UART_STATUS_RX_BUSY;
            if (received != NULL) { *received = count; }
            return UART_ERR_OVERRUN;
        }
        if (UART_REG_RXNE(handle->config.instance))
        {
            buf[count++] = (uint8_t)UART_REG_RDR_READ(handle->config.instance);
            start = UART_GET_TICK_MS(); /* reset timeout on each received byte */
        }
        else if ((UART_GET_TICK_MS() - start) >= timeout_ms)
        {
            handle->status_flags |=  (uint32_t)UART_STATUS_TIMEOUT;
            handle->status_flags &= ~(uint32_t)UART_STATUS_RX_BUSY;
            if (received != NULL) { *received = count; }
            return UART_ERR_TIMEOUT;
        }
    }

    handle->status_flags &= ~(uint32_t)UART_STATUS_RX_BUSY;
    if (received != NULL) { *received = count; }
    return UART_OK;
}

/**
 * @brief Start a non-blocking interrupt-driven transmit.
 *
 * Copies @p len bytes into the TX ring-buffer and enables the TXE interrupt
 * to drain it in the background.
 *
 * @param[in] handle  Initialised UART handle.
 * @param[in] data    Source buffer; caller retains ownership.
 * @param[in] len     Number of bytes to transmit.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if arguments are invalid.
 * @return UART_ERR_BUSY   if a transmit is already running.
 */
UART_Status UART_TransmitIT(UART_Handle *handle,
                              const uint8_t *data,
                              size_t len)
{
    if (!uart_handle_valid(handle) || data == NULL || len == 0U)
    {
        return UART_ERR_PARAM;
    }
    if (handle->status_flags & (uint32_t)UART_STATUS_TX_BUSY)
    {
        return UART_ERR_BUSY;
    }

    UART_ENTER_CRITICAL();
    for (size_t i = 0U; i < len; i++)
    {
        if (!uart_tx_push(handle, data[i]))
        {
            UART_EXIT_CRITICAL();
            return UART_ERR_BUSY; /* ring-buffer full */
        }
    }
    handle->status_flags |= (uint32_t)UART_STATUS_TX_BUSY;
    UART_EXIT_CRITICAL();

    /* TXE interrupt enable would be set here via register shim */

    return UART_OK;
}

/**
 * @brief Start a non-blocking interrupt-driven receive.
 *
 * Registers @p buf as the destination for the next @p len bytes.
 * The UART_RxCallback fires when the count is reached or a line-idle
 * event occurs.
 *
 * @param[in]  handle  Initialised UART handle.
 * @param[out] buf     Destination buffer; must stay valid until callback.
 * @param[in]  len     Maximum bytes to receive.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if arguments are invalid.
 * @return UART_ERR_BUSY   if a receive is already in progress.
 */
UART_Status UART_ReceiveIT(UART_Handle *handle,
                             uint8_t *buf,
                             size_t len)
{
    if (!uart_handle_valid(handle) || buf == NULL || len == 0U)
    {
        return UART_ERR_PARAM;
    }
    if (handle->status_flags & (uint32_t)UART_STATUS_RX_BUSY)
    {
        return UART_ERR_BUSY;
    }

    handle->status_flags |= (uint32_t)UART_STATUS_RX_BUSY;

    /* RXNE interrupt enable would be set here via register shim */
    (void)buf;
    (void)len;

    return UART_OK;
}

/**
 * @brief Query the current status flags.
 *
 * @param[in]  handle  Initialised UART handle.
 * @param[out] flags   Receives the bitmask of UART_StatusFlag values.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if either argument is invalid.
 */
UART_Status UART_GetStatus(const UART_Handle *handle, uint32_t *flags)
{
    if (!uart_handle_valid(handle) || flags == NULL)
    {
        return UART_ERR_PARAM;
    }
    *flags = handle->status_flags;
    return UART_OK;
}

/**
 * @brief Flush the software and hardware RX buffers.
 *
 * Resets ring-buffer indices and clears error status flags. Ongoing DMA
 * transfers are not affected.
 *
 * @param[in,out] handle  Initialised UART handle.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if @p handle is invalid.
 */
UART_Status UART_FlushRx(UART_Handle *handle)
{
    if (!uart_handle_valid(handle))
    {
        return UART_ERR_PARAM;
    }

    UART_ENTER_CRITICAL();
    handle->rx_head = 0U;
    handle->rx_tail = 0U;
    handle->status_flags &= ~((uint32_t)UART_STATUS_OVERRUN |
                               (uint32_t)UART_STATUS_FRAMING |
                               (uint32_t)UART_STATUS_PARITY  |
                               (uint32_t)UART_STATUS_NOISE);
    UART_REG_CLEAR_ERR(handle->config.instance);
    UART_EXIT_CRITICAL();

    return UART_OK;
}

/**
 * @brief Return the number of bytes waiting in the RX ring-buffer.
 *
 * @param[in] handle  Initialised UART handle.
 *
 * @return Byte count available for immediate reading (0 if handle is NULL).
 */
size_t UART_RxAvailable(const UART_Handle *handle)
{
    if (!uart_handle_valid(handle))
    {
        return 0U;
    }

    uint16_t head = handle->rx_head;
    uint16_t tail = handle->rx_tail;

    if (head >= tail)
    {
        return (size_t)(head - tail);
    }
    return (size_t)(UART_RX_BUF_SIZE - tail + head);
}

/**
 * @brief USART ISR handler — call from the MCU vector.
 *
 * Processes TXE, TC, RXNE, and error flags. Feeds bytes from the TX
 * ring-buffer or stores incoming bytes into the RX ring-buffer and
 * dispatches registered callbacks.
 *
 * @param[in,out] handle  Handle for the peripheral that raised the interrupt.
 */
void UART_IRQHandler(UART_Handle *handle)
{
    if (!uart_handle_valid(handle))
    {
        return;
    }

    /* RX: store incoming byte into ring-buffer */
    if (UART_REG_RXNE(handle->config.instance))
    {
        uint8_t byte = (uint8_t)UART_REG_RDR_READ(handle->config.instance);
        uart_rx_push(handle, byte);

        if (handle->config.rx_cb != NULL)
        {
            /* Single-byte notification — app can call UART_RxAvailable() for more */
            handle->config.rx_cb(handle->config.instance, &byte, 1U);
        }
    }

    /* TX: send next byte from ring-buffer */
    if (UART_REG_TXE(handle->config.instance))
    {
        uint8_t byte;
        if (uart_tx_pop(handle, &byte))
        {
            UART_REG_TDR_WRITE(handle->config.instance, byte);
        }
        else
        {
            /* Buffer drained — disable TXE interrupt and signal completion */
            handle->status_flags &= ~(uint32_t)UART_STATUS_TX_BUSY;
            if (handle->config.tx_cb != NULL)
            {
                handle->config.tx_cb(handle->config.instance, 0U);
            }
        }
    }

    /* Error handling */
    if (UART_REG_ORE(handle->config.instance) || UART_REG_FE(handle->config.instance))
    {
        uint32_t err_flags = 0U;
        if (UART_REG_ORE(handle->config.instance)) { err_flags |= (uint32_t)UART_STATUS_OVERRUN; }
        if (UART_REG_FE(handle->config.instance))  { err_flags |= (uint32_t)UART_STATUS_FRAMING; }

        handle->status_flags |= err_flags;
        UART_REG_CLEAR_ERR(handle->config.instance);

        if (handle->config.error_cb != NULL)
        {
            handle->config.error_cb(handle->config.instance, err_flags);
        }
    }
}
