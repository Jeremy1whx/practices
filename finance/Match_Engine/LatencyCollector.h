#pragma once

#include <vector>
#include <algorithm>
#include <cstdint>

class LatencyCollector {
public:

    void add(uint64_t total, uint64_t queue, uint64_t match) {
        total_latencies_.push_back(total);
        queue_latencies_.push_back(queue);
        match_durations_.push_back(match);

        max_total_ = std::max(max_total_, total);
        max_queue_ = std::max(max_queue_, queue);
        max_match_ = std::max(max_match_, match);
    }

    uint64_t p50_total() const { return percentile(total_latencies_, 0.50); }
    uint64_t p99_total() const { return percentile(total_latencies_, 0.99); }
    
    uint64_t p50_queue() const { return percentile(queue_latencies_, 0.50); }
    uint64_t p99_queue() const { return percentile(queue_latencies_, 0.99); }
    
    uint64_t p50_match() const { return percentile(match_durations_, 0.50); }
    uint64_t p99_match() const { return percentile(match_durations_, 0.99); }
    
    uint64_t max_total() const { return max_total_; }
    uint64_t max_queue() const { return max_queue_; }
    uint64_t max_match() const { return max_match_; }

    size_t size() const {

        return total_latencies_.size();
    }

private:

    uint64_t percentile(const std::vector<uint64_t>& vec, double p) const {

        if (vec.empty()) {return 0;}

        std::vector<uint64_t> sorted = vec;

        std::sort(sorted.begin(), sorted.end());

        size_t idx = static_cast<size_t>(p * sorted.size());

        idx = std::min(idx, sorted.size() - 1);

        return sorted[idx];
    }

    std::vector<uint64_t> total_latencies_;
    std::vector<uint64_t> queue_latencies_;
    std::vector<uint64_t> match_durations_;

    uint64_t max_total_ = 0;
    uint64_t max_queue_ = 0;
    uint64_t max_match_ = 0;
};