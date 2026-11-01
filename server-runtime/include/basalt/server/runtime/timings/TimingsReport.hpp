#pragma once

#include <basalt/wire/common/timings/TickDurations.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::timings {

struct WorldFacts {
    std::string name;
    std::int32_t loadedChunks = 0;
    std::int32_t entities = 0;
    std::int32_t players = 0;
};

/// Every source is optional: one left empty is reported as absent instead of failing the whole report.
struct ReportSources {
    std::string versionName;
    std::int32_t protocolVersion = 0;
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
    std::function<std::vector<WorldFacts>()> worlds;
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

    [[nodiscard]] std::string json(std::int64_t nowNanos) const;

    /// What a viewer needs to tell a report from another: the time it covers and what ran in it.
    [[nodiscard]] bool hasProfile() const;

    [[nodiscard]] static std::string stamp();

private:
    const wire::common::Timings* timings_;
    const wire::common::TickDurations* tickDurations_;
    ReportSources sources_;
    std::string directory_;
};

} // namespace basalt::server::runtime::timings
