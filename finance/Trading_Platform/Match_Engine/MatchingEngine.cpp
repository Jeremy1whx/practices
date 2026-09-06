#include "MatchingEngine.h"

exchange::MatchingEngine::MatchingEngine(EventPublisher& publisher, ExpiryScheduler& scheduler) : publisher_(publisher), expiry_scheduler_(scheduler){
    bids_.reserve(1024);
    asks_.reserve(1024);
    expiry_scheduler_.set_cancel_callback(
            [this](uint64_t order_id, CancelReason reason) {
                this->cancel_order(order_id, reason);
            }
        );
}

template<typename Container, typename Compare>
typename Container::iterator exchange::MatchingEngine::find_price_level(Container& container, double price) {
    auto it = std::lower_bound(
        container.begin(), 
        container.end(), 
        price,
        [](const PriceLevelWithPrice& element, double value) {
            return Compare{}(element, value);
        }
    );
    
    if (it != container.end() && it->price == price) {
        return it;
    }
    return container.end();
}

template<typename Container, typename Compare>
typename Container::const_iterator exchange::MatchingEngine::find_price_level(const Container& container, double price) const {
    auto it = std::lower_bound(
        container.begin(), 
        container.end(), 
        price,
        [](const PriceLevelWithPrice& element, double value) {
            return Compare{}(element, value);
        }
    );
    
    if (it != container.end() && it->price == price) {
        return it;
    }
    return container.end();
}

template<typename Container, typename Compare>
void exchange::MatchingEngine::insert_price_level(Container& container, double price, uint32_t avaliable, PriceLevel level) {
    auto it = std::lower_bound(
        container.begin(), 
        container.end(), 
        price,
        [](const PriceLevelWithPrice& element, double value) {
            return Compare{}(element, value);
        }
    );
    
    container.insert(it, PriceLevelWithPrice{price, avaliable, std::move(level)});
}

template<typename Container, typename Compare>
bool exchange::MatchingEngine::remove_price_level(Container& container, double price) {
    auto it = find_price_level<Container, Compare>(container, price);
    if (it == container.end()) {
        return false;
    }
    container.erase(it);
    return true;
}

void exchange::MatchingEngine::process_order(Order& order) {
    
    switch (order.type)
    {
    case Type::GTD:
    case Type::DAY:
    case Type::GTT:
    case Type::GTC:        
        if (order.side == Side::Buy) {
            match_buy(order);
        } else {
            match_sell(order);
        }

        if (order.quantity > 0) {
            add_to_book(order);
        }
        break;

    case Type::IOC:        
        if (order.side == Side::Buy) {
            match_buy(order);
        } else {
            match_sell(order);
        }

        if (order.quantity > 0) cancel_order(order.order_id, CancelReason::IOCResidual);
        break;
    
    case Type::FOk:
        if (order.side == Side::Buy) {
            if (all_matched_buy(order)) {
                match_buy(order);
            } else cancel_order(order.order_id, CancelReason::FOKNotFilled);
        } else {
            if (all_matched_sell(order)) {
                match_sell(order);
            } else cancel_order(order.order_id, CancelReason::FOKNotFilled);
        }
        break;
    }
}

// void exchange::MatchingEngine::publish_market_data(const Trade& trade) {

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

bool exchange::MatchingEngine::cancel_order(uint64_t order_id, CancelReason reason) {

    if (reason != CancelReason::IOCResidual) {
        if (reason != CancelReason::FOKNotFilled) {
            auto it = order_lookup_.find(order_id);

            if (it == order_lookup_.end()) {
                std::cerr << "cancel_order: line " << 134 <<  " - Order not found: " << order_id << std::endl;
                return false;
            }

            Order* order = it->second;

            if(order->expire_time != 0 && reason != CancelReason::Expired) expiry_scheduler_.delete_expiry(order_id);
        
            if (order->side == Side::Buy) {
                auto level_it = find_price_level<std::vector<PriceLevelWithPrice>, BidCompare>(bids_,order->price);

                if (level_it == bids_.end()){
                    std::cerr << "cancel_order: line " << 144 << " - Order not found: " << order_id << std::endl;
                    return false; 
                }
                
                level_it->avaliable -= order->quantity;

                auto& level = level_it->level;

                remove_order(level, order);

                if (level.head == nullptr) {
                    bids_.erase(level_it);
                }

            } else {
                auto level_it = find_price_level<std::vector<PriceLevelWithPrice>, AskCompare>(asks_, order->price);

                if (level_it == asks_.end()){
                    std::cerr << "cancel_order: line " << 158 << " - Order not found: " << order_id << std::endl;
                    return false;
                } 

                level_it->avaliable -= order->quantity;

                auto& level = level_it->level;

                remove_order(level, order);

                if (level.head == nullptr) {
                    asks_.erase(level_it);
                }
            }

            order_lookup_.erase(it);
            order_pool_.deallocate(order);
        }   
    } 

    publisher_.publish(order_id, reason);    

    return true;
}

