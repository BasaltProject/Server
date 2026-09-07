#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/placeholder/Decimals.hpp>
#include <basalt/server/runtime/placeholder/ServerPlaceholders.hpp>

#include <doctest.h>

#include <cstdint>

using basalt::wire::common::placeholder::PlaceholderContext;
using basalt::wire::common::placeholder::PlaceholderKind;
using basalt::wire::common::placeholder::PlaceholderValue;
namespace placeholder = basalt::server::runtime::placeholder;

namespace {

placeholder::ServerFacts facts(std::optional<std::array<double, 3>> rates = std::nullopt) {
    return placeholder::ServerFacts{
        .versionName = "26.2",
        .protocolVersion = 776,
        .startedAtMillis = 1000,
        .maxPlayers = [] { return 20; },
        .onlineByWorld = [] { return std::map<std::string, std::int32_t>{{"world", 2}, {"nether", 1}}; },
        .uniqueJoins = [] { return std::int64_t{7}; },
        .loadedChunks = [] { return 441; },
        .liveHeapBytes = [] { return std::optional<std::int64_t>{104857600}; },
        .tickRates = [rates] { return rates; },
        .cpuPercent = [] { return std::optional<double>{12.345}; },
        .nowMillis = [] { return std::int64_t{1000} + (93784 * 1000); },
    };
}

std::string resolve(const basalt::wire::common::placeholder::PlaceholderProvider& provider, std::string_view params) {
    const PlaceholderValue value = provider(params, PlaceholderContext{});
    REQUIRE(value.isText());
    return value.text;
}

} // namespace

TEST_CASE("the server answers what it knows about itself") {
    const auto provider = placeholder::serverPlaceholders(facts());
    CHECK(resolve(provider, "version") == "26.2");
    CHECK(resolve(provider, "protocol") == "776");
    CHECK(resolve(provider, "online") == "3");
    CHECK(resolve(provider, "online_nether") == "1");
    // A world nobody is in is nought, not an error: the question is answerable.
    CHECK(resolve(provider, "online_the_end") == "0");
    CHECK(resolve(provider, "max_players") == "20");
    CHECK(resolve(provider, "unique_joins") == "7");
    CHECK(resolve(provider, "total_chunks") == "441");
    CHECK(resolve(provider, "ram_used") == "100");
    CHECK(resolve(provider, "cpu") == "12.35");
    // One day, two hours, three minutes and four seconds after it started.
    CHECK(resolve(provider, "uptime") == "1d 2h 3m 4s");
    CHECK(resolve(provider, "uptime_seconds") == "93784");
}

TEST_CASE("a placeholder that is real but unanswerable says which") {
    const auto provider = placeholder::serverPlaceholders(facts());
    const PlaceholderValue unmeasured = provider("tps", PlaceholderContext{});
    CHECK(unmeasured.kind == PlaceholderKind::Unavailable);
    REQUIRE(unmeasured.reason.has_value());
    CHECK(*unmeasured.reason == "nothing has measured the tick rate yet");

    const PlaceholderValue heap = provider("ram_max", PlaceholderContext{});
    CHECK(heap.kind == PlaceholderKind::Unavailable);
    CHECK(heap.reason.has_value());

    // A name nobody registered is a different mistake from one this build cannot answer.
    CHECK(provider("nonsense", PlaceholderContext{}).kind == PlaceholderKind::Unknown);
}

TEST_CASE("a coloured tick rate is markup around the same number") {
    const auto healthy = placeholder::serverPlaceholders(facts(std::array{20.0, 19.5, 15.0}));
    CHECK(resolve(healthy, "tps") == "20.00");
    CHECK(resolve(healthy, "tps_5") == "19.50");
    CHECK(resolve(healthy, "tps_1_colored") == "<green>20.00</green>");
    CHECK(resolve(healthy, "tps_15_colored") == "<yellow>15.00</yellow>");

    const auto struggling = placeholder::serverPlaceholders(facts(std::array{9.125, 9.0, 9.0}));
    CHECK(resolve(struggling, "tps_1_colored") == "<red>9.13</red>");
}

TEST_CASE("the date format understands the six fields worth supporting") {
    // Midnight UTC on the first of January 2020, read back through the local zone.
    const std::string stamp = placeholder::serverPlaceholderFormat::localTime("yyyy-MM-dd HH:mm:ss", 1577836800000);
    CHECK(stamp.size() == 19);
    CHECK(stamp.substr(0, 2) == "20");
    // Anything that is not one of the six fields is kept as written.
    CHECK(placeholder::serverPlaceholderFormat::localTime("day dd of MM", 1577836800000).starts_with("day "));
}

TEST_CASE("a numeric placeholder says how many decimals it wants") {
    namespace decimals = placeholder::decimals;
    REQUIRE(decimals::split("x_2").has_value());
    CHECK(decimals::split("x_2")->first == "x");
    CHECK(decimals::split("x_2")->second == 2);
    // Not a suffix: no underscore, nothing after it, nothing before it, or more than one digit.
    CHECK_FALSE(decimals::split("x").has_value());
    CHECK_FALSE(decimals::split("x_").has_value());
    CHECK_FALSE(decimals::split("_2").has_value());
    CHECK_FALSE(decimals::split("x_25").has_value());
    CHECK_FALSE(decimals::split("has_permission").has_value());

    CHECK(decimals::format(21.70704236703265, 2) == "21.71");
    CHECK(decimals::format(21.70704236703265, 0) == "22");
    CHECK(decimals::format(-0.004, 2) == "0.00");
    CHECK(decimals::format(-1.5, 1) == "-1.5");
    CHECK(decimals::format(3.0, 3) == "3.000");
}
