#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/timings/TimingsReport.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt;
using namespace basalt::server::runtime::timings;

namespace {

const wire::common::JsonObject& objectOf(const wire::common::JsonValue& value) {
    const auto* object = value.get<wire::common::JsonObject>();
    REQUIRE(object != nullptr);
    return *object;
}

const wire::common::JsonValue& member(const wire::common::JsonObject& object, std::string_view name) {
    const wire::common::JsonValue* found = object.find(name);
    REQUIRE(found != nullptr);
    return *found;
}

} // namespace

TEST_CASE("the report is valid json with the call tree of what ran") {
    wire::common::Timings timings;
    wire::common::TickDurations ticks;
    timings.setEnabled(true);
    timings.nameThread("Server thread");
    timings.of("tick").time([&] {
        timings.of("world.\"odd\" name\n").time([] {});
        timings.of("world.other").time([] {});
    });
    for (std::int64_t tick = 0; tick < 100; ++tick) {
        ticks.record(tick * 50'000'000, 10'000'000);
    }

    ReportSources sources;
    sources.versionName = "26.2";
    const TimingsReport report{timings, ticks, sources};
    const std::string text = report.json(5'000'000'000);

    const auto parsed = wire::common::parseJson(text);
    REQUIRE(parsed.has_value());
    const wire::common::JsonObject& root = objectOf(*parsed);
    CHECK(*member(root, "format").get<std::string>() == "basalt-timings");
    CHECK(*member(root, "version").get<std::int64_t>() == 2);

    const auto* threads = member(root, "threads").get<std::vector<wire::common::JsonValue>>();
    REQUIRE(threads != nullptr);
    REQUIRE(threads->size() == 1);
    const wire::common::JsonObject& thread = objectOf(threads->front());
    CHECK(*member(thread, "name").get<std::string>() == "Server thread");
    const wire::common::JsonObject& top = objectOf(member(thread, "root"));
    const auto* children = member(top, "children").get<std::vector<wire::common::JsonValue>>();
    REQUIRE(children != nullptr);
    REQUIRE(children->size() == 1);
    CHECK(*member(objectOf(children->front()), "name").get<std::string>() == "tick");
    const auto* nested = member(objectOf(children->front()), "children").get<std::vector<wire::common::JsonValue>>();
    REQUIRE(nested != nullptr);
    CHECK(nested->size() == 2);

    const wire::common::JsonObject& tickBlock = objectOf(member(root, "ticks"));
    const wire::common::JsonObject& timeline = objectOf(member(tickBlock, "timeline"));
    CHECK(member(timeline, "points").get<std::vector<wire::common::JsonValue>>() != nullptr);
}

TEST_CASE("sources left empty are reported as absent instead of failing") {
    wire::common::Timings timings;
    wire::common::TickDurations ticks;
    const TimingsReport report{timings, ticks, ReportSources{}};
    CHECK(wire::common::parseJson(report.json(1'000'000'000)).has_value());
    CHECK_FALSE(report.hasProfile());
}
