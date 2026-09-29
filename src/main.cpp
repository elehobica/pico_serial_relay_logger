/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <cstdio>

#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "ConfigParam.h"
#include "relay.h"
#include "sd_logger.h"
#include "stdio_cdc.h"
#include "uart_port.h"
#include "usb_descriptors.h"

static bool picoW = false;
static constexpr uint PIN_LED = 25;  // PICO_DEFAULT_LED_PIN of Pico

static constexpr uint32_t BANNER_DELAY_MS = 300;
static constexpr uint32_t LED_ACTIVITY_WINDOW_MS = 200;
static constexpr uint32_t LED_ACTIVITY_PERIOD_MS = 100;  // 50 ms on / 50 ms off

// LED error indication: burst of (code + 5) blinks (0.2 s each) followed by a pause
enum class main_error_t {
    CYW43_INIT_ERROR = 0,
    FATFS_MOUNT_ERROR,
    FATFS_WRITE_ERROR
};

static inline uint32_t _millis()
{
    return to_ms_since_boot(get_absolute_time());
}

static bool _check_pico_w()
{
    adc_init();
    auto dir = gpio_get_dir(29);
    auto fnc = gpio_get_function(29);
    adc_gpio_init(29);
    adc_select_input(3);
    auto adc29 = adc_read();
    gpio_set_function(29, fnc);
    gpio_set_dir(29, dir);

    dir = gpio_get_dir(25);
    fnc = gpio_get_function(25);
    gpio_init(25);
    gpio_set_dir(25, GPIO_IN);
    auto gp25 = gpio_get(25);
    gpio_set_function(25, fnc);
    gpio_set_dir(25, dir);

    if (gp25) {
        return true; // Can't tell, so assume yes
    } else if (adc29 < 200) {
        return true; // PicoW
    } else {
        return false;
    }
}

static void _set_led(bool flag)
{
    static int last = -1;
    if (last == static_cast<int>(flag)) return;  // write on change only (cyw43 LED goes over SPI)
    last = static_cast<int>(flag);
    if (picoW) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, flag);
    } else {
        gpio_put(PIN_LED, flag);
    }
}

// blocking error indication (boot-fatal errors only)
static void _led_disp_error(const main_error_t error, const bool forever = false)
{
    int n = static_cast<int>(error) + 5;  // start from blink 5 times
    while (true) {
        for (int i = 0; i < n*2; i++) {
            _set_led(false);
            sleep_ms(200);
            if (i < n) _set_led(true);
            sleep_ms(200);
        }
        if (!forever) break;
    }
}

// non-blocking error pattern: n blinks (200 ms on / 200 ms off) then 1 s pause
static bool _led_error_pattern(const main_error_t error, uint32_t ms)
{
    uint32_t n = static_cast<uint32_t>(error) + 5;
    uint32_t cycle = n * 400 + 1000;
    uint32_t t = ms % cycle;
    return (t < n * 400) && ((t % 400) < 200);
}

static void _show_help()
{
    printf("---------------------------\r\n");
    printf("[console help]\r\n");
    printf(" 's' to show status\r\n");
    printf(" 'h' to show this help\r\n");
    printf("---------------------------\r\n");
}

static void _show_status()
{
    const relay::stats_t r = relay::stats();
    const uart_port::stats_t u = uart_port::stats();
    const sd_logger::status_t s = sd_logger::status();

    printf("---------------------------\r\n");
    printf("[UART]\r\n");
    printf(" baud rate:         %lu bps 8N1 (GP%u TX / GP%u RX)\r\n", static_cast<unsigned long>(uart_port::get_baud()), uart_port::PIN_TX, uart_port::PIN_RX);
    printf(" rx bytes:          %llu (UART -> USB/SD)\r\n", static_cast<unsigned long long>(r.rx_bytes));
    printf(" tx bytes:          %llu (USB -> UART/SD)\r\n", static_cast<unsigned long long>(r.tx_bytes));
    printf(" rx errors:         overrun %lu, framing %lu, ring overflow %lu\r\n", static_cast<unsigned long>(u.overrun), static_cast<unsigned long>(u.frame_err), static_cast<unsigned long>(u.ring_overflow));
    printf(" rx ring:           %lu / %lu (high water %lu)\r\n", static_cast<unsigned long>(u.ring_count), static_cast<unsigned long>(uart_port::RX_RING_SIZE), static_cast<unsigned long>(u.ring_high_water));
    printf("[USB]\r\n");
    printf(" configured:        %s\r\n", tud_mounted() ? "yes" : "no");
    printf(" relay port:        %s\r\n", r.host_connected ? "open (DTR on)" : "closed");
    printf(" relay drop:        %lu bytes (UART -> USB while port open)\r\n", static_cast<unsigned long>(r.usb_drop));
    printf("[microSD]\r\n");
    if (s.mounted) {
        printf(" card:              %s %7.2f GB (GB = 1E9 Bytes)\r\n", s.fs_type, s.capacity_gb);
    } else {
        printf(" card:              not mounted (%s FRESULT %d)\r\n", s.last_op, s.last_fr);
    }
    printf(" state:             %s", sd_logger::state_str(s.state));
    if (s.state == sd_logger::state_t::ERROR) {
        printf(" (%s FRESULT %d)", s.last_op, s.last_fr);
    }
    printf("\r\n");
    printf(" log file:          %s\r\n", (s.filename[0] != '\0') ? s.filename : "(none)");
    printf(" written bytes:     %llu\r\n", static_cast<unsigned long long>(s.written_bytes));
    printf(" pending / drop:    %lu / %lu bytes\r\n", static_cast<unsigned long>(s.pending), static_cast<unsigned long>(s.drop));
    printf(" sd ring high water:%lu / %lu\r\n", static_cast<unsigned long>(s.ring_high_water), static_cast<unsigned long>(sd_logger::RING_SIZE));
    printf("---------------------------\r\n");
}

