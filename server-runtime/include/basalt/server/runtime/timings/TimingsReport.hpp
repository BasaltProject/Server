#pragma once

#include <basalt/wire/common/timings/TickDurations.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::timings {

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

    [[nodiscard]] std::string json(std::int64_t nowNanos) const;

    [[nodiscard]] static std::string stamp();

private:
    const wire::common::Timings* timings_;
    const wire::common::TickDurations* tickDurations_;
    ReportSources sources_;
    std::string directory_;
};

} // namespace basalt::server::runtime::timings
