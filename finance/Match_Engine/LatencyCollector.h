#pragma once

#include <vector>
#include <algorithm>
#include <cstdint>

class LatencyCollector {
public:

    void add(uint64_t latency_ns) {
        samples_.push_back(latency_ns);
    }

    uint64_t p50() const {
        return percentile(0.50);
    }

    uint64_t p99() const {
        return percentile(0.99);
    }

    uint64_t max() const {
        return *std::max_element(samples_.begin(), samples_.end());
    }

    size_t size() const {

        return samples_.size();
    }

private:

    uint64_t percentile(double p) const {

        if (samples_.empty()) {return 0;}

        std::vector<uint64_t> sorted = samples_;

        std::sort(sorted.begin(), sorted.end());

        size_t idx = static_cast<size_t>(p * sorted.size());

        idx = std::min(idx, sorted.size() - 1);

        return sorted[idx];
    }

    std::vector<uint64_t> samples_;
};