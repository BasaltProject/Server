#include <basalt/server/runtime/timings/TimingsReport.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <algorithm>
#include <format>
#include <thread>

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
        switch (character) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                out += std::format("\\u{:04x}", static_cast<unsigned>(character));
            } else {
                out += character;
            }
        }
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

std::string TimingsReport::write(std::int64_t nowNanos) const { return save(json(nowNanos)); }

std::string TimingsReport::save(std::string_view json) const {
    wire::io::fileIO::createDirectories(directory_);
    const std::string path = directory_ + "/" + stamp() + ".json";
    wire::io::fileIO::writeText(path, std::string{json});
    return path;
}

bool TimingsReport::hasProfile() const {
    return !timings_->profiles().empty();
}

namespace {

[[nodiscard]] double millisOf(std::int64_t nanos) { return static_cast<double>(nanos) / 1000000.0; }

template <typename Value>
[[nodiscard]] auto sourced(const std::function<Value()>& source) {
    return source ? std::optional<Value>{source()} : std::nullopt;
}

[[nodiscard]] std::string orZero(const std::optional<std::int32_t>& value) {
    return value.has_value() ? std::to_string(*value) : "null";
}

void writeNode(std::string& out, const wire::common::ProfileNode& node, std::int32_t depth) {
    const std::string pad(static_cast<std::size_t>(depth) * 2, ' ');
    out += pad + "{\"name\": " + quote(node.name) + ", \"calls\": " + std::to_string(node.calls) +
           ", \"total_millis\": " + number(millisOf(node.totalNanos)) +
           ", \"self_millis\": " + number(millisOf(node.selfNanos())) +
           ", \"max_millis\": " + number(millisOf(node.maxNanos)) + ", \"children\": [";
    if (node.children.empty()) {
        out += "]}";
        return;
    }
    out += "\n";
    for (std::size_t index = 0; index < node.children.size(); ++index) {
        writeNode(out, node.children[index], depth + 1);
        out += index + 1 == node.children.size() ? "\n" : ",\n";
    }
    out += pad + "]}";
}

[[nodiscard]] std::string_view osName() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

[[nodiscard]] std::string_view archName() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#else
    return "unknown";
#endif
}

} // namespace

