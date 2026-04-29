/**
 * @file uart.h
 * @brief UART / USART driver interface for ARM Cortex-M targets.
 *
 * Supports synchronous (blocking), interrupt-driven, and DMA-backed
 * transmit/receive operations. Each peripheral instance is represented
 * by a UART_Handle that the caller allocates and passes to every API call.
 *
 * @author  Embedded BSP Team
 * @version 2.0.1
 * @date    2024-03-10
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================
 * Compile-time limits
 * ========================================================= */

/** @brief Number of UART peripheral instances available on the target. */
#define UART_INSTANCE_COUNT     3U

/** @brief Default blocking-call timeout in milliseconds. */
#define UART_TIMEOUT_DEFAULT_MS 1000U

/** @brief Size of the internal software receive ring-buffer in bytes. */
#define UART_RX_BUF_SIZE        256U

/** @brief Size of the internal software transmit ring-buffer in bytes. */
#define UART_TX_BUF_SIZE        256U

/* =========================================================
 * Enumerations
 * ========================================================= */

/**
 * @brief UART peripheral instance selector.
 */
typedef enum
{
    UART_INSTANCE_1 = 0, /**< USART1 – typically maps to the primary debug console. */
    UART_INSTANCE_2,     /**< USART2 – general-purpose. */
    UART_INSTANCE_3,     /**< USART3 – general-purpose or RS-485 bus.              */
} UART_Instance;

/**
 * @brief Standard baud-rate presets.
 *
 * The driver also accepts arbitrary integer values; use UART_BAUD_CUSTOM
 * and set the @p baud_rate field in UART_Config directly.
 */
typedef enum
{
    UART_BAUD_9600    =   9600U, /**<   9 600 bps – legacy / low-speed sensors. */
    UART_BAUD_19200   =  19200U, /**<  19 200 bps.                              */
    UART_BAUD_38400   =  38400U, /**<  38 400 bps.                              */
    UART_BAUD_57600   =  57600U, /**<  57 600 bps.                              */
    UART_BAUD_115200  = 115200U, /**< 115 200 bps – most common embedded rate.  */
    UART_BAUD_230400  = 230400U, /**< 230 400 bps.                              */
    UART_BAUD_460800  = 460800U, /**< 460 800 bps.                              */
    UART_BAUD_921600  = 921600U, /**< 921 600 bps – requires accurate clock.    */
} UART_BaudRate;

/**
 * @brief Number of data bits per frame.
 */
typedef enum
{
    UART_DATABITS_7 = 7, /**< 7-bit data (rarely used, requires parity).       */
    UART_DATABITS_8 = 8, /**< 8-bit data – standard.                           */
    UART_DATABITS_9 = 9  /**< 9-bit data – used for address-detect in RS-485.  */
} UART_DataBits;

/**
 * @brief Number of stop bits appended to each frame.
 */
typedef enum
{
    UART_STOPBITS_0_5 = 0, /**< 0.5 stop bits – synchronous mode only.    */
    UART_STOPBITS_1,       /**< 1 stop bit – standard asynchronous.        */
    UART_STOPBITS_1_5,     /**< 1.5 stop bits – smartcard mode.            */
    UART_STOPBITS_2        /**< 2 stop bits – increases inter-frame gap.   */
} UART_StopBits;

/**
 * @brief Parity generation and checking.
 */
typedef enum
{
    UART_PARITY_NONE = 0, /**< No parity bit – 8-N-1 when combined with 8 data bits. */
    UART_PARITY_EVEN,     /**< Even parity.                                           */
    UART_PARITY_ODD       /**< Odd parity.                                            */
} UART_Parity;

/**
 * @brief Hardware flow-control mode.
 */
typedef enum
{
    UART_FLOW_NONE    = 0, /**< No hardware flow control (software or none).  */
    UART_FLOW_RTS,         /**< RTS output only.                              */
    UART_FLOW_CTS,         /**< CTS input only.                               */
    UART_FLOW_RTS_CTS      /**< Full RTS/CTS hardware handshake.              */
} UART_FlowControl;

