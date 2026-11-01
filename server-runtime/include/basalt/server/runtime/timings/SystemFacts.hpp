#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace basalt::server::runtime::timings {

/// What the machine reports about itself. Anything the platform will not say is left empty, never guessed.
struct SystemFacts {
    std::string osName;
    std::string osVersion;
    std::string arch;
    std::string cpuModel;
    std::int32_t cpuThreads = 0;
    std::optional<std::int64_t> memoryUsed;
    std::optional<std::int64_t> memoryTotal;
    std::optional<std::int64_t> swapUsed;
    std::optional<std::int64_t> swapTotal;
    std::optional<std::int64_t> diskUsed;
    std::optional<std::int64_t> diskTotal;
};

[[nodiscard]] SystemFacts readSystemFacts(const std::string& diskPath);

/// Whole-machine CPU use since the previous call, so the first call has nothing to compare against.
class SystemCpuMeter {
public:
    [[nodiscard]] std::optional<double> percent();

private:
    std::mutex mutex_;
    std::uint64_t busy_ = 0;
    std::uint64_t total_ = 0;
    bool primed_ = false;
};

} // namespace basalt::server::runtime::timings
