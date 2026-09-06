#pragma once

// #include "PriceLevel.h"
// #include "Trade.h"
// #include "TradeFormatter.h"
// #include "MarketDataEvent.h"
#include "../Latency/Clock.h"
#include "../Latency/LatencyCollector.h"
// #include "../Trade_Event/TradeEventListener.h"
// #include "../Trade_Event/TradeLogger.h"
// #include "../Trade_Event/TradeDataPublisher.h"
#include "../Order/PriceLevel.h"
#include "../Memory_Pool/LRU.h"
#include "../Async_Logger/AsyncLogger.h"
#include "../CPU_Affinity/Affinity.h"
#include "../Event_Bus/EventPublisher.h"
#include "ExpiryScheduler.h"

#include <map>
#include <vector>
#include <unordered_map>
#include <iostream>

namespace exchange {
class MatchingEngine {
public:
    explicit MatchingEngine(EventPublisher& publisher, ExpiryScheduler& scheduler);
    void process_order(Order& order);
    void process_order(const Order& order){
        Order copy = order;
        return process_order(copy);
    };

    const auto& bids() const { return bids_; }

    const auto& asks() const { return asks_; }

    const auto& trades() const {return trades_;}

    const auto& order_lookup() const {return order_lookup_;}

    // const auto& market_data_events() const {return listener_->market_data_events();}

    const auto& latency_collector() const {return latency_collector_;}

    const auto& trade_count() const {return trade_count_;}

    // void set_listener(TradeEventListener* listener) {listener_ = listener;};

    // void set_logger(AsyncLogger* logger) {listener_->set_logger(logger);}

    // void publish_market_data(const Trade& trade);

    bool cancel_order(uint64_t order_id, CancelReason reason);

    void process_expiry(uint64_t current_time) {
        expiry_scheduler_.proceed_expiry(current_time);
    }

private:

    struct PriceLevelWithPrice {
        double price;
        uint32_t avaliable = 0;
        PriceLevel level;
    };

    struct BidCompare {
        bool operator()(const PriceLevelWithPrice& a, const PriceLevelWithPrice& b) const {
            return a.price > b.price;
        }
        bool operator()(const PriceLevelWithPrice& a, double price) const {
            return a.price > price;
        }
        bool operator()(double price, const PriceLevelWithPrice& a) const {
            return price > a.price;
        }
        bool operator()(double a, double b) const {
            return a > b;
        }
    };
    
    struct AskCompare {
        bool operator()(const PriceLevelWithPrice& a, const PriceLevelWithPrice& b) const {
            return a.price < b.price;
        }
        bool operator()(const PriceLevelWithPrice& a, double price) const {
            return a.price < price;
        }
        bool operator()(double price, const PriceLevelWithPrice& a) const {
            return price < a.price;
        }
        bool operator()(double a, double b) const {
            return a < b;
        }
    };

    std::vector<PriceLevelWithPrice> bids_;

    std::vector<PriceLevelWithPrice> asks_;

    std::vector<Trade> trades_;

    // std::vector<MarketDataEvent> market_data_events_;

    // AsyncLogger* logger_ = nullptr;

    EventPublisher& publisher_;

    LatencyCollector latency_collector_;

    LRUPool<Trade> trade_pool_{1024 * 1024};

    MemoryPool<Order> order_pool_{1024 * 1024};

    std::unordered_map<uint64_t, Order*> order_lookup_;

    std::atomic<uint64_t> trade_count_{0};

    ExpiryScheduler& expiry_scheduler_;

    template<typename Container, typename Compare>
    typename Container::iterator find_price_level(Container& container, double price);
    
    template<typename Container, typename Compare>
    typename Container::const_iterator find_price_level(const Container& container, double price) const;

    template<typename Container, typename Compare>
    void insert_price_level(Container& container, double price, uint32_t avaliable, PriceLevel level);
    
    template<typename Container, typename Compare>
    bool remove_price_level(Container& container, double price);

    void match_buy(Order& order);

    void match_sell(Order& order);

    void add_to_book(Order& order);

    void append_order(PriceLevel& level, Order* order);

    void remove_order(PriceLevel& level, Order* order);
    
    bool all_matched_buy(Order& order);

    bool all_matched_sell(Order& order);
};
}