/**
 * @brief Transfer mode selector.
 */
typedef enum
{
    UART_MODE_BLOCKING = 0, /**< CPU spins until transfer complete or timeout.     */
    UART_MODE_INTERRUPT,    /**< ISR-driven; completion callback invoked on finish. */
    UART_MODE_DMA           /**< DMA-driven; callback invoked from DMA ISR.         */
} UART_TransferMode;

/**
 * @brief Bit-field flags that describe the current peripheral state.
 *
 * Multiple flags may be set simultaneously; use bitwise AND to test.
 */
typedef enum
{
    UART_STATUS_OK        = 0x00U, /**< No errors, peripheral idle.               */
    UART_STATUS_TX_BUSY   = 0x01U, /**< Transmit operation in progress.           */
    UART_STATUS_RX_BUSY   = 0x02U, /**< Receive operation in progress.            */
    UART_STATUS_OVERRUN   = 0x04U, /**< Receive overrun – data lost.              */
    UART_STATUS_FRAMING   = 0x08U, /**< Framing error detected.                   */
    UART_STATUS_PARITY    = 0x10U, /**< Parity error detected.                    */
    UART_STATUS_NOISE     = 0x20U, /**< Noise error detected on RX line.          */
    UART_STATUS_TIMEOUT   = 0x40U  /**< Blocking call timed out.                  */
} UART_StatusFlag;

/**
 * @brief Driver return codes.
 */
typedef enum
{
    UART_OK           =  0, /**< Operation completed successfully.           */
    UART_ERR_PARAM    = -1, /**< Invalid parameter.                          */
    UART_ERR_BUSY     = -2, /**< Peripheral already has an active transfer.  */
    UART_ERR_TIMEOUT  = -3, /**< Blocking transfer timed out.                */
    UART_ERR_OVERRUN  = -4, /**< Receive buffer overrun.                     */
    UART_ERR_FRAMING  = -5, /**< Framing error.                              */
    UART_ERR_HW       = -6  /**< Unrecoverable hardware fault.               */
} UART_Status;

/* =========================================================
 * Callback typedefs
 * ========================================================= */

/**
 * @brief Callback invoked when a non-blocking transmit completes.
 *
 * @param instance  UART peripheral instance that generated the event.
 * @param bytes_sent  Number of bytes actually transmitted.
 */
typedef void (*UART_TxCallback)(UART_Instance instance, size_t bytes_sent);

/**
 * @brief Callback invoked when a non-blocking receive completes or a
 *        character matching the idle detection is received.
 *
 * @param instance    UART peripheral instance that generated the event.
 * @param buf         Pointer to the buffer supplied to UART_ReceiveIT / _DMA.
 * @param bytes_recv  Number of bytes written into @p buf.
 */
typedef void (*UART_RxCallback)(UART_Instance instance, const uint8_t *buf, size_t bytes_recv);

/**
 * @brief Callback invoked when the driver detects an error condition.
 *
 * @param instance  UART peripheral instance where the error occurred.
 * @param flags     Bitwise OR of UART_StatusFlag values describing the error.
 */
typedef void (*UART_ErrorCallback)(UART_Instance instance, uint32_t flags);

/* =========================================================
 * Configuration & handle structures
 * ========================================================= */

/**
 * @brief Immutable configuration snapshot passed to UART_Init().
 *
 * Once UART_Init() returns successfully, changes to this structure have
 * no effect. Call UART_DeInit() then UART_Init() to reconfigure.
 */
typedef struct
{
    UART_Instance    instance;    /**< Which peripheral to configure.             */
    uint32_t         baud_rate;   /**< Baud rate in bps (e.g. 115200).           */
    UART_DataBits    data_bits;   /**< Data bits per frame.                       */
    UART_StopBits    stop_bits;   /**< Stop bits per frame.                       */
    UART_Parity      parity;      /**< Parity mode.                               */
    UART_FlowControl flow;        /**< Hardware flow control.                     */
    UART_TxCallback  tx_cb;       /**< Transmit-complete callback (may be NULL). */
    UART_RxCallback  rx_cb;       /**< Receive-complete callback (may be NULL).  */
    UART_ErrorCallback error_cb;  /**< Error callback (may be NULL).             */
} UART_Config;

