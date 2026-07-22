#pragma once

#include <fstream>

#include "../Event_Bus/Subscriber.h"
#include <filesystem>


namespace exchange{
std::ostream& operator<<(std::ostream& os, const std::optional<double>& opt) {
            if (opt.has_value()) {
                os << opt.value();
            } else {
                os << "null";
            }
            return os;
};
std::ostream& operator<<(std::ostream& os, const std::optional<uint32_t>& opt) {
    if (opt.has_value()) {
        os << opt.value();
    } else {
        os << "null";
    }
    return os;
}

std::string_view to_string(CancelReason reason) {
    switch (reason)
    {
    case CancelReason::FOKNotFilled:
        return "FOKNotFIlled";
    case CancelReason::IOCResidual:
        return "IOCResidual";
    case CancelReason::MarketClosed:
        return "MarketClosed";
    // case CancelReason::RiskLiquidation:
    //     return "RiskLiquidation";
    case CancelReason::SystemShutdown:
        return "SystemShutdown";
    case CancelReason::UserRequest:
        return "UserRequest";    
    default:
        return "Unknown";
    }
}

std::string_view to_string(RejectReason reason) {
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
class TextWriter : public Subscriber {
public:

    explicit TextWriter(const std::filesystem::path& path){
        journal_.open(path.string(), std::ios::out | std::ios::app);
    };

    void on_event(const Event& event) override {
        std::visit([this](const auto& e) {
            write(e);
        }, event);
    };

    bool interested_in(EventType) const override {return true;};

    const char* name() const override {return "Persistence";};

private:
    std::ofstream journal_;

    void write(const TradeEvent& event) {
        journal_ << "Trade "
            << "timestamp=" << event.header.timestamp_ns 
            << " buy=" << event.trade.buy_order_id
            << " sell=" << event.trade.sell_order_id
            << " price=" << event.trade.price
            << " qty=" << event.trade.quantity << std::endl;
    }

    void write(const BookUpdateEvent& event) {
        journal_ << "BookUpdate "
            << "timestamp=" << event.header.timestamp_ns 
            << " best_bid=" << event.update.best_bid
            << " best_ask=" << event.update.best_ask
            << " last_trade_price=" << event.update.last_trade_price
            << " last_trade_qty=" << event.update.last_trade_quantity << std::endl;
    };
    
    // void write(const OrderAcceptedEvent& event) {};

    void write(const OrderCancelledEvent& event) {
        journal_ << "OrderCanCelled "
            << "timestamp=" << event.header.timestamp_ns
            << " order_id=" << event.order_id
            << " cancel_reason=" << static_cast<int>(event.reason) << std::endl;
    };

    void write(const OrderRejectedEvent& event) {
        journal_ << "OrderRejceted "
            << "timestamp= " << event.header.timestamp_ns
            << " order_id=" << event.order_id
            << " reject_reason=" << static_cast<int>(event.reason) << std::endl;
    };

    // void write(const RiskViolationEvent& event) {};
};}