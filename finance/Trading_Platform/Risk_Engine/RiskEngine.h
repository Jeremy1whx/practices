#pragma once

#include "AccountState.h"
#include "../Order/Order.h"
#include "../Event_Bus/Trade.h"
#include "../Event_Bus/RiskViolationReason.h"
#include "../Latency/Clock.h"
#include "TokenBucket.h"
#include "RiskLimits.h"
#include "OrderLookupTable.h"
#include "../Persistence/SnapshotTool.h"
#include "../Lock_Free_Ring_Buffer/mpsc/Ringbuffer.h"

#include <atomic>
#include <cstring>

namespace exchange {

struct RiskCheckResult {
    bool passed;
    RiskViolationReason reason;

    static RiskCheckResult ok() {
        return {true, RiskViolationReason::None};
    }
    static RiskCheckResult reject(RiskViolationReason r) {
        return {false, r};
    }

    explicit operator bool() const { return passed; }
};

class RiskEngine {
public:
    explicit RiskEngine(uint64_t max_account_id)
        : max_account_id_(max_account_id),
          accounts_(max_account_id),
          buckets_(max_account_id),
          broken_accounts_(max_account_id) {}

    uint64_t register_account(const RiskLimits& limits) {
        if (account_count_ >= max_account_id_) return INVALID_INDEX;
        
        uint64_t idx = account_count_++;
        if (idx >= max_account_id_) return INVALID_INDEX;

        AccountState& acct = accounts_[idx];
        std::memset(&acct, 0, sizeof(acct));
        acct.account_id = idx;
        acct.status = 0;

        set_limits(idx, limits);
        return idx;
    }

    void set_limits(uint64_t account_id, const RiskLimits& limits) {
        if (account_id >= max_account_id_) return;

        AccountState& acct = accounts_[account_id];
        acct.max_order_size = limits.max_order_size;
        acct.max_orders_per_sec = limits.max_orders_per_sec;
        acct.max_position = limits.max_position;
        acct.max_credit = limits.max_credit;
        acct.max_daily_loss = limits.max_daily_loss;
        acct.price_band_pct = limits.price_band_pct;
        acct.stp_policy = static_cast<uint16_t>(limits.stp_policy);

        buckets_[account_id].init(limits.max_orders_per_sec,
                           limits.max_orders_per_sec,
                           now_ns());
    }
    
    RiskCheckResult pre_trade_check(const Order& order, uint64_t now_ns_val) {
        total_checks_.fetch_add(1, std::memory_order_relaxed);

        if (order.account_id >= max_account_id_) [[unlikely]] {
            total_rejects_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::reject(RiskViolationReason::UnknownAccount);
        }
        AccountState& acct = accounts_[order.account_id];

        // check account status
        if (acct.status != 0) [[unlikely]] {
            total_rejects_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::reject(RiskViolationReason::AccountSuspended);
        }

        // check quantity per order
        if (order.quantity > acct.max_order_size) [[unlikely]] {
            total_rejects_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::reject(RiskViolationReason::OrderSizeLimitExceeded);
        }

        // check price band
        double ltp = last_trade_price_.load(std::memory_order_relaxed);
        if (acct.price_band_pct > 0.0 && ltp > 0.0) {
            double dev = std::abs(order.price - ltp) / ltp;
            if (dev > acct.price_band_pct) [[unlikely]] {
                total_rejects_.fetch_add(1, std::memory_order_relaxed);
                return RiskCheckResult::reject(RiskViolationReason::PriceOutOfBand);
            }
        }

        // limit rate
        if (!buckets_[order.account_id].try_consume(now_ns_val)) [[unlikely]] {
            total_rejects_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::reject(RiskViolationReason::RateLimitExceeded);
        }

        // check position
        int64_t signed_qty = (order.side == Side::Buy)
                           ?  static_cast<int64_t>(order.quantity)
                           : -static_cast<int64_t>(order.quantity);
        int64_t projected = acct.position
                          + acct.open_buy_quantity
                          - acct.open_sell_quantity
                          + signed_qty;
        if (projected > acct.max_position || projected < -acct.max_position) [[unlikely]] {
            total_rejects_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::reject(RiskViolationReason::PositionLimitExceeded);
        }

        // check credit
        int64_t credit_per_unit = 0;
        int64_t  total_credit = 0;
        if (order.side == Side::Buy) {            
            credit_per_unit = static_cast<int64_t>(order.price * 100.0 + 0.5); 
            total_credit = credit_per_unit * order.quantity;
            if (acct.credit_used + total_credit > acct.max_credit) [[unlikely]] {
                total_rejects_.fetch_add(1, std::memory_order_relaxed);
                return RiskCheckResult::reject(RiskViolationReason::CreditLimitExceeded);
            }
        }

        // daily loss limit
        if (acct.realized_pnl < -acct.max_daily_loss) [[unlikely]] {
            total_rejects_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::reject(RiskViolationReason::DailyLossLimitExceeded);
        }

        if (order.side == Side::Buy) {
            if (!lookup_.insert(order.order_id, order.account_id, credit_per_unit)) {
                acct.open_buy_quantity -= order.quantity;
                acct.credit_used -= total_credit;
                ++total_rejects_;
                return RiskCheckResult::reject(RiskViolationReason::InternalError);
            }
            acct.open_buy_quantity += order.quantity;
            acct.credit_used += total_credit;
        } else {
            if (!lookup_.insert(order.order_id, order.account_id, 0)) {
                acct.open_sell_quantity -= order.quantity;
                ++total_rejects_;
                return RiskCheckResult::reject(RiskViolationReason::InternalError);
            }
            acct.open_sell_quantity += order.quantity;
        }
        acct.last_order_ns = now_ns_val;

        return RiskCheckResult::ok();
    }    

