#include <basalt/server/runtime/timings/TimingsReport.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <format>
#include <thread>

namespace basalt::server::runtime::timings {
namespace {

using wire::common::TickDurations;

constexpr std::int32_t kReportVersion = 3;

[[nodiscard]] double millisOf(std::int64_t nanos) { return static_cast<double>(nanos) / 1000000.0; }

/// Writes compact JSON, tracking where commas go so the callers cannot get them wrong.
class JsonWriter {
public:
    [[nodiscard]] const std::string& text() const noexcept { return out_; }

    void beginObject() { open('{'); }

    void endObject() { close('}'); }

    void beginArray() { open('['); }

    void endArray() { close(']'); }

    void key(std::string_view name) {
        separate();
        quote(name);
        out_ += ':';
        afterKey_ = true;
    }

    void null() { scalar("null"); }

    void value(bool flag) { scalar(flag ? "true" : "false"); }

    void value(std::int64_t number) { scalar(std::to_string(number)); }

    void value(std::int32_t number) { scalar(std::to_string(number)); }

    void value(std::size_t number) { scalar(std::to_string(number)); }

    void value(std::string_view text) {
        separate();
        quote(text);
    }

    void value(const char* text) { value(std::string_view{text}); }

    /// Three decimals, since most sections take well under a millisecond.
    void value(double number) {
        if (!std::isfinite(number)) {
            null();
            return;
        }
        const std::int64_t rounded = static_cast<std::int64_t>(std::llround(number * 1000.0));
        const std::string digits = std::format("{:03d}", std::abs(rounded % 1000));
        scalar(std::string{rounded < 0 ? "-" : ""} + std::to_string(std::abs(rounded / 1000)) + "." + digits);
    }

    template <typename Number>
    void value(const std::optional<Number>& number) {
        if (number.has_value()) {
            value(*number);
        } else {
            null();
        }
    }

    template <typename Value>
    void member(std::string_view name, const Value& v) {
        key(name);
        value(v);
    }

private:
    void open(char bracket) {
        separate();
        out_ += bracket;
        first_.push_back(true);
    }

    void close(char bracket) {
        first_.pop_back();
        out_ += bracket;
    }

    void separate() {
        if (afterKey_) {
            afterKey_ = false;
            return;
        }
        if (!first_.empty()) {
            if (!first_.back()) {
                out_ += ',';
            }
            first_.back() = false;
        }
    }

    void scalar(const std::string& text) {
        separate();
        out_ += text;
    }

    void quote(std::string_view text) {
        out_ += '"';
        for (const char character : text) {
            switch (character) {
            case '\\':
                out_ += "\\\\";
                break;
            case '"':
                out_ += "\\\"";
                break;
            case '\n':
                out_ += "\\n";
                break;
            case '\r':
                out_ += "\\r";
                break;
            case '\t':
                out_ += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(character) < 0x20) {
                    out_ += std::format("\\u{:04x}", static_cast<unsigned>(character));
                } else {
                    out_ += character;
                }
            }
        }
        out_ += '"';
    }

    std::string out_;
    std::vector<bool> first_;
    bool afterKey_ = false;
};

[[nodiscard]] std::string label(std::int32_t seconds) {
    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }
    if (seconds < 3600) {
        return std::to_string(seconds / 60) + "m";
    }
    return std::to_string(seconds / 3600) + "h";
}

template <typename Value>
[[nodiscard]] std::optional<Value> sourced(const std::function<Value()>& source) {
    if (!source) {
        return std::nullopt;
    }
    return source();
}

[[nodiscard]] std::int64_t at(const std::vector<std::int64_t>& values, std::size_t index) {
    return index < values.size() ? values[index] : 0;
}

void writeSummary(JsonWriter& json, const wire::common::DurationSummary& summary) {
    json.beginObject();
    json.member("samples", summary.samples);
    json.member("min", summary.minMillis);
    json.member("median", summary.medianMillis);
    json.member("p95", summary.percentile95Millis);
    json.member("max", summary.maxMillis);
    json.member("mean", summary.meanMillis);
    json.endObject();
}

