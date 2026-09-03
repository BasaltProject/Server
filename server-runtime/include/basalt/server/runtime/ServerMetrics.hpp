#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::runtime {

/// What `/timings` and the server-facts placeholders report about memory. Split out of
/// `BasaltServer` because none of it depends on anything else the server owns.
///
/// There is no garbage collector here, so the figure is the process's resident set rather than a
/// live heap after a collection: what the operating system says this process is holding, which is
/// also what an operator sees in `top` and can check the report against.
class ServerMetrics {
public:
    /// Resident bytes, or nothing when the platform will not say.
    [[nodiscard]] std::optional<std::uint64_t> residentBytes() const;

    /// The high-water mark since the process started, which is what an out-of-memory kill is judged
    /// against. Updated by every call to `residentBytes`, so a server that has been polled for its
    /// facts has a real peak rather than only the reading taken when someone asked for it.
    [[nodiscard]] std::uint64_t peakResidentBytes() const noexcept { return peak_.load(std::memory_order_relaxed); }

    [[nodiscard]] std::string memoryReport() const;

private:
    mutable std::atomic<std::uint64_t> peak_{0};
};

/// CPU usage as a percentage of one core, `top`'s convention: eight busy workers read as roughly
/// 800, which is the figure an operator can check against their own machine.
///
/// The percentage only exists between two readings, so the last one is kept and the average over the
/// gap reported. Every connection thread reads this, hence the compare-and-swap: two samples at once
/// must not both subtract from a half-updated baseline.
class CpuMeter {
public:
    CpuMeter();

    /// Takes the first reading, so the first caller gets a number instead of the nothing that
    /// establishing a baseline would otherwise return.
    void prime();

    [[nodiscard]] std::optional<double> percent();

private:
    /// One reading: the CPU time seen, when it was seen, and what it worked out to.
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
