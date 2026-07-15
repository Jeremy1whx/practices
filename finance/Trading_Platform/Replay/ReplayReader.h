#pragma once

#include <fstream>
#include <filesystem>
#include <iostream>

#include "../Event_Bus/Event/Event.h"

namespace exchange{
class ReplayReader{
public:
    explicit ReplayReader(const std::filesystem::path& path) {
        journal_.open(path.string(), std::ios::in);
    };

    bool next(Event& event) {
        std::string line;

        if(!std::getline(journal_, line)) return false;

        if(line.starts_with("Trade")) return parse_trade(line,event);

        if(line.starts_with("BookUpdate")) return parse_book_update(line,event);

        return false;
    };

private:
    std::ifstream journal_;

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
};}