void exchange::MatchingEngine::match_buy(Order& order) {

    while (order.quantity > 0 && !asks_.empty()) {
        auto& best_ask = asks_.front(); // asks_.front() contains the lowest ask

        if (best_ask.price > order.price) {
            break; //seller's ask should be lower than buyer's bid, then the buyer would buy
        }

        auto& queue = best_ask.level;
        Order* resting = queue.head;
        uint32_t traded = std::min(order.quantity, resting->quantity);

        uint64_t match_start_ns = now_ns();

        // generate trade
        Trade* trade = trade_pool_.allocate();

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

        BookUpdate update;
        if (!bids_.empty()) update.best_bid = bids_.front().price;
        update.best_ask = asks_.front().price;
        update.last_trade_price = trade->price;
        update.last_trade_quantity = trade->quantity;
        
        // if (logger_) {
        //     auto msg = format_trade(*trade);
        //     logger_->log(msg.c_str());
        // } else {
        

        // listener_->on_trade(*trade);
        publisher_.publish(*trade);
        publisher_.publish(update);
        trade_pool_.recycle_oldest();
        order.quantity -= traded;
        resting->quantity -= traded;
        best_ask.avaliable -= traded;

        if (resting->quantity == 0) {
            remove_order(queue, resting);
            order_lookup_.erase(resting->order_id);
            order_pool_.deallocate(resting);

            if (queue.head == nullptr) {
                asks_.erase(asks_.begin());
            }
        }
    }    
}

bool exchange::MatchingEngine::all_matched_buy(Order& order) {
    if (asks_.empty()) return false;

    uint32_t count = 0;
    auto it = find_price_level<std::vector<PriceLevelWithPrice>, AskCompare>(asks_, order.price);

    if (it == asks_.end()) return false;

    if (it->price == order.price) ++it;

    for (auto iter = asks_.begin(); iter != it; ++iter) {
        count += iter->avaliable;
    }

    if (count < order.quantity) return false;
    return true;
}

void exchange::MatchingEngine::match_sell(Order& order) {
    while (order.quantity > 0 && !bids_.empty()) {
        auto& best_bid = bids_.front(); //bids_.front() contains the highest bid

        if (best_bid.price < order.price) {
            break; //buyer's bid should be higher than seller's ask, then the seller would sell
        }

        auto& queue = best_bid.level;
        Order* resting = queue.head;
        uint32_t traded = std::min(order.quantity, resting->quantity);

        uint64_t match_start_ns = now_ns();

        Trade* trade = trade_pool_.allocate();

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

        BookUpdate update;
        update.best_bid = bids_.front().price;
        if (!asks_.empty()) update.best_ask = asks_.front().price;
        update.last_trade_price = trade->price;
        update.last_trade_quantity = trade->quantity;
        
        // if (logger_) {
        //     auto msg = format_trade(*trade);
        //     logger_->log(msg.c_str());
        // } else {

        // listener_->on_trade(*trade);
        publisher_.publish(*trade);
        publisher_.publish(update);
        trade_pool_.recycle_oldest();

        order.quantity -= traded;
        resting->quantity -= traded;
        best_bid.avaliable -= traded;

        if (resting->quantity == 0) {
            remove_order(queue, resting);
            order_lookup_.erase(resting->order_id);
            order_pool_.deallocate(resting);

            if (queue.head == nullptr) {
                bids_.erase(bids_.begin());
            }
        }
    }
}

bool exchange::MatchingEngine::all_matched_sell(Order& order) {
    if (bids_.empty()) return false;

    uint32_t count = 0;
    auto it = find_price_level<std::vector<PriceLevelWithPrice>, BidCompare>(bids_, order.price);

    if (it == bids_.end()) return false;

    if (it->price == order.price) ++it;

    for (auto iter = bids_.begin(); iter != it; ++iter) {
        count += iter->avaliable;
    }

    if (count < order.quantity) return false;
    return true;
}

void exchange::MatchingEngine::add_to_book(Order& order) {

    Order* stored = order_pool_.allocate();    

    assert(stored != nullptr);

    *stored = std::move(order);
    
    publisher_.publish(stored->order_id, stored->side, stored->price, stored->quantity);

    if (stored->side == Side::Buy) {
        // std::cout << "add to bid, id =" << stored->order_id << std::endl;
        auto it = find_price_level<std::vector<PriceLevelWithPrice>, BidCompare>(bids_, stored->price);
        
        if (stored->type == Type::GTD || stored->type == Type::DAY || stored->type == Type::GTT) {
            if (stored->expire_time != 0) {
                expiry_scheduler_.add_expiry(*stored);
            }
        }

        if (it != bids_.end()) {
            it->avaliable += stored->quantity;
            append_order(it->level, stored);
        } else {
            PriceLevel new_level;
            append_order(new_level, stored);
            insert_price_level<std::vector<PriceLevelWithPrice>, BidCompare>(bids_, stored->price, stored->quantity, std::move(new_level));
        }
    } else {
        // std::cout << "add to ask, id = " << stored->order_id << std::endl;
        auto it = find_price_level<std::vector<PriceLevelWithPrice>, AskCompare>(asks_, stored->price);

        if (stored->type == Type::GTD || stored->type == Type::DAY || stored->type == Type::GTT) {
            if (stored->expire_time != 0) {
                expiry_scheduler_.add_expiry(*stored);
            }
        }
        
        if (it != asks_.end()) {
            it->avaliable += stored->quantity;
            append_order(it->level, stored);
        } else {
            PriceLevel new_level;
            append_order(new_level, stored);
            insert_price_level<std::vector<PriceLevelWithPrice>, AskCompare>(asks_, stored->price, stored->quantity, std::move(new_level));
        }
    }
    order_lookup_[stored->order_id] = stored;
}

void exchange::MatchingEngine::append_order(PriceLevel& level, Order* order) {

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

void exchange::MatchingEngine::remove_order(PriceLevel& level, Order* order) {

    if (order->prev) {
        order->prev->next = order->next;
    }

    if (order->next) {
        order->next->prev = order->prev;
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