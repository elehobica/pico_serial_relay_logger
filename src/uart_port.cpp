/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "uart_port.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "ring_buffer.h"

namespace uart_port
{
    static uart_inst_t* const UART_ID = uart0;
    static constexpr unsigned UART_IRQ = UART0_IRQ;

    static RingBuffer<RX_RING_SIZE> _rx_ring;
    static volatile uint32_t _overrun = 0;
    static volatile uint32_t _frame_err = 0;
    static volatile uint32_t _ring_overflow = 0;
    static uint32_t _baud = 0;

    static void _on_rx()
    {
        uart_hw_t* hw = uart_get_hw(UART_ID);
        while (!(hw->fr & UART_UARTFR_RXFE_BITS)) {
            uint32_t dr = hw->dr;  // data + per-byte error flags
            if (dr & UART_UARTDR_OE_BITS) _overrun = _overrun + 1;
            if (dr & (UART_UARTDR_FE_BITS | UART_UARTDR_PE_BITS | UART_UARTDR_BE_BITS)) _frame_err = _frame_err + 1;
            // keep the byte anyway: the log is a raw dump of what was seen on the line
            if (!_rx_ring.push(static_cast<uint8_t>(dr & 0xff))) _ring_overflow = _ring_overflow + 1;
        }
        hw->rsr = UART_UARTRSR_BITS;  // clear sticky error flags
    }

    void init(uint32_t baud)
    {
        _baud = uart_init(UART_ID, baud);
        gpio_set_function(PIN_TX, GPIO_FUNC_UART);
        gpio_set_function(PIN_RX, GPIO_FUNC_UART);
        uart_set_hw_flow(UART_ID, false, false);
        uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
        uart_set_fifo_enabled(UART_ID, true);

        irq_set_exclusive_handler(UART_IRQ, _on_rx);
        irq_set_enabled(UART_IRQ, true);
        // RX IRQ + RX timeout IRQ, RX FIFO threshold 1/8 (4 bytes): low latency, few interrupts
        uart_set_irq_enables(UART_ID, true, false);
    }

    uint32_t get_baud()
    {
        return _baud;
    }

    uint32_t rx_count()
    {
        return _rx_ring.count();
    }

    uint32_t rx_peek_contiguous(const uint8_t** p)
    {
        return _rx_ring.peek_contiguous(p);
    }

    void rx_consume(uint32_t n)
    {
        _rx_ring.consume(n);
    }

    bool tx_fifo_empty()
    {
        return (uart_get_hw(UART_ID)->fr & UART_UARTFR_TXFE_BITS) != 0;
    }

    uint32_t tx_write_raw(const uint8_t* p, uint32_t n)
    {
        uart_hw_t* hw = uart_get_hw(UART_ID);
        uint32_t i = 0;
        while (i < n && !(hw->fr & UART_UARTFR_TXFF_BITS)) {
            hw->dr = p[i++];
        }
        return i;
    }

    stats_t stats()
    {
        stats_t s;
        s.overrun = _overrun;
        s.frame_err = _frame_err;
        s.ring_overflow = _ring_overflow;
        s.ring_count = _rx_ring.count();
        s.ring_high_water = _rx_ring.high_water();
        return s;
    }
}