/**
 * @brief Runtime handle representing one UART peripheral instance.
 *
 * Allocate statically or on the heap; pass a pointer to all UART API calls.
 * The driver fills this structure during UART_Init(); treat all fields as
 * read-only from application code.
 */
typedef struct
{
    UART_Config  config;                  /**< Copy of the configuration snapshot.  */
    uint32_t     status_flags;            /**< Current status (UART_StatusFlag OR). */
    uint8_t      rx_buf[UART_RX_BUF_SIZE]; /**< Internal RX ring-buffer.            */
    uint8_t      tx_buf[UART_TX_BUF_SIZE]; /**< Internal TX ring-buffer.            */
    uint16_t     rx_head;                 /**< Ring-buffer write index.             */
    uint16_t     rx_tail;                 /**< Ring-buffer read index.              */
    uint16_t     tx_head;                 /**< TX ring-buffer write index.          */
    uint16_t     tx_tail;                 /**< TX ring-buffer read index.           */
    bool         initialized;            /**< True after a successful UART_Init(). */
} UART_Handle;

/* =========================================================
 * Public API
 * ========================================================= */

/**
 * @brief Initialise a UART peripheral and its associated handle.
 *
 * Configures the baud-rate generator, frame format, flow-control lines, and
 * (for interrupt/DMA modes) the NVIC priority. The handle's internal buffers
 * are zeroed and the ring-buffer indices are reset.
 *
 * @param[out] handle  Pointer to a caller-allocated UART_Handle.
 *                     Must not be NULL.
 * @param[in]  cfg     Pointer to the desired configuration.
 *                     Must not be NULL.
 *
 * @return UART_OK          on success.
 * @return UART_ERR_PARAM   if either pointer is NULL or cfg contains invalid values.
 * @return UART_ERR_HW      if the peripheral clock or baud-rate divider cannot be set.
 */
UART_Status UART_Init(UART_Handle *handle, const UART_Config *cfg);

/**
 * @brief Disable the peripheral and release all associated resources.
 *
 * Waits up to UART_TIMEOUT_DEFAULT_MS for any active transfer to finish,
 * then disables the peripheral clock and zeroes the handle. After this call
 * the handle must be re-initialised before use.
 *
 * @param[in,out] handle  Handle returned by a previous successful UART_Init().
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if @p handle is NULL or not initialised.
 * @return UART_ERR_TIMEOUT if the in-progress transfer did not drain in time.
 */
UART_Status UART_DeInit(UART_Handle *handle);

/**
 * @brief Transmit data in blocking mode.
 *
 * The function returns only after all @p len bytes have been written into the
 * hardware FIFO / shift register or @p timeout_ms has elapsed, whichever
 * comes first.
 *
 * @param[in] handle      Initialised UART handle.
 * @param[in] data        Pointer to the transmit buffer. Must not be NULL.
 * @param[in] len         Number of bytes to transmit. Must be > 0.
 * @param[in] timeout_ms  Maximum time to wait in milliseconds.
 *                        Pass UART_TIMEOUT_DEFAULT_MS for the driver default.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if @p handle or @p data is NULL, or @p len is 0.
 * @return UART_ERR_BUSY   if another transmit is already in progress.
 * @return UART_ERR_TIMEOUT if the transfer did not complete within @p timeout_ms.
 */
UART_Status UART_Transmit(UART_Handle *handle,
                          const uint8_t *data,
                          size_t len,
                          uint32_t timeout_ms);

