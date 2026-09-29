/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#include <cstdint>

// UART0 on GP0 (TX) / GP1 (RX) toward the target serial device.
// RX is captured by IRQ into a ring buffer; TX is written to the hardware FIFO without blocking.
namespace uart_port
{
    static constexpr unsigned PIN_TX = 0;
    static constexpr unsigned PIN_RX = 1;
    static constexpr uint32_t RX_RING_SIZE = 16384;  // ~1.4 s at 115200 bps (8N1: 11.52 KB/s)
    static constexpr uint32_t TX_FIFO_DEPTH = 32;

    struct stats_t {
        uint32_t overrun;          // hardware RX FIFO overrun (main loop / ISR stalled)
        uint32_t frame_err;        // framing / parity / break errors
        uint32_t ring_overflow;    // RX ring full (SD or USB stalled too long)
        uint32_t ring_count;
        uint32_t ring_high_water;
    };

    void init(uint32_t baud);
    uint32_t get_baud();  // actual baud rate configured

    // RX (consumer side)
    uint32_t rx_count();
    uint32_t rx_peek_contiguous(const uint8_t** p);
    void rx_consume(uint32_t n);

    // TX
    bool tx_fifo_empty();
    // write up to n bytes into the TX FIFO without blocking, returns the number of bytes written
    uint32_t tx_write_raw(const uint8_t* p, uint32_t n);

    stats_t stats();
}