    void release_reservation(const Order& order) {
        if (order.account_id >= max_account_id_) return;
        AccountState& acct = accounts_[order.account_id];

        const auto* entry = lookup_.find(order.order_id);
        if (!entry) return; 

        if (order.side == Side::Buy) {
            acct.open_buy_quantity -= static_cast<int64_t>(order.quantity);
            acct.credit_used -= entry->credit_per_unit * static_cast<int64_t>(order.quantity);
        } else {
            acct.open_sell_quantity -= static_cast<int64_t>(order.quantity);
        }
        lookup_.erase(order.order_id);
    }
    
    void on_trade(const Trade& trade, uint64_t now_ns_val) {
        last_trade_price_.store(trade.price, std::memory_order_relaxed);

        const auto* buyer_entry  = lookup_.find(trade.buy_order_id);
        const auto* seller_entry = lookup_.find(trade.sell_order_id);

        if (buyer_entry) {
            AccountState& buyer = accounts_[buyer_entry->account_id];
            assert(buyer.open_buy_quantity >= trade.quantity && "Inconsistent state!");
            buyer.cash_flow -= static_cast<int64_t>(trade.price * trade.quantity * 100); 
            buyer.position += static_cast<int64_t>(trade.quantity);
            buyer.open_buy_quantity -= static_cast<int64_t>(trade.quantity);
            buyer.credit_used -= buyer_entry->credit_per_unit * static_cast<int64_t>(trade.quantity);
            buyer.last_update_ns = now_ns_val;
            if (buyer.position == 0) {
                buyer.realized_pnl += buyer.cash_flow;
                buyer.cash_flow     = 0;
            }
        } else if (seller_entry) {
            if (trade.account_id < max_account_id_) {
                AccountState& a = accounts_[trade.account_id];
                a.cash_flow -= static_cast<int64_t>(trade.price * trade.quantity * 100);
                a.position += static_cast<int64_t>(trade.quantity);
                if (a.position == 0) { a.realized_pnl += a.cash_flow; a.cash_flow = 0; }
            }
        }

        if (seller_entry) {
            AccountState& seller = accounts_[seller_entry->account_id];
            seller.cash_flow += static_cast<int64_t>(trade.price * trade.quantity * 100); 
            seller.position -= static_cast<int64_t>(trade.quantity);
            seller.open_sell_quantity -= static_cast<int64_t>(trade.quantity);
            seller.last_update_ns = now_ns_val;
            if (seller.position == 0) {
                seller.realized_pnl += seller.cash_flow;
                seller.cash_flow     = 0;
            }
        } else if (buyer_entry) {
            if (trade.account_id < max_account_id_) {
                AccountState& a = accounts_[trade.account_id];
                a.cash_flow += static_cast<int64_t>(trade.price * trade.quantity * 100);
                a.position -= static_cast<int64_t>(trade.quantity);
                if (a.position == 0) { a.realized_pnl += a.cash_flow; a.cash_flow = 0; }
            }
        }
    }

