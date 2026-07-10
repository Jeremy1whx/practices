#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "PersistenceService.h"
#include "../Event_Bus/EventBus.h"
#include "../Event_Bus/EventPublisher.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"

TEST_CASE("Persistence receives events") {
    exchange::EventBus bus;

    exchange::PersistenceService prs_srv("test.log");

    bus.subscribe(&prs_srv);

    exchange::Trade trade{
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {exchange::EventType::Trade, 123},
        trade
    };

    bus.publish(trade_event);

    std::ifstream file("test.log");
    std::string content;
    std::getline(file, content);

    REQUIRE(file.is_open());
    REQUIRE(content.find("Trade") != std::string::npos);
    file.close();

    exchange::BookUpdate update{
        .best_bid = 99.5,
        .best_ask = 100.5,
        .last_trade_price = 100.0,
        .last_trade_quantity = 10
    };

    exchange::BookUpdateEvent book_event{
        {exchange::EventType::BookUpdate, 124},
        update
    };

    bus.publish(book_event);

    file.open("test.log");
    std::stringstream ss;
    ss << file.rdbuf();
    content = ss.str();
    REQUIRE(content.find("Trade") != std::string::npos);
    REQUIRE(content.find("BookUpdate") != std::string::npos);
    file.close();
}