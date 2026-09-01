#pragma once

#include <basalt/wire/common/timings/TickDurations.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::timings {

/// What the report can ask the running server about itself, so this file needs nothing global.
struct ReportSources {
    std::string versionName;
    std::int32_t protocolVersion = 0;
    std::function<std::int64_t()> uptimeSeconds;
    std::function<std::int32_t()> onlinePlayers;
    std::function<std::int32_t()> maxPlayers;
    std::function<std::int32_t()> loadedChunks;
    /// Bytes of decompressed region files held in memory, which chunk counts do not account for.
    std::function<std::int64_t()> regionCacheBytes;
    /// What the loaded chunks themselves keep resident, so heap growth can be attributed or ruled
    /// out.
    std::function<std::int64_t()> chunkBytes;
    std::function<std::int32_t()> chunkBlockEntities;
    std::function<std::int32_t()> chunkScheduledTicks;
    std::function<std::int32_t()> chunkGeneratedEntities;
    /// Worldgen structure caches: templates, template blocks, assemblies, assembly pieces.
    std::function<std::vector<std::int64_t>()> worldgenCaches;
    std::function<std::int32_t()> worldCount;
    std::function<std::optional<double>()> cpuPercent;
    std::function<std::optional<std::int64_t>()> liveHeapBytes;
    /// What the OS says this process occupies, the figure operators compare servers by.
    std::function<std::optional<std::int64_t>()> residentBytes;
};

/// The `.json` a `/timings` run leaves behind.
///
/// Written locally and never uploaded anywhere. spark posts its reports to a viewer, which is the
/// right call for a plugin that cannot ask an operator to read protobuf; a file on the server's own
/// disk owes nobody a round trip, and a performance report is exactly the sort of thing that
/// silently carries the world seed, player names and file paths off the machine.
///
/// The shape is deliberately flat and self-describing: what a section cost, how often it ran, and
/// the distribution of tick durations behind it. Anything reading it can do so without a schema.
class TimingsReport {
public:
    /// Beside the logs and the asset cache, in the directory the server already owns.
    static constexpr std::string_view kDefaultDirectory = "./.basalt/timings";

    TimingsReport(const wire::common::Timings& timings, const wire::common::TickDurations& tickDurations,
                  ReportSources sources)
        : timings_(&timings), tickDurations_(&tickDurations), sources_(std::move(sources)) {}

    /// Writes the report and answers its path.
    ///
    /// The file is named after local time to the second, which is how an operator refers to a run
    /// ("the one from just before it lagged") and what keeps two runs a minute apart from colliding.
    [[nodiscard]] std::string write(std::int64_t nowNanos,
                                    const std::string& directory = std::string{kDefaultDirectory}) const;

    [[nodiscard]] std::string json(std::int64_t nowNanos) const;

    /// The local timestamp the file is named after, exposed so a caller can name something beside it.
    [[nodiscard]] static std::string stamp();

private:
    const wire::common::Timings* timings_;
    const wire::common::TickDurations* tickDurations_;
    ReportSources sources_;
};

} // namespace basalt::server::runtime::timings