    void on_order_fully_filled(uint64_t order_id) {
        lookup_.erase(order_id);
    }

    void on_order_removed(uint64_t order_id, uint32_t remaining_qty, Side side) {
        const auto* entry = lookup_.find(order_id);
        if (!entry) return;

        AccountState& acct = accounts_[entry->account_id];
        if (side == Side::Buy) {
            assert(acct.open_buy_quantity >= static_cast<int64_t>(remaining_qty) && "Risk engine state corrupted!");
            acct.open_buy_quantity -= static_cast<int64_t>(remaining_qty);
            acct.credit_used -= entry->credit_per_unit * static_cast<int64_t>(remaining_qty);
        } else {
            assert(acct.open_sell_quantity >= static_cast<int64_t>(remaining_qty) && "Risk engine state corrupted!");
            acct.open_sell_quantity -= static_cast<int64_t>(remaining_qty);
        }

        lookup_.erase(order_id);
    }

    void export_accounts(std::vector<AccountSnapshot>& out) const {
        out.reserve(account_count_);
        for (uint32_t i = 0; i < account_count_; ++i) {
            const AccountState& a = accounts_[i];
            if (a.status == 0 && (a.position != 0 || a.open_buy_quantity != 0 
                                || a.open_sell_quantity != 0 || a.realized_pnl != 0
                                || a.cash_flow != 0)) {
                out.push_back({
                    a.account_id,
                    a.position,
                    a.open_buy_quantity,
                    a.open_sell_quantity,
                    a.credit_used,
                    a.realized_pnl,
                    a.cash_flow, 
                });
            }
        }
    }

    void restore(const std::vector<AccountSnapshot>& accounts, double last_trade_price) {
        clear_state();
        for (const auto& s : accounts) {
            if (s.account_id >= max_account_id_) continue; 
            // std::cout << "ENGINE RESTORE: id=" << s.account_id 
            //       << ", pos=" << s.position 
            //       << ", open_buy=" << s.open_buy_quantity << std::endl;
            uint64_t idx = s.account_id;
            if (idx == INVALID_INDEX) continue;
            AccountState& a = accounts_[idx];
            a.account_id = s.account_id;
            a.position = s.position;
            a.open_buy_quantity = s.open_buy_quantity;
            a.open_sell_quantity = s.open_sell_quantity;
            a.credit_used = s.credit_used;
            a.realized_pnl = s.realized_pnl;
            a.cash_flow = s.cash_flow;
        }
        last_trade_price_.store(last_trade_price, std::memory_order_relaxed);
    }

    void apply_replay_accepted(const Order& order, uint64_t now) {
        uint64_t idx = order.account_id;
        if (idx >= max_account_id_) return;
        AccountState& acct = accounts_[idx];

        if (order.side == Side::Buy) {
            int64_t cpu = static_cast<int64_t>(order.price * 100.0 + 0.5);
            acct.open_buy_quantity += static_cast<int64_t>(order.quantity);
            acct.credit_used += cpu * static_cast<int64_t>(order.quantity);
            lookup_.insert(order.order_id, order.account_id, cpu);
        } else {
            acct.open_sell_quantity += static_cast<int64_t>(order.quantity);
            lookup_.insert(order.order_id, order.account_id, 0);
        }
        acct.last_order_ns = now;
    }

