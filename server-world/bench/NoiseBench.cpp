// Samples a dense grid of NormalNoise, the shape a density function evaluates during world
// generation, and reports nanoseconds per sample. The number it prints is meant to be read next to
// the same measurement taken on the frozen Kotlin server.

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace basalt::server::random;
using namespace basalt::server::world::noise;

namespace {

constexpr int kSide = 64;
constexpr int kWarmupPasses = 2;
constexpr auto kMeasureFor = std::chrono::seconds{3};

double sweep(const NormalNoise& noise) {
    double sink = 0.0;
    for (int x = 0; x < kSide; ++x) {
        for (int y = 0; y < kSide; ++y) {
            for (int z = 0; z < kSide; ++z) {
                sink += noise.getValue(x * 0.31, y * 0.31, z * 0.31);
            }
        }
    }
    return sink;
}

} // namespace

int main() {
    RandomSource source = RandomSource::xoroshiro(1);
    const std::vector<double> amplitudes(8, 1.0);
    const NormalNoise noise = NormalNoise::create(source, -7, amplitudes);

    double sink = 0.0;
    for (int i = 0; i < kWarmupPasses; ++i) {
        sink += sweep(noise);
    }

    const auto start = std::chrono::steady_clock::now();
    std::int64_t passes = 0;
    while (std::chrono::steady_clock::now() - start < kMeasureFor) {
        sink += sweep(noise);
        ++passes;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    const std::int64_t samples = passes * kSide * kSide * kSide;
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    std::printf("samples=%lld ms=%lld nsPerSample=%.2f sink=%.6f\n", static_cast<long long>(samples),
                static_cast<long long>(nanos / 1000000), static_cast<double>(nanos) / static_cast<double>(samples),
                sink);
    return 0;
}
