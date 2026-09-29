/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "stdio_cdc.h"

#include "pico/stdio/driver.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "usb_descriptors.h"

// Never block for long here: the main loop must keep draining the UART ring.
// The CDC TX FIFO is sized to take a whole status report without waiting;
// if the terminal is not attached or does not drain, drop the output quickly.
static constexpr uint32_t OUT_DEADLINE_US = 200;
static constexpr uint8_t ITF = CDC_ITF_CONSOLE;

static void (*_chars_available_callback)(void*) = nullptr;
static void* _chars_available_param = nullptr;

static void stdio_cdc_out_chars(const char* buf, int len)
{
    if (!tud_cdc_n_connected(ITF)) return;

    absolute_time_t deadline = make_timeout_time_us(OUT_DEADLINE_US);
    int written = 0;
    while (written < len) {
        uint32_t avail = tud_cdc_n_write_available(ITF);
        if (avail > 0) {
            uint32_t n = tud_cdc_n_write(ITF, buf + written, (uint32_t)(len - written) < avail ? (uint32_t)(len - written) : avail);
            written += (int)n;
        } else {
            tud_cdc_n_write_flush(ITF);
            tud_task();
        }
        if (time_reached(deadline)) break;  // drop the rest
    }
}

static void stdio_cdc_out_flush()
{
    tud_cdc_n_write_flush(ITF);
}

static int stdio_cdc_in_chars(char* buf, int len)
{
    if (!tud_cdc_n_connected(ITF) || !tud_cdc_n_available(ITF)) return PICO_ERROR_NO_DATA;
    uint32_t n = tud_cdc_n_read(ITF, buf, (uint32_t)len);
    return (n > 0) ? (int)n : PICO_ERROR_NO_DATA;
}

static void stdio_cdc_set_chars_available_callback(void (*fn)(void*), void* param)
{
    _chars_available_callback = fn;
    _chars_available_param = param;
}

// TinyUSB CDC receive callback (shared by all CDC instances; the relay instance is polled by relay::task())
extern "C" void tud_cdc_rx_cb(uint8_t itf)
{
    if (itf != ITF) return;
    if (_chars_available_callback) {
        _chars_available_callback(_chars_available_param);
    }
}

static stdio_driver_t stdio_cdc_driver = {
    .out_chars = stdio_cdc_out_chars,
    .out_flush = stdio_cdc_out_flush,
    .in_chars = stdio_cdc_in_chars,
    .set_chars_available_callback = stdio_cdc_set_chars_available_callback,
    .next = nullptr,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .last_ended_with_cr = false,
    .crlf_enabled = PICO_STDIO_DEFAULT_CRLF,
#endif
};

void stdio_cdc_init()
{
    stdio_set_driver_enabled(&stdio_cdc_driver, true);
}

bool stdio_cdc_connected()
{
    return tud_cdc_n_connected(ITF);
}