    void apply_replay_trade(const Trade& trade, uint64_t now) {
        last_trade_price_.store(trade.price, std::memory_order_relaxed);

        const auto* buyer_entry = lookup_.find(trade.buy_order_id);
        const auto* seller_entry = lookup_.find(trade.sell_order_id);

        if (buyer_entry) {
            AccountState& a = accounts_[buyer_entry->account_id];
            a.cash_flow -= static_cast<int64_t>(trade.price * trade.quantity * 100.0);
            a.position += static_cast<int64_t>(trade.quantity);
            a.open_buy_quantity -= static_cast<int64_t>(trade.quantity);
            a.credit_used -= buyer_entry->credit_per_unit * static_cast<int64_t>(trade.quantity);
            if (a.position == 0) {
                a.realized_pnl += a.cash_flow;
                a.cash_flow = 0;
            }
        } else if (seller_entry) {
            if (trade.account_id < max_account_id_) {
                AccountState& a = accounts_[trade.account_id];
                a.cash_flow -= static_cast<int64_t>(trade.price * trade.quantity * 100.0);
                a.position += static_cast<int64_t>(trade.quantity);
                if (a.position == 0) { a.realized_pnl += a.cash_flow; a.cash_flow = 0; }
            }
        }

        if (seller_entry) {
            AccountState& a = accounts_[seller_entry->account_id];
            a.cash_flow += static_cast<int64_t>(trade.price * trade.quantity * 100.0);
            a.position -= static_cast<int64_t>(trade.quantity);
            a.open_sell_quantity -= static_cast<int64_t>(trade.quantity);
            if (a.position == 0) {
                a.realized_pnl += a.cash_flow;
                a.cash_flow = 0;
            }
        } else if (buyer_entry) {
            if (trade.account_id < max_account_id_) {
                AccountState& a = accounts_[trade.account_id];
                a.cash_flow += static_cast<int64_t>(trade.price * trade.quantity * 100.0);
                a.position -= static_cast<int64_t>(trade.quantity);
                if (a.position == 0) { a.realized_pnl += a.cash_flow; a.cash_flow = 0; }
            }
        }
    }

    void register_replay_order(const Order& order) {
        int64_t credit_per_uint = (order.side == Side::Buy)
                ? static_cast<int64_t>(order.price * 100.0 + 0.5)
                : 0;
        lookup_.insert(order.order_id, order.account_id, credit_per_uint);
    }

    const AccountState* get_account(uint64_t account_id) const {
        if (account_id >= account_count_) return nullptr;
        return &accounts_[account_id];
    }

    STPPolicy get_stp_policy(uint64_t account_id) const {
        if (account_id >= max_account_id_) return STPPolicy::None;
        return static_cast<STPPolicy>(accounts_[account_id].stp_policy);
    }

    void clear_state() {
        for (uint32_t i = 0; i < account_count_; ++i) {
            AccountState& acct = accounts_[i];
            acct.cash_flow = 0;
            acct.position = 0;
            acct.open_buy_quantity = 0;
            acct.open_sell_quantity = 0;
            acct.credit_used = 0;
            acct.realized_pnl = 0;
            acct.last_order_ns = 0;
        }
        lookup_.clear(); 
        last_trade_price_.store(0.0, std::memory_order_relaxed);
    }

    void reset_daily_pnl() {
        for (uint32_t i = 0; i < account_count_; ++i) {
            accounts_[i].realized_pnl = 0;
        }
    }

    double last_trade_price() const { return last_trade_price_.load(std::memory_order_relaxed); }
    uint64_t total_checks() const { return total_checks_.load(std::memory_order_relaxed); }
    uint64_t total_rejects() const { return total_rejects_.load(std::memory_order_relaxed); }
    uint32_t account_count() const {return account_count_;}

    void set_unrealized_pnl(uint32_t idx, int64_t upnl) {
        if (idx >= account_count_) return; 
        accounts_[idx].unrealized_pnl = upnl;
    }

    bool is_circuit_broken(uint64_t account_id) const {
        if (account_id >= max_account_id_) return false;
        return broken_accounts_[account_id].load(std::memory_order_acquire);
    }

    void set_circuit_breaker(uint64_t account_id, bool broken) {
        if (account_id >= max_account_id_) return;
        broken_accounts_[account_id].store(broken, std::memory_order_release);
    }

    void request_liquidation(uint64_t account_id) {
        liquidate_queue_.push(account_id);
    }

    bool pop_liquidation_request(uint64_t& account_id) {
        return liquidate_queue_.pop(account_id);
    }

private:
    uint64_t max_account_id_;
    uint32_t account_count_ = 0;
    std::vector<AccountState> accounts_;
    std::vector<TokenBucket> buckets_;
    std::vector<std::atomic<bool>> broken_accounts_;
    OrderLookupTable lookup_{2 * 1024 * 1024};
    MPSCRingBuffer<uint64_t> liquidate_queue_{1024};
    std::atomic<double> last_trade_price_{0.0};
    std::atomic<uint64_t> total_checks_{0};
    std::atomic<uint64_t> total_rejects_{0};
};
}