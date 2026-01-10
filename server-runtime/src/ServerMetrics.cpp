#include <basalt/server/runtime/ServerMetrics.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

constexpr std::uint64_t kBytesPerMegabyte = 1'048'576;

/// Normally one reading a second is plenty.
constexpr std::int64_t kMinimumWindowMillis = 1000;

/// The exception is the very first answer after priming: holding out for a full second there would
/// report nothing at all for that second, which is exactly when somebody is looking at a freshly
/// started server.
constexpr std::int64_t kFirstWindowMillis = 50;

} // namespace

std::optional<std::uint64_t> ServerMetrics::residentBytes() const {
    const std::uint64_t bytes = wire::io::residentSetBytes();
    if (bytes == 0) {
        return std::nullopt;
    }
    std::uint64_t seen = peak_.load(std::memory_order_relaxed);
    while (bytes > seen && !peak_.compare_exchange_weak(seen, bytes, std::memory_order_relaxed)) {
    }
    return bytes;
}

std::string ServerMetrics::memoryReport() const {
    const std::optional<std::uint64_t> bytes = residentBytes();
    if (!bytes.has_value()) {
        return "This platform does not report how much memory the process is holding.";
    }
    return "Memory: " + std::to_string(*bytes / kBytesPerMegabyte) + " MB resident (peak " +
           std::to_string(peakResidentBytes() / kBytesPerMegabyte) + " MB).";
}

CpuMeter::CpuMeter() : start_(std::chrono::steady_clock::now()), last_(Sample{}) {}

void CpuMeter::prime() { static_cast<void>(percent()); }

std::optional<double> CpuMeter::percent() {
    const std::optional<std::chrono::microseconds> used = wire::io::processCpuTime();
    if (!used.has_value()) {
        return std::nullopt;
    }
    const auto micros = static_cast<std::int64_t>(used->count());
    const auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_).count();

    Sample previous = last_.load(std::memory_order_relaxed);
    if (!previous.present) {
        Sample expected = previous;
        static_cast<void>(last_.compare_exchange_strong(
            expected, Sample{.micros = micros, .atMillis = now, .present = true}, std::memory_order_relaxed));
        return std::nullopt;
    }
    const std::int64_t elapsed = now - previous.atMillis;
    const std::int64_t window = previous.measured ? kMinimumWindowMillis : kFirstWindowMillis;
    if (elapsed < window) {
        return previous.measured ? std::optional{previous.percent} : std::nullopt;
    }
    const double spent = static_cast<double>(micros - previous.micros) / 1000.0;
    const double percent = std::max(spent / static_cast<double>(elapsed) * 100.0, 0.0);
    static_cast<void>(last_.compare_exchange_strong(
        previous, Sample{.micros = micros, .atMillis = now, .percent = percent, .measured = true, .present = true},
        std::memory_order_relaxed));
    return percent;
}

} // namespace basalt::server::runtime