/**
 * @brief Receive data in blocking mode.
 *
 * Blocks until exactly @p len bytes have been received into @p buf or
 * @p timeout_ms has elapsed.
 *
 * @param[in]  handle      Initialised UART handle.
 * @param[out] buf         Buffer that receives the incoming bytes. Must not be NULL.
 * @param[in]  len         Number of bytes to receive. Must be > 0.
 * @param[in]  timeout_ms  Maximum wait time in milliseconds.
 * @param[out] received    Optional pointer that receives the actual byte count
 *                         written to @p buf. May be NULL.
 *
 * @return UART_OK         on success (exactly @p len bytes received).
 * @return UART_ERR_PARAM  if a required pointer is NULL or @p len is 0.
 * @return UART_ERR_BUSY   if another receive is already in progress.
 * @return UART_ERR_TIMEOUT if fewer than @p len bytes arrived within @p timeout_ms.
 * @return UART_ERR_FRAMING on a frame error mid-transfer.
 * @return UART_ERR_OVERRUN if the hardware FIFO overflowed mid-transfer.
 */
UART_Status UART_Receive(UART_Handle *handle,
                         uint8_t *buf,
                         size_t len,
                         uint32_t timeout_ms,
                         size_t *received);

/**
 * @brief Start a non-blocking (interrupt-driven) transmit.
 *
 * Returns immediately after enqueuing @p len bytes into the TX ring-buffer.
 * The registered UART_TxCallback is invoked from the USART TXE/TC ISR when
 * the last byte has been shifted out.
 *
 * @param[in] handle  Initialised UART handle.
 * @param[in] data    Transmit buffer; must remain valid until the callback fires.
 * @param[in] len     Number of bytes to transmit. Must be > 0.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if @p handle or @p data is NULL, or @p len is 0.
 * @return UART_ERR_BUSY   if a transmit is already queued.
 */
UART_Status UART_TransmitIT(UART_Handle *handle,
                             const uint8_t *data,
                             size_t len);

/**
 * @brief Start a non-blocking (interrupt-driven) receive.
 *
 * Returns immediately. The driver accumulates incoming bytes into @p buf.
 * The UART_RxCallback is invoked when @p len bytes have been received or
 * a line-idle event is detected (if supported by the hardware).
 *
 * @param[in]  handle  Initialised UART handle.
 * @param[out] buf     Buffer to fill; must remain valid until the callback fires.
 * @param[in]  len     Maximum number of bytes to receive.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if @p handle or @p buf is NULL, or @p len is 0.
 * @return UART_ERR_BUSY   if a receive is already queued.
 */
UART_Status UART_ReceiveIT(UART_Handle *handle,
                            uint8_t *buf,
                            size_t len);

/**
 * @brief Query the current status flags of the peripheral.
 *
 * @param[in]  handle  Initialised UART handle.
 * @param[out] flags   Receives a bitwise OR of UART_StatusFlag values.
 *                     Must not be NULL.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if either pointer is NULL or the handle is not initialised.
 */
UART_Status UART_GetStatus(const UART_Handle *handle, uint32_t *flags);

/**
 * @brief Discard all bytes in the software and hardware RX buffers.
 *
 * Does not affect in-progress DMA transfers. Clears overrun / framing /
 * parity error flags in the handle's status field.
 *
 * @param[in,out] handle  Initialised UART handle.
 *
 * @return UART_OK         on success.
 * @return UART_ERR_PARAM  if @p handle is NULL or not initialised.
 */
UART_Status UART_FlushRx(UART_Handle *handle);

/**
 * @brief Return the number of bytes currently available in the RX ring-buffer.
 *
 * @param[in] handle  Initialised UART handle.
 *
 * @return Number of bytes available (0 if none or handle is invalid).
 */
size_t UART_RxAvailable(const UART_Handle *handle);

/**
 * @brief Internal USART interrupt service routine handler.
 *
 * Called from the MCU vector (USART1_IRQHandler, USART2_IRQHandler, etc.).
 * Processes TXE, TC, RXNE, and error flags, updates the ring-buffers, and
 * fires registered callbacks. Do not call from application code.
 *
 * @param[in,out] handle  Handle for the peripheral that raised the interrupt.
 */
void UART_IRQHandler(UART_Handle *handle);

#endif /* UART_H */
