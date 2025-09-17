#include <basalt/server/world/terrain/BiomeZoomer.hpp>

namespace basalt::server::world::terrain {
namespace {

constexpr std::int64_t kMultiplier = 6364136223846793005LL;
constexpr std::int64_t kIncrement = 1442695040888963407LL;

/// Java wraps on overflow; C++ signed overflow is undefined, so the arithmetic goes unsigned.
[[nodiscard]] std::int64_t next(std::int64_t value, std::int64_t addition) noexcept {
    const auto raw = static_cast<std::uint64_t>(value);
    const std::uint64_t stepped =
        raw * (raw * static_cast<std::uint64_t>(kMultiplier) + static_cast<std::uint64_t>(kIncrement)) +
        static_cast<std::uint64_t>(addition);
    return static_cast<std::int64_t>(stepped);
}

[[nodiscard]] double fiddle(std::int64_t value) noexcept {
    const std::int64_t shifted = value >> 24;
    const std::int64_t remainder = shifted % 1024LL;
    const std::int64_t positive = remainder < 0 ? remainder + 1024LL : remainder;
    return (static_cast<double>(positive) / 1024.0 - 0.5) * 0.9;
}

} // namespace

double BiomeZoomer::fiddledDistance(std::int64_t seed, std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                    double distanceX, double distanceY, double distanceZ) noexcept {
    std::int64_t random = seed;
    random = next(random, quartX);
    random = next(random, quartY);
    random = next(random, quartZ);
    random = next(random, quartX);
    random = next(random, quartY);
    random = next(random, quartZ);
    const double offsetX = fiddle(random);
    random = next(random, seed);
    const double offsetY = fiddle(random);
    random = next(random, seed);
    const double offsetZ = fiddle(random);
    const double x = distanceX + offsetX;
    const double y = distanceY + offsetY;
    const double z = distanceZ + offsetZ;
    return x * x + y * y + z * z;
}

} // namespace basalt::server::world::terrain
