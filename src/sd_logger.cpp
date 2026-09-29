/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "sd_logger.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "pico/stdlib.h"

#include "ff.h"
#include "tf_card.h"

#include "ring_buffer.h"

namespace sd_logger
{
    static constexpr const char* LOG_PREFIX = "serial_";
    static constexpr const char* LOG_EXT = ".log";
    static constexpr const char* LOG_NAME_FMT = "serial_%03u.log";
    static constexpr int OPEN_RETRY = 10;

    static FATFS _fs;  // must outlive f_mount(): FatFs keeps a pointer to it
    static FIL _fil;
    static RingBuffer<RING_SIZE> _ring;

    static state_t _state = state_t::NOT_MOUNTED;
    static bool _mounted = false;
    static const char* _fs_type = "";
    static float _capacity_gb = 0.0f;
    static char _filename[32] = "";
    static uint64_t _written_bytes = 0;
    static uint32_t _drop = 0;
    static int _last_fr = FR_OK;
    static const char* _last_op = "";

    static bool _dirty = false;
    static uint32_t _last_push_ms = 0;
    static uint32_t _last_sync_ms = 0;

    static inline uint32_t _millis()
    {
        return to_ms_since_boot(get_absolute_time());
    }

    static void _set_error(const char* op, FRESULT fr)
    {
        _state = state_t::ERROR;
        _last_op = op;
        _last_fr = fr;
        printf("ERROR: SD %s failed (FRESULT %d). logging stopped\r\n", op, fr);
    }

    static bool _mount()
    {
        pico_fatfs_spi_config_t config = {
            spi0,
            CLK_SLOW_DEFAULT,
            CLK_FAST_DEFAULT,
            PIN_SPI0_MISO_DEFAULT,
            PIN_SPI0_CS_DEFAULT,
            PIN_SPI0_SCK_DEFAULT,
            PIN_SPI0_MOSI_DEFAULT,
            true  // use internal pullup
        };
        pico_fatfs_set_config(&config);

        FRESULT fr = FR_NOT_READY;
        for (int i = 0; i < MOUNT_RETRY; i++) {
            fr = f_mount(&_fs, "", 1);
            if (fr == FR_OK) break;
            pico_fatfs_reboot_spi();
        }
        if (fr != FR_OK) {
            _last_op = "mount";
            _last_fr = fr;
            printf("ERROR: SD mount failed (FRESULT %d)\r\n", fr);
            return false;
        }

        _fs_type = (_fs.fs_type == FS_FAT12) ? "FAT12" :
                   (_fs.fs_type == FS_FAT16) ? "FAT16" :
                   (_fs.fs_type == FS_FAT32) ? "FAT32" :
                   (_fs.fs_type == FS_EXFAT) ? "exFAT" : "unknown";
        _capacity_gb = static_cast<float>(_fs.csize) * static_cast<float>(_fs.n_fatent) * 512e-9f;
        _mounted = true;
        printf("SD mount ok: %s %7.2f GB (GB = 1E9 Bytes)\r\n", _fs_type, _capacity_gb);
        return true;
    }

    // scan root directory for serial_NNN.log and return max(NNN) + 1 (1 if none)
    static unsigned _next_suffix()
    {
        DIR dir;
        FILINFO fno;
        unsigned max_suffix = 0;
        const size_t prefix_len = strlen(LOG_PREFIX);

        FRESULT fr = f_opendir(&dir, "");
        if (fr != FR_OK) {
            printf("WARNING: f_opendir failed (FRESULT %d)\r\n", fr);
            return 1;
        }
        while (true) {
            fr = f_readdir(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == '\0') break;
            if (fno.fattrib & AM_DIR) continue;
            if (strncasecmp(fno.fname, LOG_PREFIX, prefix_len) != 0) continue;
            const char* digits = fno.fname + prefix_len;
            char* end;
            unsigned long num = strtoul(digits, &end, 10);
            if (end == digits) continue;                 // no digits
            if (strcasecmp(end, LOG_EXT) != 0) continue; // remainder must be ".log"
            if (num + 1 > max_suffix) max_suffix = static_cast<unsigned>(num + 1);
        }
        f_closedir(&dir);
        return (max_suffix == 0) ? 1 : max_suffix;
    }

