#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::runtime {

class ServerMetrics {
public:
    [[nodiscard]] std::optional<std::uint64_t> residentBytes() const;

    [[nodiscard]] std::uint64_t peakResidentBytes() const noexcept { return peak_.load(std::memory_order_relaxed); }

    [[nodiscard]] std::string memoryReport() const;

private:
    mutable std::atomic<std::uint64_t> peak_{0};
};

class CpuMeter {
public:
    CpuMeter();

    void prime();

    [[nodiscard]] std::optional<double> percent();

private:
    struct Sample {
        std::int64_t micros = 0;
        std::int64_t atMillis = 0;
        double percent = 0.0;
        /// A baseline reading has no percentage yet, which is not the same as a percentage of zero.
        bool measured = false;
        bool present = false;
    };

    std::chrono::steady_clock::time_point start_;
    std::atomic<Sample> last_;
};

} // namespace basalt::server::runtime
