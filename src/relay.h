/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#include <cstdint>

// Data path glue:
//   UART RX ring -> USB CDC (relay) + SD logger
//   USB CDC (relay) -> UART TX + SD logger
// Never blocks. Works without a USB host (standalone logging).
namespace relay
{
    struct stats_t {
        uint64_t rx_bytes;         // UART -> (USB, SD)
        uint64_t tx_bytes;         // USB -> (UART, SD)
        uint32_t usb_drop;         // UART bytes not delivered to USB while the host was connected
        uint32_t last_activity_ms;
        bool host_connected;       // DTR asserted on the relay CDC port
    };

    void init();
    void task();
    stats_t stats();
    bool activity_within(uint32_t ms);
}
