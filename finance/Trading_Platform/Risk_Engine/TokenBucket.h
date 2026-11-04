#pragma once
#include <cstdint>
#include <algorithm>

struct alignas(64) TokenBucket {
    uint64_t last_refill_ns;
    uint32_t tokens;         
    uint32_t max_tokens;   
    uint32_t refill_per_sec; 

    void init(uint32_t max, uint32_t rate, uint64_t now) {
        max_tokens = max;
        tokens = max;
        refill_per_sec = rate;
        last_refill_ns = now;
    }

    bool try_consume(uint64_t now_ns) {
        uint64_t elapsed = now_ns - last_refill_ns;
        if (elapsed >= 1'000'000'000ULL) {
            tokens = max_tokens;
            last_refill_ns = now_ns;
        } else {
            uint64_t refill = elapsed * refill_per_sec / 1'000'000'000ULL;
            if (refill > 0) {
                uint32_t cap = max_tokens - tokens;
                tokens += static_cast<uint32_t>(std::min<uint64_t>(refill, cap));
                last_refill_ns = now_ns;
            }
        }
        if (tokens == 0) return false;
        --tokens;
        return true;
    }
};