void writeWindowSeries(JsonWriter& json, const std::vector<std::int64_t>& values, bool nanos) {
    json.beginArray();
    for (const std::int64_t entry : values) {
        if (nanos) {
            json.value(millisOf(entry));
        } else {
            json.value(entry);
        }
    }
    json.endArray();
}

void writeNode(JsonWriter& json, const wire::common::ProfileNode& node, bool withWindows) {
    json.beginObject();
    json.member("name", node.name);
    json.member("calls", node.calls);
    json.member("total_millis", millisOf(node.totalNanos));
    json.member("self_millis", millisOf(node.selfNanos()));
    json.member("max_millis", millisOf(node.maxNanos));
    if (withWindows) {
        json.key("window_calls");
        writeWindowSeries(json, node.windowCalls, false);
        json.key("window_millis");
        writeWindowSeries(json, node.windowNanos, true);
    }
    json.key("children");
    json.beginArray();
    for (const wire::common::ProfileNode& child : node.children) {
        writeNode(json, child, withWindows);
    }
    json.endArray();
    json.endObject();
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
    wire::io::fileIO::writeText(path, json);
    return path;
}

bool TimingsReport::hasProfile() const { return !timings_->profiles().empty(); }

void TimingsReport::sampleIfDue(std::int64_t nowNanos) const {
    if (!timings_->enabled()) {
        return;
    }
    const std::int64_t origin = nowNanos - timings_->windowNanos(nowNanos);
    {
        const std::scoped_lock lock{seriesMutex_};
        if (origin != seriesOriginNanos_) {
            // A new run: the old points belong to a profile that was thrown away.
            series_.clear();
            seriesOriginNanos_ = origin;
            lastSampleNanos_ = nowNanos;
            startEpochMillis_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();
            return;
        }
        if (nowNanos - lastSampleNanos_ < TickDurations::kNanosPerSecond) {
            return;
        }
        lastSampleNanos_ = nowNanos;
    }

    Sample sample;
    sample.atSeconds = (nowNanos - origin) / TickDurations::kNanosPerSecond;
    const std::vector<wire::common::TickBucket> bucket = tickDurations_->timeline(nowNanos, 1);
    if (!bucket.empty()) {
        sample.ticks = bucket.front().ticks;
        sample.msptMean = bucket.front().meanMillis;
        sample.msptMax = bucket.front().maxMillis;
    }
    if (sources_.live) {
        const LiveFacts live = sources_.live();
        sample.players = live.players;
        sample.entities = live.entities;
        sample.chunks = live.chunks;
        sample.cpuProcess = live.cpuProcessPercentOfOneCore;
        sample.cpuSystem = live.cpuSystemPercent;
        sample.resident = live.residentBytes;
    }
    const std::scoped_lock lock{seriesMutex_};
    series_.push_back(sample);
    if (series_.size() > kMaxSamples) {
        series_.erase(series_.begin());
    }
}

