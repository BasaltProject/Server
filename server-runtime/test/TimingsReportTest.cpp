#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/timings/TimingsReport.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <doctest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::timings;
namespace common = basalt::wire::common;

namespace {

/// Sources that answer a fixed number for everything, so the report's own shape is what is measured.
[[nodiscard]] ReportSources fixedSources() {
    return ReportSources{.versionName = "26.2.0-dev",
                         .protocolVersion = 776,
                         .uptimeSeconds = [] { return std::int64_t{3600}; },
                         .onlinePlayers = [] { return 7; },
                         .maxPlayers = [] { return 20; },
                         .loadedChunks = [] { return 1024; },
                         .regionCacheBytes = [] { return std::int64_t{5242880}; },
                         .chunkBytes = [] { return std::int64_t{33554432}; },
                         .chunkBlockEntities = [] { return 42; },
                         .chunkScheduledTicks = [] { return 3; },
                         .chunkGeneratedEntities = [] { return 11; },
                         .worldgenCaches = [] { return std::vector<std::int64_t>{1, 2, 3, 4}; },
                         .worldCount = [] { return 3; },
                         .cpuPercent = [] { return std::optional<double>{12.3456}; },
                         .liveHeapBytes = [] { return std::optional<std::int64_t>{123456789}; },
                         .residentBytes = [] { return std::optional<std::int64_t>{987654321}; }};
}

/// What a platform that cannot answer says: nothing, rather than a made-up zero.
[[nodiscard]] ReportSources silentSources() {
    ReportSources sources = fixedSources();
    sources.cpuPercent = [] { return std::optional<double>{}; };
    sources.liveHeapBytes = [] { return std::optional<std::int64_t>{}; };
    sources.residentBytes = [] { return std::optional<std::int64_t>{}; };
    // Fewer cache figures than the report writes, which the reader has to cope with rather than
    // read past the end of.
    sources.worldgenCaches = [] { return std::vector<std::int64_t>{9}; };
    return sources;
}

[[nodiscard]] std::string lineOf(std::string_view json, std::string_view key) {
    const std::size_t at = json.find(key);
    if (at == std::string_view::npos) {
        return "-";
    }
    const std::size_t start = json.rfind('\n', at) + 1;
    const std::size_t end = json.find('\n', at);
    return std::string{json.substr(start, end - start)};
}

} // namespace

TEST_CASE("the report says what the server was doing, in the shape the frozen kotlin wrote") {
    common::Timings timings;
    common::TickDurations durations;
    const TimingsReport report{timings, durations, fixedSources()};
    const std::string json = report.json(60 * common::TickDurations::kNanosPerSecond);

    CHECK(json.starts_with("{\n"));
    CHECK(json.ends_with("}\n"));
    CHECK(lineOf(json, "\"basalt\"") == R"(  "basalt": "26.2.0-dev",)");
    CHECK(lineOf(json, "\"protocol\"") == R"(  "protocol": 776,)");
    CHECK(lineOf(json, "\"uptime_seconds\"") == R"(  "uptime_seconds": 3600,)");
    // Measuring is off until somebody turns it on, and the report says so rather than pretending.
    CHECK(lineOf(json, "\"measuring\"") == R"(  "measuring": false,)");

    CHECK(lineOf(json, "\"players_online\"") == R"(    "players_online": 7,)");
    CHECK(lineOf(json, "\"players_max\"") == R"(    "players_max": 20,)");
    CHECK(lineOf(json, "\"worlds\"") == R"(    "worlds": 3,)");
    CHECK(lineOf(json, "\"chunks_loaded\"") == R"(    "chunks_loaded": 1024,)");
    CHECK(lineOf(json, "\"region_cache_bytes\"") == R"(    "region_cache_bytes": 5242880,)");
    CHECK(lineOf(json, "\"chunk_bytes\"") == R"(    "chunk_bytes": 33554432,)");
    CHECK(lineOf(json, "\"worldgen_templates\"") == R"(    "worldgen_templates": 1,)");
    CHECK(lineOf(json, "\"worldgen_assembly_pieces\"") == R"(    "worldgen_assembly_pieces": 4,)");
    // Three decimals, because most of what is measured here takes well under a millisecond.
    CHECK(lineOf(json, "\"cpu_percent_of_one_core\"") == R"(    "cpu_percent_of_one_core": 12.346,)");
    CHECK(lineOf(json, "\"heap_bytes_live\"") == R"(    "heap_bytes_live": 123456789,)");
    CHECK(lineOf(json, "\"process_resident_bytes\"") == R"(    "process_resident_bytes": 987654321)");

    CHECK(lineOf(json, "\"target_per_second\"") == R"(    "target_per_second": 20,)");
    // The five rate windows and the three duration windows, named the way an operator reads them.
    CHECK(json.find(R"(      "5s": )") != std::string::npos);
    CHECK(json.find(R"(      "10s": )") != std::string::npos);
    CHECK(json.find(R"(      "1m": )") != std::string::npos);
    CHECK(json.find(R"(      "5m": )") != std::string::npos);
    CHECK(json.find(R"(      "15m": )") != std::string::npos);
    // An empty ring answers zero samples rather than refusing.
    CHECK(json.find(R"("samples": 0)") != std::string::npos);

    // Nothing was measured, so there are no sections: a section nobody called is absent, not a row
    // of zeroes.
    CHECK(json.find("\"sections\": [\n  ]\n") != std::string::npos);

    // And the whole thing parses, which is what a trailing comma anywhere would break.
    const std::expected<basalt::wire::common::JsonValue, basalt::wire::common::JsonError> parsed =
        basalt::wire::common::parseJson(json);
    REQUIRE(parsed.has_value());
    const basalt::wire::common::JsonObject* root = parsed->get<basalt::wire::common::JsonObject>();
    REQUIRE(root != nullptr);

    // Five rate windows and three duration windows, no more and no fewer.
    const basalt::wire::common::JsonObject& ticks = *root->find("ticks")->get<basalt::wire::common::JsonObject>();
    CHECK(ticks.find("per_second")->get<basalt::wire::common::JsonObject>()->size() == 5);
    const basalt::wire::common::JsonObject& millis =
        *ticks.find("duration_millis")->get<basalt::wire::common::JsonObject>();
    CHECK(millis.size() == 3);
    CHECK(millis.names()[0] == "10s");
    CHECK(millis.names()[1] == "1m");
    CHECK(millis.names()[2] == "5m");
}

