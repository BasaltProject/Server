#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/blending/Blender.hpp>
#include <basalt/server/world/noise/Noise.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace basalt::server::world::blending {
namespace {

constexpr std::int32_t kHeightBlendingRangeCells = 27;

[[nodiscard]] float length(float x, float y) {
    return static_cast<float>(std::sqrt(static_cast<double>(x * x + y * y)));
}

[[nodiscard]] float length(float x, float y, float z) {
    return static_cast<float>(std::sqrt(static_cast<double>(x * x + y * y + z * z)));
}

[[nodiscard]] double lengthD(double x, double y, double z) { return std::sqrt(x * x + y * y + z * z); }

[[nodiscard]] double clamp(double value, double low, double high) { return std::clamp(value, low, high); }

[[nodiscard]] const noise::NoiseStack& shiftNoise() {
    static const std::shared_ptr<const noise::NoiseStack> kNoise = [] {
        const std::array<double, 4> amplitudes{1.0, 1.0, 1.0, 0.0};
        random::RandomSource random = random::RandomSource::xoroshiro(42);
        return noise::NormalNoise::createParity(-3, amplitudes).create(random);
    }();
    return *kNoise;
}

[[nodiscard]] double heightToOffset(double height) {
    const double targetY = height + 0.5;
    const double targetYMod = std::fmod(std::fmod(targetY, 8.0) + 8.0, 8.0);
    return 1.0 * (32.0 * (targetY - 128.0) - 3.0 * (targetY - 120.0) * targetYMod + 3.0 * targetYMod * targetYMod) /
           (128.0 * (32.0 - 3.0 * targetYMod));
}

[[nodiscard]] std::int64_t pack(std::int32_t x, std::int32_t z) {
    return (static_cast<std::int64_t>(x) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(z) << 32);
}

[[nodiscard]] std::int32_t fastutilMix(std::int64_t x) {
    auto h = static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ULL;
    h ^= h >> 32;
    return static_cast<std::int32_t>(h ^ (h >> 16));
}

[[nodiscard]] std::int32_t arraySize(std::int64_t expected) {
    const auto needed = static_cast<std::int64_t>(std::ceil(static_cast<double>(expected) / 0.75));
    std::int64_t size = 2;
    while (size < needed) {
        size <<= 1;
    }
    return static_cast<std::int32_t>(size);
}

[[nodiscard]] std::int32_t maxFill(std::int32_t n) {
    return std::min(static_cast<std::int32_t>(std::ceil(static_cast<double>(n) * 0.75)), n - 1);
}

/// The indices of `keys` in the order `Long2ObjectOpenHashMap.forEach` visits them after inserting them in order.
[[nodiscard]] std::vector<std::size_t> fastutilOrder(const std::vector<std::int64_t>& keys) {
    std::int32_t n = arraySize(16);
    std::int32_t fill = maxFill(n);
    std::vector<std::int64_t> table(static_cast<std::size_t>(n), 0);
    std::vector<std::size_t> payload(static_cast<std::size_t>(n), 0);
    std::optional<std::size_t> nullPayload;
    std::int32_t size = 0;
    const auto place = [](std::vector<std::int64_t>& slots, std::vector<std::size_t>& values, std::int64_t key,
                          std::size_t value) {
        const auto mask = static_cast<std::int32_t>(slots.size()) - 1;
        std::int32_t pos = fastutilMix(key) & mask;
        while (slots[static_cast<std::size_t>(pos)] != 0) {
            pos = (pos + 1) & mask;
        }
        slots[static_cast<std::size_t>(pos)] = key;
        values[static_cast<std::size_t>(pos)] = value;
    };
    for (std::size_t index = 0; index < keys.size(); ++index) {
        const std::int64_t key = keys[index];
        if (key == 0) {
            if (nullPayload.has_value()) {
                continue;
            }
            nullPayload = index;
        } else {
            const auto mask = n - 1;
            std::int32_t pos = fastutilMix(key) & mask;
            bool replaced = false;
            while (table[static_cast<std::size_t>(pos)] != 0) {
                if (table[static_cast<std::size_t>(pos)] == key) {
                    payload[static_cast<std::size_t>(pos)] = index;
                    replaced = true;
                    break;
                }
                pos = (pos + 1) & mask;
            }
            if (replaced) {
                continue;
            }
            table[static_cast<std::size_t>(pos)] = key;
            payload[static_cast<std::size_t>(pos)] = index;
        }
        if (size++ >= fill) {
            const std::int32_t grown = arraySize(size + 1);
            std::vector<std::int64_t> newTable(static_cast<std::size_t>(grown), 0);
            std::vector<std::size_t> newPayload(static_cast<std::size_t>(grown), 0);
            for (std::int32_t slot = n; slot-- != 0;) {
                if (table[static_cast<std::size_t>(slot)] != 0) {
                    place(newTable, newPayload, table[static_cast<std::size_t>(slot)],
                          payload[static_cast<std::size_t>(slot)]);
                }
            }
            table = std::move(newTable);
            payload = std::move(newPayload);
            n = grown;
            fill = maxFill(n);
        }
    }
    std::vector<std::size_t> order;
    if (nullPayload.has_value()) {
        order.push_back(*nullPayload);
    }
    for (std::int32_t slot = n; slot-- != 0;) {
        if (table[static_cast<std::size_t>(slot)] != 0) {
            order.push_back(payload[static_cast<std::size_t>(slot)]);
        }
    }
    return order;
}

[[nodiscard]] std::vector<Blender::Source> inWalkOrder(std::vector<Blender::Source> sources) {
    std::vector<std::int64_t> keys;
    keys.reserve(sources.size());
    for (const Blender::Source& source : sources) {
        keys.push_back(pack(source.chunkX, source.chunkZ));
    }
    std::vector<Blender::Source> ordered;
    ordered.reserve(sources.size());
    for (const std::size_t index : fastutilOrder(keys)) {
        ordered.push_back(std::move(sources[index]));
    }
    return ordered;
}

} // namespace

