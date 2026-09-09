#pragma once

#include "EventSink.h"

#include <fstream>
#include <filesystem>
#include <mutex>


namespace exchange{
inline std::ostream& operator<<(std::ostream& os, const std::optional<double>& opt) {
            if (opt.has_value()) {
                os << opt.value();
            } else {
                os << "null";
            }
            return os;
};
inline std::ostream& operator<<(std::ostream& os, const std::optional<uint32_t>& opt) {
    if (opt.has_value()) {
        os << opt.value();
    } else {
        os << "null";
    }
    return os;
}

inline std::string_view to_string(CancelReason reason) {
    switch (reason) {
    case CancelReason::FOKNotFilled:
        return "FOKNotFIlled";
    case CancelReason::IOCResidual:
        return "IOCResidual";
    case CancelReason::MarketClosed:
        return "MarketClosed";
    case CancelReason::RiskLiquidation:
        return "RiskLiquidation";
    case CancelReason::SystemShutdown:
        return "SystemShutdown";
    case CancelReason::UserRequest:
        return "UserRequest";
    case CancelReason::Expired:
        return "Expired";    
    default:
        return "Unknown";
    }
}

inline std::string_view to_string(RejectReason reason) {
    switch (reason)
    {
    case RejectReason::DuplicateOrderId:
        return "DuplicateOrderId";
    case RejectReason::InternalError:
        return "InternalError";
    case RejectReason::InvalidPrice:
        return "InvalidPrice";
    case RejectReason::InvalidQuantity:
        return "InvalidQuantity";
    case RejectReason::MarketClosed:
        return "MarketClosed";
    // case RejectReason::RiskViolation:
    //     return "RiskViolation";    
    default:
        return "Unknown";
    }
}
class EventTextWriter {
public:

    explicit EventTextWriter(const std::filesystem::path& path, MPSCRingBuffer<Event>& event_queue) : event_queue_(event_queue){
        journal_.open(path.string(), std::ios::out | std::ios::app);
    }

    void write_events() {
            Event event;
            while (event_queue_.pop(event)) {
                std::lock_guard<std::mutex> lock(journal_mutex_);
                std::visit([this](const auto& e) {
                    write(e);
                }, event);
            }
    }

    void set_event_journal(uint64_t sequence) {
        std::lock_guard<std::mutex> lock(journal_mutex_);
        if (journal_.is_open()) {
            journal_.flush();
            journal_.close();
        } 
        const std::filesystem::path path = "event_journal" + std::to_string(sequence) + ".txt";
        journal_.open(path.string(), std::ios::out | std::ios::app);
    }

private:
    std::ofstream journal_;

    std::mutex journal_mutex_;

    MPSCRingBuffer<Event>& event_queue_;

    void write(const TradeEvent& event) {
        journal_ << "Trade "
            << "timestamp=" << event.header.timestamp_ns 
            << " buy=" << event.trade.buy_order_id
            << " sell=" << event.trade.sell_order_id
            << " price=" << event.trade.price
            << " qty=" << event.trade.quantity << '\n';
    }

    void write(const BookUpdateEvent& event) {
        journal_ << "BookUpdate "
            << "timestamp=" << event.header.timestamp_ns 
            << " best_bid=" << event.update.best_bid
            << " best_ask=" << event.update.best_ask
            << " last_trade_price=" << event.update.last_trade_price
            << " last_trade_qty=" << event.update.last_trade_quantity << '\n';
    }
    
    void write(const OrderAcceptedEvent& event) {
        journal_ << "OrderAccepted "
            << "timestamp=" << event.header.timestamp_ns
            << " order_id=" << event.order_id
            << " order_price=" << event.price
            << " order_quantity=" << event.quantity << '\n';
    };

    void write(const OrderCancelledEvent& event) {
        journal_ << "OrderCanCelled "
            << "timestamp=" << event.header.timestamp_ns
            << " order_id=" << event.order_id
            << " cancel_reason=" << static_cast<int>(event.reason) << '\n';
    }

    void write(const OrderRejectedEvent& event) {
        journal_ << "OrderRejceted "
            << "timestamp= " << event.header.timestamp_ns
            << " order_id=" << event.order_id
            << " reject_reason=" << static_cast<int>(event.reason) << '\n';
    }

    // void write(const RiskViolationEvent& event) {};
};

class EventBinaryWriter {
public:
    explicit EventBinaryWriter(const std::filesystem::path& directory, MPSCRingBuffer<Event>& event_queue) 
        : 
        directory_(directory / "event_journal"),
        event_queue_(event_queue) 
        {
        std::filesystem::create_directories(directory_);
        open_file(0);
    }