TEST_CASE("what the platform cannot answer is null, and a short cache list is not read past") {
    common::Timings timings;
    common::TickDurations durations;
    const TimingsReport report{timings, durations, silentSources()};
    const std::string json = report.json(0);

    CHECK(lineOf(json, "\"cpu_percent_of_one_core\"") == R"(    "cpu_percent_of_one_core": null,)");
    CHECK(lineOf(json, "\"heap_bytes_live\"") == R"(    "heap_bytes_live": null,)");
    CHECK(lineOf(json, "\"process_resident_bytes\"") == R"(    "process_resident_bytes": null)");
    // One figure given, three missing: the missing ones are zero rather than whatever was next in
    // memory.
    CHECK(lineOf(json, "\"worldgen_templates\"") == R"(    "worldgen_templates": 9,)");
    CHECK(lineOf(json, "\"worldgen_template_blocks\"") == R"(    "worldgen_template_blocks": 0,)");
    CHECK(lineOf(json, "\"worldgen_assembly_pieces\"") == R"(    "worldgen_assembly_pieces": 0,)");
}

TEST_CASE("the sections a run measured, and the ones it did not") {
    common::Timings timings;
    timings.setEnabled(true);
    // Three sections, one of them never called: only the two that cost something are reported.
    timings.of("tick").record(2500000);
    timings.of("tick").record(1500000);
    timings.of("chunk.load").record(12345678);
    static_cast<void>(timings.of("never.called"));

    common::TickDurations durations;
    const TimingsReport report{timings, durations, fixedSources()};
    const std::string json = report.json(10 * common::TickDurations::kNanosPerSecond);

    CHECK(lineOf(json, "\"measuring\"") == R"(  "measuring": true,)");
    CHECK(json.find(R"("name": "never.called")") == std::string::npos);
    CHECK(json.find(R"({"name": "tick", "calls": 2, "total_millis": 4.000, "average_millis": 2.000, )"
                    R"("max_millis": 2.500})") != std::string::npos);
    CHECK(json.find(R"({"name": "chunk.load", "calls": 1, "total_millis": 12.346, )"
                    R"("average_millis": 12.346, "max_millis": 12.346})") != std::string::npos);
    // The last section carries no comma after it, which is what makes the document parse.
    CHECK(json.find("}\n  ]\n}\n") != std::string::npos);
}

TEST_CASE("the stamp is a local timestamp to the second") {
    const std::string stamp = TimingsReport::stamp();
    // `2026-09-01_07-43-11`: the shape an operator refers to a run by.
    CHECK(stamp.size() == 19);
    CHECK(stamp[4] == '-');
    CHECK(stamp[7] == '-');
    CHECK(stamp[10] == '_');
    CHECK(stamp[13] == '-');
    CHECK(stamp[16] == '-');
    for (const std::size_t index : {0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U, 11U, 12U, 14U, 15U, 17U, 18U}) {
        CAPTURE(index);
        CHECK(std::isdigit(static_cast<unsigned char>(stamp[index])) != 0);
    }
    // The year and the month are the real ones, not the raw `tm` fields: `tm_year` counts from 1900
    // and `tm_mon` from zero, and both read as a plausible-looking timestamp if left uncorrected.
    const std::int32_t year = std::stoi(stamp.substr(0, 4));
    const std::int32_t month = std::stoi(stamp.substr(5, 2));
    const std::int32_t day = std::stoi(stamp.substr(8, 2));
    CHECK(year >= 2020);
    CHECK(year <= 2200);
    CHECK(month >= 1);
    CHECK(month <= 12);
    CHECK(day >= 1);
    CHECK(day <= 31);

    // A range check is not enough for the month: `tm_mon` counts from zero, and eleven months of the
    // year the raw field still lands inside 1..12. Compared against the calendar instead. The stamp
    // is local and this is UTC, so they can be a day apart; when they agree on the day they must
    // agree on the month and the year exactly, and that is the case this asserts.
    const std::chrono::year_month_day today{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
    const std::int32_t utcDay = static_cast<std::int32_t>(static_cast<unsigned>(today.day()));
    if (day == utcDay) {
        CHECK(month == static_cast<std::int32_t>(static_cast<unsigned>(today.month())));
        CHECK(year == static_cast<std::int32_t>(static_cast<int>(today.year())));
    }

    CHECK(TimingsReport::kDefaultDirectory == "./.basalt/timings");
}
