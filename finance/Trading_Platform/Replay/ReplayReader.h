#pragma once

#include <fstream>
#include <filesystem>
#include <iostream>

#include "../Event_Bus/Event/Event.h"

namespace exchange{

class ReplayReader {
public:
    virtual ~ReplayReader() = default;
    virtual bool next(Event&) = 0;
};

class TextReader : public ReplayReader{
public:
    explicit TextReader(const std::filesystem::path& path) {
        event_journal_.open(path.string(), std::ios::in);
    };

    bool next(Event& event) {
        std::string line;

        if(!std::getline(event_journal_, line)) return false;

        if(line.starts_with("Trade")) return parse_trade(line,event);

        if(line.starts_with("BookUpdate")) return parse_book_update(line,event);

        if(line.starts_with("OrderCancelled")) return parse_order_cancelled(line,event);

        if(line.starts_with("OrderRejected")) return parse_order_rejected(line,event);

        return false;
    };

private:
    std::ifstream event_journal_;

    bool parse_trade(const std::string& line, Event& event) {
        uint64_t timestamp;
        uint64_t buy;
        uint64_t sell;
        double price;
        uint32_t qty;

        sscanf(
            line.c_str(),
            "Trade timestamp=%lu buy=%lu sell=%lu price=%lf qty=%u",
            &timestamp,
            &buy,
            &sell,
            &price,
            &qty
        );


        Trade trade;

        trade.buy_order_id = buy;
        trade.sell_order_id = sell;
        trade.price = price;
        trade.quantity = qty;

        event = TradeEvent{
            EventHeader{
                EventType::Trade,
                timestamp
            },
            trade
        };

        return true;
    };


    bool parse_book_update(const std::string& line, Event& event) {
        uint64_t timestamp;
        double best_bid;
        double best_ask;
        double last_trade_price;
        uint32_t last_trade_qty;

        int match = sscanf(
            line.c_str(),
            "BookUpdate timestamp=%lu best_bid=%lf best_ask=%lf last_trade_price=%lf last_trade_qty=%u",
            &timestamp,
            &best_bid,
            &best_ask,
            &last_trade_price,
            &last_trade_qty
        );

        if (match != 5) {
            std::cout << "didn't match" << std::endl;
            return false;
        }

        BookUpdate update;

        update.best_bid = best_bid;
        update.best_ask = best_ask;
        update.last_trade_price = last_trade_price;
        update.last_trade_quantity = last_trade_qty;

        event = BookUpdateEvent{
            EventHeader{
                EventType::BookUpdate,
                timestamp
            },
            update
        };

        return true;
    };
    

    bool parse_order_cancelled(const std::string& line, Event event) {
        uint64_t timestamp;
        uint64_t order_id;
        int r;

        sscanf(
            line.c_str(),
            "OrderCancelled timestamp=%lu order_id=%lu cancel_reason=%d",
            &timestamp,
            &order_id,
            &r
        );

        CancelReason reason = static_cast<CancelReason>(r);

        event = OrderCancelledEvent{
            EventHeader{
                EventType::OrderCancelled,
                timestamp
            },
            order_id,
            reason
        };

        return true;
    }

    bool parse_order_rejected(const std::string& line, Event event){
        uint64_t timestamp;
        uint64_t order_id;
        int r;

        sscanf(
            line.c_str(),
            "OrderRejected timestamp=%lu order_id=%lu reject_reason=%d",
            &timestamp,
            &order_id,
            &r
        );

        RejectReason reason = static_cast<RejectReason>(r);

        event = OrderRejectedEvent{
            EventHeader{
                EventType::OrderRejected,
                timestamp
            },
            order_id,
            reason
        };

        return true;
    }
};}