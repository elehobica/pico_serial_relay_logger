/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

// pico stdio driver over the application-owned TinyUSB CDC console interface (CDC_ITF_CONSOLE)
// (replacement of pico_stdio_usb, which owns its own tusb_config.h / descriptors)
void stdio_cdc_init();
bool stdio_cdc_connected();
