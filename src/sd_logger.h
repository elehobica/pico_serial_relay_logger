/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#include <cstdint>

// Raw dump of relayed bytes to a new file on microSD at every boot (serial_NNN.log).
// push() is the producer (main loop, relay), task() is the consumer (main loop) doing FatFs I/O.
namespace sd_logger
{
    static constexpr uint32_t RING_SIZE = 32768;       // ~2.8 s at 115200 bps
    static constexpr uint32_t WRITE_UNIT = 512;        // sector aligned writes
    static constexpr uint32_t WRITE_MAX_PER_CALL = 4096;
    static constexpr uint32_t IDLE_FLUSH_MS = 200;     // write the sub-sector tail after this idle time
    static constexpr uint32_t SYNC_INTERVAL_MS = 1000; // f_sync() at most once per this interval
    static constexpr int MOUNT_RETRY = 5;

    enum class state_t {
        NOT_MOUNTED = 0,  // mount failed (or not initialized yet)
        OPEN,             // log file open, logging
        ERROR             // write / sync error occurred, logging stopped
    };

    struct status_t {
        state_t state;
        bool mounted;
        const char* fs_type;
        float capacity_gb;
        const char* filename;   // "" if no file
        uint64_t written_bytes;
        uint32_t pending;       // bytes in ring not yet written
        uint32_t drop;          // bytes dropped because ring was full
        uint32_t ring_high_water;
        int last_fr;            // last FRESULT on error
        const char* last_op;    // FatFs operation that failed
    };

    // mount microSD and open a new log file. returns true if logging is ready.
    bool init();
    // enqueue bytes to be written. returns the number of bytes accepted.
    uint32_t push(const uint8_t* p, uint32_t n);
    // consumer: perform pending writes / sync. call from the main loop.
    void task();
    status_t status();
    const char* state_str(state_t state);
}