Blender::Blender(std::vector<Source> heightAndBiome, std::vector<Source> density, CarvingFilter carvingFilter)
    : heightAndBiome_(inWalkOrder(std::move(heightAndBiome))), density_(inWalkOrder(std::move(density))),
      carvingFilter_(std::move(carvingFilter)) {}

Blender::Output Blender::blendOffsetAndFactor(std::int32_t blockX, std::int32_t blockZ) const {
    const std::int32_t cellX = blockX >> 2;
    const std::int32_t cellZ = blockZ >> 2;
    // parity: `getBlendingDataValue` looks at the chunk the cell is in, then at the ones behind it on a shared edge.
    const auto fixedValue = [this, cellX, cellZ]() {
        const auto valueIn = [&](std::int32_t chunkX, std::int32_t chunkZ) {
            for (const Source& source : heightAndBiome_) {
                if (source.chunkX == chunkX && source.chunkZ == chunkZ) {
                    return source.data->heightAt(cellX - chunkX * 4, cellZ - chunkZ * 4);
                }
            }
            return BlendingData::kNoValue;
        };
        const std::int32_t chunkX = cellX >> 2;
        const std::int32_t chunkZ = cellZ >> 2;
        const bool minX = (cellX & 3) == 0;
        const bool minZ = (cellZ & 3) == 0;
        float value = valueIn(chunkX, chunkZ);
        if (value == BlendingData::kNoValue) {
            if (minX && minZ) {
                value = valueIn(chunkX - 1, chunkZ - 1);
            }
            if (value == BlendingData::kNoValue) {
                if (minX) {
                    value = valueIn(chunkX - 1, chunkZ);
                }
                if (value == BlendingData::kNoValue && minZ) {
                    value = valueIn(chunkX, chunkZ - 1);
                }
            }
        }
        return value;
    }();
    if (fixedValue != BlendingData::kNoValue) {
        return {0.0F, static_cast<float>(heightToOffset(static_cast<double>(fixedValue)))};
    }
    double totalWeight = 0.0;
    double weightedHeights = 0.0;
    double closestDistance = std::numeric_limits<double>::infinity();
    for (const Source& source : heightAndBiome_) {
        source.data->iterateHeights(
            source.chunkX * 4, source.chunkZ * 4, [&](std::int32_t testCellX, std::int32_t testCellZ, float height) {
                const double distance = static_cast<double>(
                    length(static_cast<float>(cellX - testCellX), static_cast<float>(cellZ - testCellZ)));
                if (distance > kHeightBlendingRangeCells) {
                    return;
                }
                closestDistance = std::min(closestDistance, distance);
                const double weight = 1.0 / (distance * distance * distance * distance);
                weightedHeights += static_cast<double>(height) * weight;
                totalWeight += weight;
            });
    }
    if (closestDistance == std::numeric_limits<double>::infinity()) {
        return {1.0F, 0.0F};
    }
    const double averageHeight = weightedHeights / totalWeight;
    double alpha = clamp(closestDistance / static_cast<double>(kHeightBlendingRangeCells + 1), 0.0, 1.0);
    alpha = 3.0 * alpha * alpha - 2.0 * alpha * alpha * alpha;
    return {static_cast<float>(alpha), static_cast<float>(heightToOffset(averageHeight))};
}

