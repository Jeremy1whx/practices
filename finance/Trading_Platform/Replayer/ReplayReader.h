#pragma once

#include "../Event_Bus/EventBus.h"

#include <filesystem>
#include <optional>

namespace exchange{

class TextReader{
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
};

struct OrderSnapshot {
        uint64_t order_id;
        Side side;
        double price;
        uint64_t quantity;
        uint64_t sequence;
        Type type;
        uint64_t expire_time;
    };

    struct SnapshotBatch {
        uint64_t snapshot_time = 0;
        uint64_t sequence = 0;
        std::vector<OrderSnapshot> orders;
    };  

class BinaryReader {
public:
    explicit BinaryReader(const std::filesystem::path directory) : directory_(directory) {}

    std::optional<uint64_t> get_latest_snapshot_sequence() const {
        uint64_t max_seq = 0;
        bool found = false;
        for (const auto& entry : std::filesystem::directory_iterator(directory_ / "snapshot")) {
            if (!entry.is_regular_file()) continue;
            auto filename = entry.path().filename().string();
            if (filename.find("snapshot_") != 0) continue;
            size_t start = filename.find('_') + 1;
            size_t end = filename.find(".bin", start);
            if (end == std::string::npos) continue;
        
            uint64_t seq = std::stoull(filename.substr(start, end - start));
            if (!found || seq > max_seq) { max_seq = seq; found = true; }
        }
        return found ? std::optional<uint64_t>(max_seq) : std::nullopt;
    }

    bool open_snapshot(uint64_t sequence) {
        auto path = directory_ / "snapshot" / ("snapshot_" + std::to_string(sequence) + ".bin");
        if (!std::filesystem::exists(path)) return false;
        snapshot_.open(path.string(), std::ios::binary);
        return snapshot_.is_open();
    }

    bool open_event_journal(uint64_t sequence) {
        auto path = directory_ / "event_journal" / ("event_journal_" + std::to_string(sequence) + ".bin");
        if (!std::filesystem::exists(path)) return false;
        event_journal_.open(path.string(), std::ios::binary);
        return event_journal_.is_open();
    }

    bool read_event(Event& event) {
        uint16_t type;
        if (!event_journal_.read(reinterpret_cast<char*>(&type), sizeof(type))) return false;
        switch (static_cast<EventType>(type)) {
            case EventType::Trade:          return parse_trade(event);
            case EventType::BookUpdate:     return parse_book_update(event);
            case EventType::OrderAccepted:  return parse_order(event);
            case EventType::OrderCancelled: return parse_cancel(event);
            case EventType::OrderRejected:  return parse_reject(event);
            default: return false;
        }
    }

    bool read_snapshot(SnapshotBatch& batch) {
        struct SnapshotHeader {
            uint32_t magic;
            uint32_t version;
            uint64_t timestamp;
            uint64_t sequence;
            uint64_t order_count;
            uint64_t reserved;
        };
        SnapshotHeader header;
        if (!snapshot_.read(reinterpret_cast<char*>(&header), sizeof(header))) return false;
        if (header.magic != 0x534E5053 || header.version != 1) return false;
        batch.snapshot_time = header.timestamp;
        batch.sequence = header.sequence;
        batch.orders.resize(header.order_count);
        if (!snapshot_.read(reinterpret_cast<char*>(batch.orders.data()),
                            header.order_count * sizeof(OrderSnapshot))) return false;
        return true;
    }

    void close_event() {
        if (event_journal_.is_open()) event_journal_.close();
    }

    void close_snapshot() {
        if (snapshot_.is_open()) snapshot_.close();}

    
    
private:
    std::filesystem::path directory_;
    std::ifstream event_journal_;
    std::ifstream snapshot_;  

    bool parse_trade(Event& event) {
        struct TradeData {
            EventType type;
            uint64_t timestamp_ns;
            uint64_t buy_order_id;
            uint64_t sell_order_id;
            double price;
            uint64_t quantity;
        };
        TradeData data;
        event_journal_.read(reinterpret_cast<char*>(&data), sizeof(data));
        Trade trade;
        trade.buy_order_id = data.buy_order_id;
        trade.sell_order_id = data.sell_order_id;
        trade.price = data.price;
        trade.quantity = data.quantity;
        TradeEvent ev{ {EventType::Trade, data.timestamp_ns}, trade };
        event = ev;
        return true;
    }

    bool parse_book_update(Event& event) {
        struct BookData {
            EventType type;
            uint64_t timestamp_ns;
            double best_bid;
            double best_ask;
            double last_trade_price;
            uint64_t last_trade_quantity;
        };
        BookData data;
        event_journal_.read(reinterpret_cast<char*>(&data), sizeof(data));
        BookUpdate update;
        update.best_ask = data.best_ask;
        update.best_bid = data.best_bid;
        update.last_trade_price = data.last_trade_price;
        update.last_trade_quantity = data.last_trade_quantity;
        BookUpdateEvent ev{ {EventType::BookUpdate, data.timestamp_ns}, update };
        event = ev;
        return true;
    }
    
    bool parse_order(Event& event) {
        struct OrderData {
            EventType type;
            uint64_t timestamp_ns;
            uint64_t order_id;
            Side side;
            double price;
            uint64_t quantity;
        };
        OrderData data;
        event_journal_.read(reinterpret_cast<char*>(&data), sizeof(data));        
        OrderAcceptedEvent ev{ {EventType::OrderAccepted, data.timestamp_ns}, data.order_id, data.side, data.price, data.quantity};
        event = ev;
        return true;
    }

    bool parse_cancel(Event& event) {
        struct CancelData {
            EventType type;
            uint64_t timestamp_ns;
            uint64_t order_id;
            uint16_t reason;
        };
        CancelData data;
        event_journal_.read(reinterpret_cast<char*>(&data), sizeof(data));        
        OrderCancelledEvent ev{ {EventType::OrderCancelled, data.timestamp_ns}, data.order_id, static_cast<CancelReason>(data.reason)};
        event = ev;
        return true;
    }

    bool parse_reject(Event& event) {
        struct RejectData {
            EventType type;
            uint64_t timestamp_ns;
            uint64_t order_id;
            uint16_t reason;
        };
        RejectData data;
        event_journal_.read(reinterpret_cast<char*>(&data), sizeof(data));        
        OrderRejectedEvent ev{ {EventType::OrderRejected, data.timestamp_ns}, data.order_id, static_cast<RejectReason>(data.reason)};
        event = ev;
        return true;
    }
};
}