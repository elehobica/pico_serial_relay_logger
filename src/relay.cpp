/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "relay.h"

#include "pico/stdlib.h"
#include "tusb.h"

#include "sd_logger.h"
#include "uart_port.h"
#include "usb_descriptors.h"

namespace relay
{
    static constexpr uint8_t ITF = CDC_ITF_RELAY;

    static uint64_t _rx_bytes = 0;
    static uint64_t _tx_bytes = 0;
    static uint32_t _usb_drop = 0;
    static uint32_t _last_activity_ms = 0;
    static bool _prev_connected = false;

    static inline uint32_t _millis()
    {
        return to_ms_since_boot(get_absolute_time());
    }

    void init()
    {
        _rx_bytes = 0;
        _tx_bytes = 0;
        _usb_drop = 0;
        _last_activity_ms = 0;
        _prev_connected = false;
    }

    void task()
    {
        bool connected = tud_cdc_n_connected(ITF);
        if (_prev_connected && !connected) {
            // host went away: discard anything queued toward it, so the next session starts clean
            tud_cdc_n_write_clear(ITF);
            tud_cdc_n_read_flush(ITF);
        }
        _prev_connected = connected;

        // UART -> SD (always) and USB (only while the host has the port open)
        bool wrote_usb = false;
        for (int i = 0; i < 2; i++) {  // twice to cover ring wrap-around
            const uint8_t* p;
            uint32_t n = uart_port::rx_peek_contiguous(&p);
            if (n == 0) break;
            sd_logger::push(p, n);
            if (connected) {
                uint32_t avail = tud_cdc_n_write_available(ITF);
                uint32_t w = (n < avail) ? n : avail;
                if (w > 0) {
                    tud_cdc_n_write(ITF, p, w);
                    wrote_usb = true;
                }
                _usb_drop += n - w;
            }
            uart_port::rx_consume(n);
            _rx_bytes += n;
            _last_activity_ms = _millis();
        }
        if (wrote_usb) tud_cdc_n_write_flush(ITF);

        // USB -> UART and SD. Take at most one TX FIFO worth when the FIFO is empty,
        // so this never blocks; the host is throttled by USB NAK, no bytes are dropped.
        if (connected && tud_cdc_n_available(ITF) && uart_port::tx_fifo_empty()) {
            uint8_t buf[uart_port::TX_FIFO_DEPTH];
            uint32_t n = tud_cdc_n_read(ITF, buf, sizeof(buf));
            if (n > 0) {
                uart_port::tx_write_raw(buf, n);  // n <= FIFO depth and FIFO is empty: all bytes fit
                sd_logger::push(buf, n);
                _tx_bytes += n;
                _last_activity_ms = _millis();
            }
        }
    }

    stats_t stats()
    {
        stats_t s;
        s.rx_bytes = _rx_bytes;
        s.tx_bytes = _tx_bytes;
        s.usb_drop = _usb_drop;
        s.last_activity_ms = _last_activity_ms;
        s.host_connected = tud_cdc_n_connected(ITF);
        return s;
    }

    bool activity_within(uint32_t ms)
    {
        return _last_activity_ms != 0 && (_millis() - _last_activity_ms) < ms;
    }
}
