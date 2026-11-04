#pragma once

#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <fstream>

#include "RiskEngine.h"
#include "../Match_Engine/MatchingEngine.h"
#include "../Event_Bus/EventBus.h"

namespace exchange {

enum class CircuitBreakerReason : uint8_t {
    None = 0,
    DailyLossLimit  = 1,
    GlobalExposure  = 2,
    Manual          = 3,
};

class RiskMonitor {
public:
    RiskMonitor(RiskEngine& risk, std::chrono::milliseconds interval = std::chrono::milliseconds(100))
        : risk_(risk), interval_(interval) {}

    void start() {
        running_ = true;
        thread_ = std::thread([this] {
            while (running_) {
                uint32_t n = risk_.account_count();
                for (uint32_t i = 0; i < n; ++i) {
                    const auto* a = risk_.get_account(i);
                    if (a && a->realized_pnl < -a->max_daily_loss
                        && !risk_.is_circuit_broken(i)) {
                        risk_.set_circuit_breaker(i, true);
                        risk_.request_liquidation(i);
                    }
                }
                std::this_thread::sleep_for(interval_);
            }
        });
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

    void check_once() {
        uint32_t n = risk_.account_count();
        for (uint32_t i = 0; i < n; ++i) {
            const auto* a = risk_.get_account(i);
            if (a && a->realized_pnl < -a->max_daily_loss
                && !risk_.is_circuit_broken(i)) {
                risk_.set_circuit_breaker(i, true);
                risk_.request_liquidation(i);
            }
        }
    }

private:
    RiskEngine& risk_;
    std::chrono::milliseconds interval_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};
}