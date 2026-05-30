#pragma once

#include <basalt/server/world/densityfunction/DensityFunctionJson.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::world::terrain {
class NoiseChunk;
struct NoiseRouter;
} // namespace basalt::server::world::terrain

namespace basalt::server::world::terrain::carver {

/// parity: `CarvingMask`, the carved blocks of one chunk, column by column.
struct CarvingMask {
    std::int32_t minY = 0;
    std::int32_t maxY = 0;
    std::vector<bool> bits;

    CarvingMask(std::int32_t min, std::int32_t max)
        : minY(min), maxY(max), bits(static_cast<std::size_t>(256 * (max - min + 1)), false) {}

    [[nodiscard]] std::int32_t height() const noexcept { return maxY - minY + 1; }

    void set(std::int32_t x, std::int32_t y, std::int32_t z) {
        bits[static_cast<std::size_t>(y - minY + (z + (x << 4)) * height())] = true;
    }
};

/// parity: `WorldCarver`, a `cave` or `canyon` carver read from `worldgen/carver`.
class WorldCarver {
public:
    virtual ~WorldCarver() = default;

    [[nodiscard]] virtual bool isStartChunk(random::RandomSource& random) const = 0;

    virtual void carve(random::RandomSource& random, std::int32_t chunkX, std::int32_t chunkZ, std::int32_t sourceX,
                       std::int32_t sourceZ, std::int32_t minGenY, std::int32_t genDepth, CarvingMask& mask) const = 0;
};

class CarverRegistry {
public:
    explicit CarverRegistry(densityfunction::WorldgenReader reader);

    [[nodiscard]] const std::vector<std::shared_ptr<const WorldCarver>>& forBiome(const std::string& biome);

private:
    [[nodiscard]] std::shared_ptr<const WorldCarver> carver(const std::string& id);

    densityfunction::WorldgenReader reader_;
    std::mutex mutex_;
    std::map<std::string, std::vector<std::shared_ptr<const WorldCarver>>, std::less<>> biomes_;
    std::map<std::string, std::shared_ptr<const WorldCarver>, std::less<>> carvers_;
};

using CarverBiome = std::function<const std::string&(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ)>;

using TopMaterial =
    std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z, bool underFluid)>;

/// parity: `NoiseBasedChunkGenerator.generateCarvers` and `applyCarvingMask`.
void carve(Chunk& chunk, std::int64_t seed, NoiseChunk& noiseChunk, const NoiseRouter& router, CarverRegistry& registry,
           const CarverBiome& biomeAt, const TopMaterial& topMaterial,
           const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>* filter = nullptr);

void carveShape(Chunk& chunk, const NoiseRouter& router, const CarverBiome& biomeAt);

} // namespace basalt::server::world::terrain::carver