    void request_journal_switch(uint64_t sequence) {
        pending_sequence_.store(sequence, std::memory_order_release);
        switch_requested_.store(true, std::memory_order_release);
    }
    
    void write_events() {
        constexpr size_t BATCH_SIZE = 1024;
        std::array<Event, BATCH_SIZE> batch;
        
        size_t count = 0;
        while (count < BATCH_SIZE && event_queue_.pop(batch[count])) {
            ++count;
        }
        
        if (count > 0) {
            std::lock_guard lock(mutex_);
            for (size_t i = 0; i < count; ++i) {
                write_event(batch[i]);
            }
            file_.flush();
        }
        
        check_and_switch_if_needed();
    }    
    
private:
    std::filesystem::path directory_;
    std::ofstream file_;
    std::mutex mutex_;
    MPSCRingBuffer<Event>& event_queue_;
    std::atomic<bool> switch_requested_{false};
    std::atomic<uint64_t> pending_sequence_{0};       

    void open_file(uint64_t sequence) {
        const std::filesystem::path full_path =directory_ / ("event_journal_" + std::to_string(sequence) + ".bin");
        file_.open(full_path.string(), std::ios::out | std::ios::binary | std::ios::app);
    }

    void check_and_switch_if_needed() {
        if (!switch_requested_.load(std::memory_order_acquire)) {
            return;
        }
        
        if (!event_queue_.empty()) {
            return; 
        }
        
        std::lock_guard lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
        
        uint64_t seq = pending_sequence_.load(std::memory_order_acquire);
        open_file(seq);
        switch_requested_.store(false, std::memory_order_release);
    }
        
    void write_event(const Event& event) {
        uint16_t type = static_cast<uint16_t>(event.index());
        file_.write(reinterpret_cast<const char*>(&type), sizeof(type));
        
        std::visit([this](const auto& e) {
            write_event_data(e);
        }, event);
    }
    
    void write_event_data(const TradeEvent& event) {
        struct TradeData {
            EventType type;
            uint64_t timestamp_ns;
            uint64_t buy_order_id;
            uint64_t sell_order_id;
            double price;
            uint64_t quantity;
        };
        
        TradeData data{
            event.header.type,
            event.header.timestamp_ns,
            event.trade.buy_order_id,
            event.trade.sell_order_id,
            event.trade.price,
            event.trade.quantity
        };
        
        file_.write(reinterpret_cast<const char*>(&data), sizeof(data));
    }
    
    void write_event_data(const BookUpdateEvent& event) {
        struct BookData {
            EventType type;
            uint64_t timestamp_ns;
            double best_bid;
            double best_ask;
            double last_trade_price;
            uint64_t last_trade_quantity;
        };
        
        BookData data{
            event.header.type,
            event.header.timestamp_ns,
            event.update.best_bid.value(),
            event.update.best_ask.value(),
            event.update.last_trade_price.value(),
            event.update.last_trade_quantity.value()
        };
        
        file_.write(reinterpret_cast<const char*>(&data), sizeof(data));
    }
    
    void write_event_data(const OrderAcceptedEvent& event) {
        struct OrderData {
            EventType type;
            uint64_t timestamp_ns;
            uint64_t order_id;
            Side side;
            double price;
            uint64_t quantity;
        };
        
        OrderData data{
            event.header.type,
            event.header.timestamp_ns,
            event.order_id,
            event.side,
            event.price,
            event.quantity
        };
        
        file_.write(reinterpret_cast<const char*>(&data), sizeof(data));
    }
    
    void write_event_data(const OrderCancelledEvent& event) {
        struct CancelData {
            EventType type;
            uint64_t timestamp_ns;
            uint64_t order_id;
            uint16_t reason;
        };
        
        CancelData data{
            event.header.type,
            event.header.timestamp_ns,
            event.order_id,
            static_cast<uint16_t>(event.reason)
        };
        
        file_.write(reinterpret_cast<const char*>(&data), sizeof(data));
    }
    
    void write_event_data(const OrderRejectedEvent& event) {
        struct RejectData {
            EventType type;
            uint64_t timestamp_ns;
            uint64_t order_id;
            uint16_t reason;
        };
        
        RejectData data{
            event.header.type,
            event.header.timestamp_ns,
            event.order_id,
            static_cast<uint16_t>(event.reason)
        };
        
        file_.write(reinterpret_cast<const char*>(&data), sizeof(data));
    }
};
}