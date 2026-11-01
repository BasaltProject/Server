#pragma once

#include <basalt/server/runtime/timings/SystemFacts.hpp>
#include <basalt/wire/common/timings/TickDurations.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::timings {

struct WorldFacts {
    std::string name;
    std::int32_t loadedChunks = 0;
    std::int32_t entities = 0;
    std::int32_t blockEntities = 0;
    std::int32_t players = 0;
    std::map<std::string, std::int32_t> entityCounts;
};

struct PluginFacts {
    std::string id;
    std::string name;
    std::string version;
    std::int32_t errors = 0;
    bool disabled = false;
};

/// What is true right now, sampled once a second while the profiler runs.
struct LiveFacts {
    std::int32_t players = 0;
    std::int32_t entities = 0;
    std::int32_t chunks = 0;
    std::optional<double> cpuProcessPercentOfOneCore;
    std::optional<double> cpuSystemPercent;
    std::optional<std::int64_t> residentBytes;
};

/// Every source is optional: one left empty is reported as absent instead of failing the whole report.
struct ReportSources {
    std::string versionName;
    std::string buildVersion;
    std::string brand = "Basalt";
    std::int32_t protocolVersion = 0;
    std::string diskPath = ".";
    std::function<std::int64_t()> uptimeSeconds;
    std::function<std::int32_t()> onlinePlayers;
    std::function<std::int32_t()> maxPlayers;
    std::function<std::int32_t()> loadedChunks;
    /// Bytes of decompressed region files held in memory, which chunk counts do not account for.
    std::function<std::int64_t()> regionCacheBytes;
    std::function<std::int64_t()> chunkBytes;
    std::function<std::int32_t()> chunkBlockEntities;
    std::function<std::int32_t()> chunkScheduledTicks;
    std::function<std::int32_t()> chunkGeneratedEntities;
    std::function<std::vector<std::int64_t>()> worldgenCaches;
    std::function<std::int32_t()> worldCount;
    std::function<std::optional<double>()> cpuPercent;
    std::function<std::optional<std::int64_t>()> liveHeapBytes;
    std::function<std::optional<std::int64_t>()> residentBytes;
    std::function<std::optional<double>()> systemCpuPercent;
    std::function<std::vector<WorldFacts>()> worlds;
    std::function<std::vector<PluginFacts>()> plugins;
    std::function<std::vector<std::pair<std::string, std::string>>()> configuration;
    std::function<LiveFacts()> live;
};

class TimingsReport {
public:
    static constexpr std::string_view kDefaultDirectory = "./.basalt/timings";

    TimingsReport(const wire::common::Timings& timings, const wire::common::TickDurations& tickDurations,
                  ReportSources sources, std::string directory = std::string{kDefaultDirectory})
        : timings_(&timings), tickDurations_(&tickDurations), sources_(std::move(sources)),
          directory_(std::move(directory)) {}

    [[nodiscard]] const std::string& directory() const noexcept { return directory_; }

    [[nodiscard]] std::string write(std::int64_t nowNanos) const;

    /// Saves a report already built, so what is uploaded and what is kept on disk are the same document.
    [[nodiscard]] std::string save(std::string_view json) const;

    /// `user` is who asked for the profile, or empty for the console.
    [[nodiscard]] std::string json(std::int64_t nowNanos, std::string_view user = {}) const;

    /// What a viewer needs to tell a report from another: the time it covers and what ran in it.
    [[nodiscard]] bool hasProfile() const;

    /// Records one point of the time series if a second has passed since the last. Called from the tick thread
    /// while profiling, and a no-op otherwise.
    void sampleIfDue(std::int64_t nowNanos) const;

    [[nodiscard]] static std::string stamp();

private:
    struct Sample {
        std::int64_t atSeconds = 0;
        std::int32_t ticks = 0;
        double msptMean = 0.0;
        double msptMax = 0.0;
        std::int32_t players = 0;
        std::int32_t entities = 0;
        std::int32_t chunks = 0;
        std::optional<double> cpuProcess;
        std::optional<double> cpuSystem;
        std::optional<std::int64_t> resident;
    };

    static constexpr std::size_t kMaxSamples = 60 * 60 * 2;

    const wire::common::Timings* timings_;
    const wire::common::TickDurations* tickDurations_;
    ReportSources sources_;
    std::string directory_;

    mutable std::mutex seriesMutex_;
    mutable std::vector<Sample> series_;
    mutable std::int64_t seriesOriginNanos_ = -1;
    mutable std::int64_t lastSampleNanos_ = 0;
    mutable std::int64_t startEpochMillis_ = 0;
};

} // namespace basalt::server::runtime::timings
