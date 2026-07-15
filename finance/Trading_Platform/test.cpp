#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "Match_Engine/MatchingEngineThread.h"
#include "Lock_Free_Ring_Buffer/mpsc/catch.hpp"
#include "Event_Bus/EventBus.h"
#include "Event_Bus/Subscriber.h"
#include "Event_Bus/EventPublisher.h"
#include "Persistence/PersistenceService.h"
#include "Replay/ReplayEngine.h"

TEST_CASE("Integration Test") {
    std::filesystem::remove("prs_srv.log");
    std::filesystem::remove("logger.log");
    exchange::EventBus bus;

    exchange::EventPublisher publisher(bus);

    AsyncLogger logger("logger.log");
    logger.start();

    exchange::TradeLogger trade_logger(logger);

    exchange::MarketDataService market_data;

    exchange::PersistenceService prs_srv("prs_srv.log");

    bus.subscribe(&trade_logger);
    bus.subscribe(&market_data);
    bus.subscribe(&prs_srv);

    exchange::MatchingEngine engine(publisher);

    Order sell{1, Side::Sell, 100.0, 10, 1};

    Order buy{2, Side::Buy, 100.0, 10, 2};

    engine.process_order(sell);

    engine.process_order(buy);

    logger.stop();

    REQUIRE(market_data.latest().last_trade_price == 100.0);

    REQUIRE(market_data.latest().last_trade_quantity == 10);

    REQUIRE(engine.trade_count() == 1);

    REQUIRE(engine.trades().size() == 1);

    REQUIRE(bus.stats().published_events == 2);

    REQUIRE(bus.stats().trade_events == 1);

    REQUIRE(bus.stats().book_update_events == 1);

    REQUIRE(trade_logger.events_received() == 1);

    REQUIRE(market_data.events_received() == 1);

    std::ifstream file("prs_srv.log");
    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    REQUIRE(content.find("Trade") != std::string::npos);
    REQUIRE(content.find("BookUpdate") != std::string::npos);
    file.close();
}

TEST_CASE("Replay Test") {
    exchange::ReplayReader reader("prs_srv.log");

    exchange::EventBus bus;

    exchange::ReplayEngine engine(reader, bus);

    exchange::MarketDataService market_data;

    bus.subscribe(&market_data);

    engine.replay();

    REQUIRE(bus.stats().published_events == 2);

    REQUIRE(bus.stats().trade_events == 1);

    REQUIRE(bus.stats().book_update_events == 1);

    REQUIRE(market_data.latest().last_trade_price == 100.0);

    REQUIRE(market_data.latest().last_trade_quantity == 10);
}