static void _show_banner()
{
    printf("\r\n---------------------------\r\n");
    printf("  pico_serial_logger ver. %s\r\n", ConfigParam::instance().P_CFG_VERSION.get().c_str());
#if defined(RASPBERRYPI_PICO2_W)
    printf("  board: Pico 2%s\r\n", picoW ? " W" : "");
#else
    printf("  board: Pico%s\r\n", picoW ? " W" : "");
#endif
    _show_help();
    _show_status();
}

// banner is printed a short while after the console terminal connects (DTR asserted),
// giving the host side time to start reading; best effort, depends on the terminal
static void _banner_task()
{
    static bool terminal_connected = false;
    static bool banner_pending = false;
    static uint32_t connected_ms = 0;

    bool connected = stdio_cdc_connected();
    if (connected && !terminal_connected) {
        banner_pending = true;
        connected_ms = _millis();
    }
    terminal_connected = connected;
    if (!connected) banner_pending = false;

    if (banner_pending && (_millis() - connected_ms) >= BANNER_DELAY_MS) {
        banner_pending = false;
        _show_banner();
    }
}

static void _console_task()
{
    int chr = getchar_timeout_us(0);
    if (chr == PICO_ERROR_TIMEOUT) return;

    char c = static_cast<char>(chr);
    if (c == 's') {
        _show_status();
    } else if (c == 'h') {
        _show_help();
    }
    // Discard any input of the rest.
    while (getchar_timeout_us(1) != PICO_ERROR_TIMEOUT) {};
}

static void _led_task()
{
    // | priority | state                              | LED                                   |
    // |----------|------------------------------------|---------------------------------------|
    // | 1        | SD mount error / SD write error    | burst of N blinks (0.2 s) then 1 s off |
    // | 2        | UART activity within last 200 ms   | fast blink (50 ms on / 50 ms off)      |
    // | 3        | idle, SD logging OK                | steady ON                              |
    // USB host presence is intentionally not shown: the device also runs standalone.
    uint32_t ms = _millis();
    bool led;
    sd_logger::state_t sd_state = sd_logger::status().state;
    if (sd_state == sd_logger::state_t::NOT_MOUNTED) {
        led = _led_error_pattern(main_error_t::FATFS_MOUNT_ERROR, ms);
    } else if (sd_state == sd_logger::state_t::ERROR) {
        led = _led_error_pattern(main_error_t::FATFS_WRITE_ERROR, ms);
    } else if (relay::activity_within(LED_ACTIVITY_WINDOW_MS)) {
        led = (ms % LED_ACTIVITY_PERIOD_MS) < (LED_ACTIVITY_PERIOD_MS / 2);
    } else {
        led = true;
    }
    _set_led(led);
}

int main()
{
    stdio_init_all();  // no stdio driver is enabled at build time; console driver is added below
    picoW = _check_pico_w();

    ConfigParam& cfgParam = ConfigParam::instance();
    cfgParam.initialize();
    uint32_t baud = cfgParam.P_CFG_BAUDRATE.get();

    // Pico / Pico W dependencies
    if (picoW) {
        if (cyw43_arch_init()) {  // this is needed for driving LED
            _led_disp_error(main_error_t::CYW43_INIT_ERROR, true);
            return 1;
        }
    } else {
        // LED
        gpio_init(PIN_LED);
        gpio_set_dir(PIN_LED, GPIO_OUT);
    }
    _set_led(false);

    // UART capture starts first: bytes arriving during the SD mount are kept in the RX ring
    uart_port::init(baud);

    // microSD mount and new log file (before USB init: mount retries may take seconds)
    sd_logger::init();

    // USB device (2 x CDC composite)
    tud_init(BOARD_TUD_RHPORT);
    stdio_cdc_init();
    relay::init();

    while (true) {
        tud_task();
        relay::task();
        sd_logger::task();
        _banner_task();
        _console_task();
        _led_task();
        tight_loop_contents();
    }

    return 0;
}
