#include "MatchingEngine.h"


void MatchingEngine::submit_order(Order& order) {

    if (order.side == Side::Buy) {
        match_buy(order);
    } else {
        match_sell(order);
    }
}

// void MatchingEngine::publish_market_data(const Trade& trade) {

//     MarketDataEvent event;

//     if (!bids_.empty()) {
//         event.best_bid = bids_.begin()->first;
//     }

//     if (!asks_.empty()) {
//         event.best_ask = asks_.begin()->first;
//     }

//     event.last_trade_price = trade.price;

//     event.last_trade_quantity = trade.quantity;

//     // market_data_events_.push_back(event);
// }

bool MatchingEngine::cancel_order(uint64_t order_id) {

    auto it = order_lookup_.find(order_id);

    if (it == order_lookup_.end()) {
        std::cerr << "cancel_order: line " << 37 <<  " - Order not found: " << order_id << std::endl;
        return false;
    }

    Order* order = it->second;

    if (order->side == Side::Buy) {
        auto level_it = listener_->bids().find(order->price);

        if (level_it == listener_->bids().end()){
            std::cerr << "cancel_order: line " << 47 << " - Order not found: " << order_id << std::endl;
            return false; 
        }        

        auto& level = level_it->second;

        remove_order(level, order);

        if (level.head == nullptr) {
            listener_->bids().erase(level_it);
        }

    } else {
        auto level_it = listener_->asks().find(order->price);

        if (level_it == listener_->asks().end()){
            std::cerr << "cancel_order: line " << 63 << " - Order not found: " << order_id << std::endl;
            return false;
        } 

        auto& level = level_it->second;

        remove_order(level, order);

        if (level.head == nullptr) {
            listener_->asks().erase(level_it);
        }
    }

    order_lookup_.erase(it);

    listener_->order_pool().deallocate(order);

    return true;
}

void MatchingEngine::match_buy(Order& order) {

    while (order.quantity > 0 && !listener_->asks().empty()) {
        auto best_ask = listener_->asks().begin(); // asks_.begin() contains the lowest ask

        if (best_ask->first > order.price) {
            break; //seller's ask should be lower than buyer's bid, then the buyer would buy
        }

        auto& queue = best_ask->second;
        Order* resting = queue.head;
        uint32_t traded = std::min(order.quantity, resting->quantity);

        uint64_t match_start_ns = now_ns();

        // generate trade
        Trade* trade = listener_->trade_pool().allocate();

        assert(trade != nullptr);

        trade->buy_order_id = order.order_id;

        trade->sell_order_id = resting->order_id;

        trade->price = resting->price;

        trade->quantity = traded;

        uint64_t current_ns = now_ns();

        trade->total_latency_ns = current_ns - order.ingress_timestamp_ns;

        trade->queue_latency_ns = order.egress_timestamp_ns - order.ingress_timestamp_ns;

        trade->match_duration_ns = current_ns - match_start_ns;

        latency_collector_.add(trade->total_latency_ns, trade->queue_latency_ns, trade->match_duration_ns);

        trades_.push_back(*trade);

        trade_count_.fetch_add(1, std::memory_order_relaxed);
        
        // if (logger_) {
        //     auto msg = format_trade(*trade);
        //     logger_->log(msg.c_str());
        // } else {
        //     trade_pool_.recycle_oldest();}

        listener_->on_trade(*trade);

        order.quantity -= traded;
        resting->quantity -= traded;

        if (resting->quantity == 0) {
            remove_order(queue, resting);
            order_lookup_.erase(resting->order_id);
            listener_->order_pool().deallocate(resting);

            if (queue.head == nullptr) {
                listener_->asks().erase(best_ask);
            }
        }
    }

    if (order.quantity > 0) {
        add_to_book(order);
    }
}

void MatchingEngine::match_sell(Order& order) {
    while (order.quantity > 0 && !listener_->bids().empty()) {
        auto best_bid = listener_->bids().begin(); //bids_.begin() contains the highest bid

        if (best_bid->first < order.price) {
            break; //buyer's bid should be higher than seller's ask, then the seller would sell
        }

        auto& queue = best_bid->second;
        Order* resting = queue.head;
        uint32_t traded = std::min(order.quantity, resting->quantity);

        uint64_t match_start_ns = now_ns();

        Trade* trade = listener_->trade_pool().allocate();

        assert(trade != nullptr);

        trade->buy_order_id = resting->order_id;            

        trade->sell_order_id =order.order_id;           

        trade->price = resting->price;

        trade->quantity = traded;

        uint64_t current_ns = now_ns();

        trade->total_latency_ns = current_ns - order.ingress_timestamp_ns;

        trade->queue_latency_ns = order.egress_timestamp_ns - order.ingress_timestamp_ns;

        trade->match_duration_ns = current_ns - match_start_ns;

        latency_collector_.add(trade->total_latency_ns, trade->queue_latency_ns, trade->match_duration_ns);

        trades_.push_back(*trade);

        trade_count_.fetch_add(1, std::memory_order_relaxed);
        
        // if (logger_) {
        //     auto msg = format_trade(*trade);
        //     logger_->log(msg.c_str());
        // } else {
        //     trade_pool_.recycle_oldest();}

        listener_->on_trade(*trade);

        order.quantity -= traded;
        resting->quantity -= traded;

        if (resting->quantity == 0) {
            remove_order(queue, resting);
            order_lookup_.erase(resting->order_id);
            listener_->order_pool().deallocate(resting);

            if (queue.head == nullptr) {
                listener_->bids().erase(best_bid);
            }
        }
    }
    if (order.quantity > 0) {
        add_to_book(order);
    }
}

void MatchingEngine::add_to_book(Order& order) {

    Order* stored = listener_->order_pool().allocate();    

    assert(stored != nullptr);

    *stored = std::move(order);

    if (stored->side == Side::Buy) {
        append_order(listener_->bids()[stored->price], stored);
    } else {
        append_order(listener_->asks()[stored->price], stored);
    }
    order_lookup_[stored->order_id] = stored;
}

void MatchingEngine::append_order(PriceLevel& level, Order* order) {

    order->next = nullptr;

    order->prev = level.tail;

    if (level.tail) {
        level.tail->next = order;
    }

    level.tail = order;

    if (!level.head) {
        level.head = order;
    }
}

void MatchingEngine::remove_order(PriceLevel& level, Order* order) {

    if (order->prev) {
        order->prev->next =
            order->next;
    }

    if (order->next) {
        order->next->prev =
            order->prev;
    }

    if (level.head == order) {
        level.head = order->next;
    }

    if (level.tail == order) {
        level.tail = order->prev;
    }

    order->next = nullptr;

    order->prev = nullptr;
}