std::string TimingsReport::json(std::int64_t nowNanos) const {
    const std::int64_t measuredSeconds = timings_->windowNanos(nowNanos) / TickDurations::kNanosPerSecond;
    std::string out;
    out += "{\n";
    out += "  \"format\": \"basalt-timings\",\n";
    out += "  \"version\": 2,\n";
    out += "  \"basalt\": " + quote(sources_.versionName) + ",\n";
    out += "  \"protocol\": " + std::to_string(sources_.protocolVersion) + ",\n";
    out += "  \"generated\": " + quote(stamp()) + ",\n";
    out += "  \"generated_epoch_millis\": " +
           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count()) +
           ",\n";
    out += "  \"uptime_seconds\": " + orZero(sourced(sources_.uptimeSeconds).transform([](std::int64_t value) {
        return static_cast<std::int32_t>(value);
    })) + ",\n";
    out += std::string{"  \"measuring\": "} + (timings_->enabled() ? "true" : "false") + ",\n";
    out += "  \"measured_seconds\": " + std::to_string(measuredSeconds) + ",\n";
    out += "  \"platform\": {\"os\": " + quote(osName()) + ", \"arch\": " + quote(archName()) +
           ", \"cores\": " + std::to_string(std::thread::hardware_concurrency()) + "},\n";

    out += "  \"server\": {\n";
    out += "    \"players_online\": " + orZero(sourced(sources_.onlinePlayers)) + ",\n";
    out += "    \"players_max\": " + orZero(sourced(sources_.maxPlayers)) + ",\n";
    out += "    \"worlds\": " + orZero(sourced(sources_.worldCount)) + ",\n";
    out += "    \"chunks_loaded\": " + orZero(sourced(sources_.loadedChunks)) + ",\n";
    out += "    \"region_cache_bytes\": " + orNull(sourced(sources_.regionCacheBytes)) + ",\n";
    out += "    \"chunk_bytes\": " + orNull(sourced(sources_.chunkBytes)) + ",\n";
    out += "    \"chunk_block_entities\": " + orZero(sourced(sources_.chunkBlockEntities)) + ",\n";
    out += "    \"chunk_scheduled_ticks\": " + orZero(sourced(sources_.chunkScheduledTicks)) + ",\n";
    out += "    \"chunk_generated_entities\": " + orZero(sourced(sources_.chunkGeneratedEntities)) + ",\n";
    const std::vector<std::int64_t> worldgen = sources_.worldgenCaches ? sources_.worldgenCaches() : std::vector<std::int64_t>{};
    out += "    \"worldgen_templates\": " + std::to_string(at(worldgen, 0)) + ",\n";
    out += "    \"worldgen_template_blocks\": " + std::to_string(at(worldgen, 1)) + ",\n";
    out += "    \"worldgen_assemblies\": " + std::to_string(at(worldgen, 2)) + ",\n";
    out += "    \"worldgen_assembly_pieces\": " + std::to_string(at(worldgen, 3)) + ",\n";
    out += "    \"cpu_percent_of_one_core\": " +
           number(sources_.cpuPercent ? sources_.cpuPercent() : std::optional<double>{}) + ",\n";
    out += "    \"heap_bytes_live\": " + orNull(sources_.liveHeapBytes ? sources_.liveHeapBytes() : std::nullopt) + ",\n";
    out += "    \"process_resident_bytes\": " +
           orNull(sources_.residentBytes ? sources_.residentBytes() : std::nullopt) + ",\n";
    out += "    \"worlds_detail\": [";
    const std::vector<WorldFacts> worlds = sources_.worlds ? sources_.worlds() : std::vector<WorldFacts>{};
    for (std::size_t index = 0; index < worlds.size(); ++index) {
        out += "{\"name\": " + quote(worlds[index].name) + ", \"chunks\": " + std::to_string(worlds[index].loadedChunks) +
               ", \"entities\": " + std::to_string(worlds[index].entities) +
               ", \"players\": " + std::to_string(worlds[index].players) + "}" +
               (index + 1 == worlds.size() ? "" : ", ");
    }
    out += "]\n";
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
    out += "    },\n";
    // One point per second over what was measured, capped at what the ring remembers.
    const auto window = static_cast<std::int32_t>(std::clamp<std::int64_t>(measuredSeconds, 1, 300));
    const std::vector<wire::common::TickBucket> timeline = tickDurations_->timeline(nowNanos, window);
    out += "    \"timeline\": {\"seconds\": " + std::to_string(window) + ", \"points\": [";
    for (std::size_t index = 0; index < timeline.size(); ++index) {
        out += "{\"ticks\": " + std::to_string(timeline[index].ticks) +
               ", \"mean_millis\": " + number(timeline[index].meanMillis) +
               ", \"max_millis\": " + number(timeline[index].maxMillis) + "}" +
               (index + 1 == timeline.size() ? "" : ", ");
    }
    out += "]}\n";
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
    out += "  ],\n";

    // Where the time went, as the sections were nested while they ran, one tree per thread.
    const std::vector<wire::common::ThreadProfile> threads = timings_->profiles();
    out += "  \"threads\": [\n";
    for (std::size_t index = 0; index < threads.size(); ++index) {
        out += "    {\"name\": " + quote(threads[index].name) +
               ", \"total_millis\": " + number(millisOf(threads[index].root.totalNanos)) + ", \"root\":\n";
        writeNode(out, threads[index].root, 3);
        out += std::string{"}"} + (index + 1 == threads.size() ? "" : ",") + "\n";
    }
    out += "  ]\n";
    out += "}\n";
    return out;
}

} // namespace basalt::server::runtime::timings
