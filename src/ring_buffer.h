/*------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "hardware/sync.h"

// Lock-free single-producer / single-consumer byte ring buffer.
// Indices are free-running 32-bit counters (count = head - tail), so N must be a power of 2 and < 2^31.
// The producer may run in an IRQ handler while the consumer runs in the main loop.
template <size_t N>
class RingBuffer
{
    static_assert(N > 0 && (N & (N - 1)) == 0, "RingBuffer size must be a power of 2");
    static_assert(N < (1UL << 31), "RingBuffer size too large");

public:
    RingBuffer() : _head(0), _tail(0), _high_water(0) {}

    static constexpr uint32_t capacity() { return static_cast<uint32_t>(N); }
    uint32_t count() const { return _head - _tail; }
    uint32_t free() const { return capacity() - count(); }

    // ---- producer side ----
    // push up to n bytes, returns the number of bytes accepted
    uint32_t push(const uint8_t* p, uint32_t n)
    {
        uint32_t f = free();
        if (n > f) n = f;
        if (n == 0) return 0;
        uint32_t head = _head;
        uint32_t idx = head & MASK;
        uint32_t first = capacity() - idx;
        if (first > n) first = n;
        memcpy(&_buf[idx], p, first);
        if (n > first) memcpy(&_buf[0], p + first, n - first);
        __dmb();
        _head = head + n;
        uint32_t c = count();
        if (c > _high_water) _high_water = c;
        return n;
    }

    bool push(uint8_t b)
    {
        if (free() == 0) return false;
        uint32_t head = _head;
        _buf[head & MASK] = b;
        __dmb();
        _head = head + 1;
        uint32_t c = count();
        if (c > _high_water) _high_water = c;
        return true;
    }

    // ---- consumer side ----
    // returns pointer to the contiguous readable region and its length (0 if empty)
    uint32_t peek_contiguous(const uint8_t** p) const
    {
        uint32_t c = count();
        uint32_t idx = _tail & MASK;
        uint32_t n = capacity() - idx;
        if (n > c) n = c;
        *p = &_buf[idx];
        return n;
    }

    void consume(uint32_t n)
    {
        __dmb();
        _tail = _tail + n;
    }

    uint32_t pop(uint8_t* p, uint32_t n)
    {
        uint32_t total = 0;
        while (total < n) {
            const uint8_t* src;
            uint32_t avail = peek_contiguous(&src);
            if (avail == 0) break;
            uint32_t take = n - total;
            if (take > avail) take = avail;
            memcpy(p + total, src, take);
            consume(take);
            total += take;
        }
        return total;
    }

    uint32_t high_water() const { return _high_water; }
    void reset_high_water() { _high_water = count(); }

private:
    static constexpr uint32_t MASK = static_cast<uint32_t>(N - 1);
    uint8_t _buf[N];
    volatile uint32_t _head;
    volatile uint32_t _tail;
    volatile uint32_t _high_water;
};