std::string TimingsReport::json(std::int64_t nowNanos, std::string_view user) const {
    const std::int64_t measuredSeconds = timings_->windowNanos(nowNanos) / TickDurations::kNanosPerSecond;
    const SystemFacts system = readSystemFacts(sources_.diskPath);
    const auto cores = static_cast<double>(std::max<std::int32_t>(system.cpuThreads, 1));
    const std::int64_t nowEpoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    JsonWriter json;
    json.beginObject();
    json.member("format", "basalt-timings");
    json.member("version", kReportVersion);

    json.key("metadata");
    json.beginObject();
    json.member("user", user.empty() ? std::string_view{"Console"} : user);
    json.member("generated", stamp());
    json.member("end_epoch_millis", nowEpoch);
    json.member("start_epoch_millis", nowEpoch - measuredSeconds * 1000);
    json.member("measuring", timings_->enabled());
    json.member("measured_seconds", measuredSeconds);
    json.member("window_seconds", wire::common::Timings::kWindowSeconds);
    json.key("platform");
    json.beginObject();
    json.member("brand", sources_.brand);
    json.member("version", sources_.buildVersion);
    json.member("minecraft", sources_.versionName);
    json.member("protocol", sources_.protocolVersion);
    json.endObject();
    json.endObject();

    json.key("system");
    json.beginObject();
    json.member("os", system.osName);
    json.member("os_version", system.osVersion);
    json.member("arch", system.arch);
    json.member("cpu_model", system.cpuModel);
    json.member("cpu_threads", system.cpuThreads);
    json.member("memory_used", system.memoryUsed);
    json.member("memory_total", system.memoryTotal);
    json.member("swap_used", system.swapUsed);
    json.member("swap_total", system.swapTotal);
    json.member("disk_used", system.diskUsed);
    json.member("disk_total", system.diskTotal);
    json.endObject();

    json.key("server");
    json.beginObject();
    json.member("uptime_seconds", sourced(sources_.uptimeSeconds));
    json.member("players_online", sourced(sources_.onlinePlayers));
    json.member("players_max", sourced(sources_.maxPlayers));
    json.member("worlds", sourced(sources_.worldCount));
    json.member("chunks_loaded", sourced(sources_.loadedChunks));
    json.member("region_cache_bytes", sourced(sources_.regionCacheBytes));
    json.member("chunk_bytes", sourced(sources_.chunkBytes));
    json.member("chunk_block_entities", sourced(sources_.chunkBlockEntities));
    json.member("chunk_scheduled_ticks", sourced(sources_.chunkScheduledTicks));
    json.member("chunk_generated_entities", sourced(sources_.chunkGeneratedEntities));
    const std::vector<std::int64_t> worldgen =
        sources_.worldgenCaches ? sources_.worldgenCaches() : std::vector<std::int64_t>{};
    json.member("worldgen_templates", at(worldgen, 0));
    json.member("worldgen_template_blocks", at(worldgen, 1));
    json.member("worldgen_assemblies", at(worldgen, 2));
    json.member("worldgen_assembly_pieces", at(worldgen, 3));
    const std::optional<double> processCpu = sources_.cpuPercent ? sources_.cpuPercent() : std::nullopt;
    json.member("cpu_percent_of_one_core", processCpu);
    json.member("cpu_process_percent", processCpu.has_value() ? std::optional{*processCpu / cores} : std::nullopt);
    json.member("cpu_system_percent", sources_.systemCpuPercent ? sources_.systemCpuPercent() : std::nullopt);
    json.member("heap_bytes_live", sources_.liveHeapBytes ? sources_.liveHeapBytes() : std::nullopt);
    json.member("process_resident_bytes", sources_.residentBytes ? sources_.residentBytes() : std::nullopt);
    json.endObject();

    json.key("world_statistics");
    json.beginObject();
    const std::vector<WorldFacts> worlds = sources_.worlds ? sources_.worlds() : std::vector<WorldFacts>{};
    std::map<std::string, std::int32_t> totalByType;
    std::int32_t totalEntities = 0;
    for (const WorldFacts& world : worlds) {
        totalEntities += world.entities;
        for (const auto& [type, count] : world.entityCounts) {
            totalByType[type] += count;
        }
    }
    json.member("total_entities", totalEntities);
    json.key("entity_counts");
    json.beginObject();
    for (const auto& [type, count] : totalByType) {
        json.member(type, count);
    }
    json.endObject();
    json.key("worlds");
    json.beginArray();
    for (const WorldFacts& world : worlds) {
        json.beginObject();
        json.member("name", world.name);
        json.member("chunks", world.loadedChunks);
        json.member("entities", world.entities);
        json.member("block_entities", world.blockEntities);
        json.member("players", world.players);
        json.key("entity_counts");
        json.beginObject();
        for (const auto& [type, count] : world.entityCounts) {
            json.member(type, count);
        }
        json.endObject();
        json.endObject();
    }
    json.endArray();
    json.endObject();

    json.key("configuration");
    json.beginObject();
    if (sources_.configuration) {
        for (const auto& [name, setting] : sources_.configuration()) {
            json.member(name, setting);
        }
    }
    json.endObject();

    json.key("plugins");
    json.beginArray();
    if (sources_.plugins) {
        for (const PluginFacts& plugin : sources_.plugins()) {
            json.beginObject();
            json.member("id", plugin.id);
            json.member("name", plugin.name);
            json.member("version", plugin.version);
            json.member("errors", plugin.errors);
            json.member("disabled", plugin.disabled);
            json.endObject();
        }
    }
    json.endArray();

    json.key("ticks");
    json.beginObject();
    json.member("target_per_second", 20);
    json.key("per_second");
    json.beginObject();
    for (const std::int32_t window : {5, 10, 60, 300, 900}) {
        json.member(label(window), tickDurations_->ticksPerSecond(nowNanos, window));
    }
    json.endObject();
    json.key("duration_millis");
    json.beginObject();
    for (const std::int32_t window : {10, 60, 300}) {
        json.key(label(window));
        writeSummary(json, tickDurations_->summary(nowNanos, window));
    }
    json.endObject();
    json.endObject();

    json.key("metrics");
    json.beginObject();
    {
        const std::scoped_lock lock{seriesMutex_};
        json.member("interval_seconds", 1);
        const auto column = [&](std::string_view name, auto&& pick) {
            json.key(name);
            json.beginArray();
            for (const Sample& sample : series_) {
                json.value(pick(sample));
            }
            json.endArray();
        };
        column("seconds", [](const Sample& s) { return s.atSeconds; });
        column("tps", [](const Sample& s) { return std::min(static_cast<double>(s.ticks), 20.0); });
        column("mspt_mean", [](const Sample& s) { return s.msptMean; });
        column("mspt_max", [](const Sample& s) { return s.msptMax; });
        column("cpu_process", [cores](const Sample& s) {
            return s.cpuProcess.has_value() ? std::optional{*s.cpuProcess / cores} : std::nullopt;
        });
        column("cpu_system", [](const Sample& s) { return s.cpuSystem; });
        column("memory_resident", [](const Sample& s) { return s.resident; });
        column("players", [](const Sample& s) { return s.players; });
        column("entities", [](const Sample& s) { return s.entities; });
        column("chunks", [](const Sample& s) { return s.chunks; });
    }
    json.endObject();

    std::vector<wire::common::TimingSnapshot> sections;
    for (const wire::common::TimingSnapshot& section : timings_->snapshot()) {
        if (section.calls > 0) {
            sections.push_back(section);
        }
    }
    json.key("sections");
    json.beginArray();
    for (const wire::common::TimingSnapshot& section : sections) {
        json.beginObject();
        json.member("name", section.name);
        json.member("calls", section.calls);
        json.member("total_millis", millisOf(section.totalNanos));
        json.member("average_millis", millisOf(section.averageNanos()));
        json.member("max_millis", millisOf(section.maxNanos));
        json.endObject();
    }
    json.endArray();

    const std::vector<wire::common::ThreadProfile> threads = timings_->profiles();
    const bool withWindows = std::ranges::any_of(
        threads, [](const wire::common::ThreadProfile& thread) { return thread.root.windowNanos.size() > 1; });
    json.key("threads");
    json.beginArray();
    for (const wire::common::ThreadProfile& thread : threads) {
        json.beginObject();
        json.member("name", thread.name);
        json.member("total_millis", millisOf(thread.root.totalNanos));
        json.key("root");
        writeNode(json, thread.root, withWindows);
        json.endObject();
    }
    json.endArray();

    json.endObject();
    return json.text() + "\n";
}

} // namespace basalt::server::runtime::timings
