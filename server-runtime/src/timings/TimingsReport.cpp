#include <basalt/server/runtime/timings/TimingsReport.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <format>

namespace basalt::server::runtime::timings {
namespace {

using wire::common::DurationSummary;
using wire::common::TickDurations;

// Three decimals (most sections take well under a millisecond) and `null` for what the platform could not answer.
[[nodiscard]] std::string number(std::optional<double> value) {
    if (!value.has_value()) {
        return "null";
    }
    const std::int64_t rounded = static_cast<std::int64_t>(std::round(*value * 1000.0));
    return std::to_string(rounded / 1000) + "." + std::format("{:03d}", std::abs(rounded % 1000));
}

[[nodiscard]] std::string number(double value) { return number(std::optional<double>{value}); }

[[nodiscard]] std::string quote(std::string_view value) {
    std::string out = "\"";
    for (const char character : value) {
        if (character == '\\' || character == '"') {
            out += '\\';
        }
        out += character;
    }
    return out + "\"";
}

[[nodiscard]] std::string label(std::int32_t seconds) {
    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }
    if (seconds < 3600) {
        return std::to_string(seconds / 60) + "m";
    }
    return std::to_string(seconds / 3600) + "h";
}

[[nodiscard]] std::string summaryOf(const DurationSummary& value) {
    return R"({"samples": )" + std::to_string(value.samples) + R"(, "min": )" + number(value.minMillis) +
           R"(, "median": )" + number(value.medianMillis) + R"(, "p95": )" + number(value.percentile95Millis) +
           R"(, "max": )" + number(value.maxMillis) + R"(, "mean": )" + number(value.meanMillis) + "}";
}

[[nodiscard]] std::string orNull(std::optional<std::int64_t> value) {
    return value.has_value() ? std::to_string(*value) : "null";
}

[[nodiscard]] std::int64_t at(const std::vector<std::int64_t>& values, std::size_t index) {
    return index < values.size() ? values[index] : 0;
}

} // namespace

std::string TimingsReport::stamp() {
    // Local time, via `localtime_r` since this platform's standard library lacks C++20 `zoned_time`.
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return std::format("{:04d}-{:02d}-{:02d}_{:02d}-{:02d}-{:02d}", local.tm_year + 1900, local.tm_mon + 1,
                       local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
}

std::string TimingsReport::write(std::int64_t nowNanos) const {
    const std::string& directory = directory_;
    wire::io::fileIO::createDirectories(directory);
    const std::string path = directory + "/" + stamp() + ".json";
    wire::io::fileIO::writeText(path, json(nowNanos));
    return path;
}

std::string TimingsReport::json(std::int64_t nowNanos) const {
    std::string out;
    out += "{\n";
    out += "  \"basalt\": " + quote(sources_.versionName) + ",\n";
    out += "  \"protocol\": " + std::to_string(sources_.protocolVersion) + ",\n";
    out += "  \"generated\": " + quote(stamp()) + ",\n";
    out += "  \"uptime_seconds\": " + std::to_string(sources_.uptimeSeconds()) + ",\n";
    out += std::string{"  \"measuring\": "} + (timings_->enabled() ? "true" : "false") + ",\n";
    out +=
        "  \"measured_seconds\": " + std::to_string(timings_->windowNanos(nowNanos) / TickDurations::kNanosPerSecond) +
        ",\n";

    out += "  \"server\": {\n";
    out += "    \"players_online\": " + std::to_string(sources_.onlinePlayers()) + ",\n";
    out += "    \"players_max\": " + std::to_string(sources_.maxPlayers()) + ",\n";
    out += "    \"worlds\": " + std::to_string(sources_.worldCount()) + ",\n";
    out += "    \"chunks_loaded\": " + std::to_string(sources_.loadedChunks()) + ",\n";
    out += "    \"region_cache_bytes\": " + std::to_string(sources_.regionCacheBytes()) + ",\n";
    out += "    \"chunk_bytes\": " + std::to_string(sources_.chunkBytes()) + ",\n";
    out += "    \"chunk_block_entities\": " + std::to_string(sources_.chunkBlockEntities()) + ",\n";
    out += "    \"chunk_scheduled_ticks\": " + std::to_string(sources_.chunkScheduledTicks()) + ",\n";
    out += "    \"chunk_generated_entities\": " + std::to_string(sources_.chunkGeneratedEntities()) + ",\n";
    const std::vector<std::int64_t> worldgen = sources_.worldgenCaches();
    out += "    \"worldgen_templates\": " + std::to_string(at(worldgen, 0)) + ",\n";
    out += "    \"worldgen_template_blocks\": " + std::to_string(at(worldgen, 1)) + ",\n";
    out += "    \"worldgen_assemblies\": " + std::to_string(at(worldgen, 2)) + ",\n";
    out += "    \"worldgen_assembly_pieces\": " + std::to_string(at(worldgen, 3)) + ",\n";
    out += "    \"cpu_percent_of_one_core\": " + number(sources_.cpuPercent()) + ",\n";
    out += "    \"heap_bytes_live\": " + orNull(sources_.liveHeapBytes()) + ",\n";
    out += "    \"process_resident_bytes\": " + orNull(sources_.residentBytes()) + "\n";
    out += "  },\n";

    out += "  \"ticks\": {\n";
    out += "    \"target_per_second\": 20,\n";
    out += "    \"per_second\": {\n";
    static constexpr std::int32_t kRates[] = {5, 10, 60, 300, 900};
    for (std::size_t index = 0; index < std::size(kRates); ++index) {
        const std::string_view comma = index + 1 == std::size(kRates) ? "" : ",";
        out += "      \"" + label(kRates[index]) +
               "\": " + number(tickDurations_->ticksPerSecond(nowNanos, kRates[index])) + std::string{comma} + "\n";
    }
    out += "    },\n";
    out += "    \"duration_millis\": {\n";
    static constexpr std::int32_t kWindows[] = {10, 60, 300};
    for (std::size_t index = 0; index < std::size(kWindows); ++index) {
        const std::string_view comma = index + 1 == std::size(kWindows) ? "" : ",";
        out += "      \"" + label(kWindows[index]) +
               "\": " + summaryOf(tickDurations_->summary(nowNanos, kWindows[index])) + std::string{comma} + "\n";
    }
    out += "    }\n";
    out += "  },\n";

    // A section nobody called is absent, not a zero row: a report is read to find what cost something.
    std::vector<wire::common::TimingSnapshot> sections;
    for (const wire::common::TimingSnapshot& section : timings_->snapshot()) {
        if (section.calls > 0) {
            sections.push_back(section);
        }
    }
    out += "  \"sections\": [\n";
    for (std::size_t index = 0; index < sections.size(); ++index) {
        const wire::common::TimingSnapshot& section = sections[index];
        const std::string_view comma = index + 1 == sections.size() ? "" : ",";
        out += "    {";
        out += "\"name\": " + quote(section.name) + ", ";
        out += "\"calls\": " + std::to_string(section.calls) + ", ";
        out += "\"total_millis\": " + number(static_cast<double>(section.totalNanos) / 1000000.0) + ", ";
        out += "\"average_millis\": " + number(static_cast<double>(section.averageNanos()) / 1000000.0) + ", ";
        out += "\"max_millis\": " + number(static_cast<double>(section.maxNanos) / 1000000.0);
        out += "}" + std::string{comma} + "\n";
    }
    out += "  ]\n";
    out += "}\n";
    return out;
}

} // namespace basalt::server::runtime::timings