float Blender::blendDensity(std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ, float noiseValue) const {
    const std::int32_t cellX = blockX >> 2;
    const std::int32_t cellZ = blockZ >> 2;
    // parity: `blockY / 8` truncates in Java, which only differs from the floor below zero.
    const std::int32_t truncatedCellY = blockY / 8;
    const auto valueIn = [&](std::int32_t chunkX, std::int32_t chunkZ) {
        for (const Source& source : heightAndBiome_) {
            if (source.chunkX == chunkX && source.chunkZ == chunkZ) {
                return source.data->densityAt(cellX - chunkX * 4, truncatedCellY, cellZ - chunkZ * 4);
            }
        }
        return BlendingData::kNoValue;
    };
    const std::int32_t chunkX = cellX >> 2;
    const std::int32_t chunkZ = cellZ >> 2;
    const bool minX = (cellX & 3) == 0;
    const bool minZ = (cellZ & 3) == 0;
    float fixed = valueIn(chunkX, chunkZ);
    if (fixed == BlendingData::kNoValue) {
        if (minX && minZ) {
            fixed = valueIn(chunkX - 1, chunkZ - 1);
        }
        if (fixed == BlendingData::kNoValue) {
            if (minX) {
                fixed = valueIn(chunkX - 1, chunkZ);
            }
            if (fixed == BlendingData::kNoValue && minZ) {
                fixed = valueIn(chunkX, chunkZ - 1);
            }
        }
    }
    if (fixed != BlendingData::kNoValue) {
        return fixed;
    }
    const std::int32_t densityCellY = truncatedCellY;
    double totalWeight = 0.0;
    double weightedDensities = 0.0;
    double closestDistance = std::numeric_limits<double>::infinity();
    for (const Source& source : density_) {
        source.data->iterateDensities(
            source.chunkX * 4, source.chunkZ * 4, densityCellY - 1, densityCellY + 1,
            [&](std::int32_t testCellX, std::int32_t testCellY, std::int32_t testCellZ, float density) {
                const double distance = static_cast<double>(length(static_cast<float>(cellX - testCellX),
                                                                   static_cast<float>((densityCellY - testCellY) * 2),
                                                                   static_cast<float>(cellZ - testCellZ)));
                if (distance > 2.0) {
                    return;
                }
                closestDistance = std::min(closestDistance, distance);
                const double weight = 1.0 / (distance * distance * distance * distance);
                weightedDensities += static_cast<double>(density) * weight;
                totalWeight += weight;
            });
    }
    if (closestDistance == std::numeric_limits<double>::infinity()) {
        return noiseValue;
    }
    const double average = weightedDensities / totalWeight;
    const double alpha = clamp(closestDistance / 3.0, 0.0, 1.0);
    return static_cast<float>(average + alpha * (static_cast<double>(noiseValue) - average));
}