    static bool _open_new_file()
    {
        unsigned suffix = _next_suffix();
        for (int i = 0; i < OPEN_RETRY; i++, suffix++) {
            snprintf(_filename, sizeof(_filename), LOG_NAME_FMT, suffix);
            FRESULT fr = f_open(&_fil, _filename, FA_WRITE | FA_CREATE_NEW);
            if (fr == FR_OK) {
                printf("log file: %s\r\n", _filename);
                return true;
            }
            if (fr != FR_EXIST) {
                _filename[0] = '\0';
                _set_error("open", fr);
                return false;
            }
        }
        _filename[0] = '\0';
        _set_error("open", FR_EXIST);
        return false;
    }

    bool init()
    {
        _state = state_t::NOT_MOUNTED;
        if (!_mount()) return false;
        if (!_open_new_file()) return false;
        _state = state_t::OPEN;
        _last_sync_ms = _millis();
        return true;
    }

    uint32_t push(const uint8_t* p, uint32_t n)
    {
        if (_state != state_t::OPEN) return n;  // nothing to log into: discard silently (not counted as drop)
        uint32_t accepted = _ring.push(p, n);
        _drop += n - accepted;
        if (accepted > 0) _last_push_ms = _millis();
        return accepted;
    }

    // write up to max_bytes from the ring (handles wrap-around with two f_write calls)
    static bool _write_from_ring(uint32_t max_bytes)
    {
        uint32_t remain = max_bytes;
        while (remain > 0) {
            const uint8_t* p;
            uint32_t n = _ring.peek_contiguous(&p);
            if (n == 0) break;
            if (n > remain) n = remain;
            UINT bw = 0;
            FRESULT fr = f_write(&_fil, p, n, &bw);
            if (fr != FR_OK) {
                _set_error("write", fr);
                return false;
            }
            if (bw != n) {
                _set_error("write (disk full?)", FR_DENIED);
                return false;
            }
            _ring.consume(n);
            _written_bytes += n;
            remain -= n;
            _dirty = true;
        }
        return true;
    }

    void task()
    {
        if (_state != state_t::OPEN) return;

        uint32_t now = _millis();
        uint32_t count = _ring.count();

        if (count >= WRITE_UNIT) {
            uint32_t n = count & ~(WRITE_UNIT - 1);
            if (n > WRITE_MAX_PER_CALL) n = WRITE_MAX_PER_CALL;
            if (!_write_from_ring(n)) return;
        } else if (count > 0 && (now - _last_push_ms) >= IDLE_FLUSH_MS) {
            if (!_write_from_ring(count)) return;
        }

        if (_dirty && (now - _last_sync_ms) >= SYNC_INTERVAL_MS) {
            FRESULT fr = f_sync(&_fil);
            if (fr != FR_OK) {
                _set_error("sync", fr);
                return;
            }
            _dirty = false;
            _last_sync_ms = now;
        }
    }

    status_t status()
    {
        status_t s;
        s.state = _state;
        s.mounted = _mounted;
        s.fs_type = _fs_type;
        s.capacity_gb = _capacity_gb;
        s.filename = _filename;
        s.written_bytes = _written_bytes;
        s.pending = _ring.count();
        s.drop = _drop;
        s.ring_high_water = _ring.high_water();
        s.last_fr = _last_fr;
        s.last_op = _last_op;
        return s;
    }

    const char* state_str(state_t state)
    {
        switch (state) {
            case state_t::NOT_MOUNTED: return "not mounted";
            case state_t::OPEN:        return "logging";
            case state_t::ERROR:       return "error";
            default:                   return "unknown";
        }
    }
}