std::optional<std::int32_t> Blender::blendBiome(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const {
    float closestDistance = std::numeric_limits<float>::infinity();
    std::optional<std::int32_t> closestBiome;
    for (const Source& source : heightAndBiome_) {
        source.data->iterateBiomes(source.chunkX * 4, quartY, source.chunkZ * 4,
                                   [&](std::int32_t testCellX, std::int32_t testCellZ, std::int32_t biome) {
                                       const float distance = length(static_cast<float>(quartX - testCellX),
                                                                     static_cast<float>(quartZ - testCellZ));
                                       if (distance > static_cast<float>(kHeightBlendingRangeCells)) {
                                           return;
                                       }
                                       if (distance < closestDistance) {
                                           closestBiome = biome;
                                           closestDistance = distance;
                                       }
                                   });
    }
    if (closestDistance == std::numeric_limits<float>::infinity()) {
        return std::nullopt;
    }
    const float shift = shiftNoise().get(static_cast<double>(quartX), 0.0, static_cast<double>(quartZ)) * 12.0F;
    const float alpha =
        std::clamp((closestDistance + shift) / static_cast<float>(kHeightBlendingRangeCells + 1), 0.0F, 1.0F);
    return alpha > 0.5F ? std::nullopt : closestBiome;
}

Blender::CarvingFilter Blender::makeCarvingFilter(std::vector<OldSide> sides) {
    struct Cube {
        double offsetX;
        double offsetZ;
        double radiusY;
        double centerY;
    };

    std::vector<Cube> cubes;
    for (const OldSide& old : sides) {
        double offsetX = 0.0;
        double offsetZ = 0.0;
        if (old.side.has_value()) {
            offsetX = static_cast<double>(sideStepX(*old.side) * 16);
            offsetZ = static_cast<double>(sideStepZ(*old.side) * 16);
        }
        const double radiusY = static_cast<double>(old.data->areaHeight()) / 2.0;
        cubes.push_back({offsetX, offsetZ, radiusY, static_cast<double>(old.data->minY()) + radiusY});
    }
    return [cubes = std::move(cubes)](std::int32_t x, std::int32_t y, std::int32_t z) {
        const noise::NoiseStack& shift = shiftNoise();
        const double shiftedX =
            static_cast<double>(x) + 0.5 +
            static_cast<double>(shift.get(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)) *
                                4.0F);
        const double shiftedY =
            static_cast<double>(y) + 0.5 +
            static_cast<double>(shift.get(static_cast<double>(y), static_cast<double>(z), static_cast<double>(x)) *
                                4.0F);
        const double shiftedZ =
            static_cast<double>(z) + 0.5 +
            static_cast<double>(shift.get(static_cast<double>(z), static_cast<double>(x), static_cast<double>(y)) *
                                4.0F);
        double closest = std::numeric_limits<double>::infinity();
        for (const Cube& cube : cubes) {
            const double deltaX = std::abs(shiftedX - 8.0 - cube.offsetX) - 8.0;
            const double deltaY = std::abs(shiftedY - cube.centerY) - cube.radiusY;
            const double deltaZ = std::abs(shiftedZ - 8.0 - cube.offsetZ) - 8.0;
            const double distance = lengthD(std::max(0.0, deltaX), std::max(0.0, deltaY), std::max(0.0, deltaZ));
            closest = std::min(closest, distance);
        }
        return closest < 4.0;
    };
}

} // namespace basalt::server::world::blending
