#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/GenerationMetrics.hpp>
#include <basalt/server/world/terrain/DimensionTerrainGenerator.hpp>
#include <basalt/server/world/terrain/decoration/end/EndDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherLocalDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherSurfaceStructuresDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherUndergroundDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherVegetalDecoration.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain::dimensionTerrainGenerator {
namespace {

using QuartBiome = std::function<std::string_view(std::int32_t, std::int32_t, std::int32_t)>;

class BiomeSourceEnvironment final : public surface::SurfaceRuleEnvironment {
public:
    BiomeSourceEnvironment(QuartBiome quart, QuartBiome block) : quart_(std::move(quart)), block_(std::move(block)) {}

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return intern(block_(x, y, z));
    }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                  densityfunction::SamplerContext*) const override {
        return intern(quart_(quartX, quartY, quartZ));
    }

private:
    [[nodiscard]] const std::string& intern(std::string_view name) const {
        if (const auto found = names_.find(name); found != names_.end()) {
            return *found;
        }
        return *names_.emplace(name).first;
    }

    QuartBiome quart_;
    QuartBiome block_;
    mutable std::set<std::string, std::less<>> names_;
};

[[nodiscard]] Chunk terrainOf(const NoiseRouter& router, const surface::SurfaceRuleEnvironment& environment,
                              ChunkHeight bounds, std::int32_t chunkX, std::int32_t chunkZ,
                              densityfunction::Sampler beardifier, const std::function<void(Chunk&)>& structurePass) {
    realTerrainGenerator::GeneratedTerrain terrain =
        realTerrainGenerator::buildTerrain(router, environment, Chunk{chunkX, chunkZ, bounds}, std::move(beardifier));
    Chunk& chunk = terrain.chunk();
    generationMetrics::measure(generationMetrics::Phase::Structures, [&] { structurePass(chunk); });
    return std::move(chunk);
}

[[nodiscard]] BiomeSourceEnvironment environmentOf(const auto& biomeSource) {
    return BiomeSourceEnvironment{
        [&biomeSource](std::int32_t x, std::int32_t y, std::int32_t z) { return biomeSource.biomeAtQuart(x, y, z); },
        [&biomeSource](std::int32_t x, std::int32_t y, std::int32_t z) { return biomeSource.blockBiomeAt(x, y, z); }};
}

} // namespace

Chunk netherTerrain(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                    std::int32_t chunkZ, std::int64_t seed, NetherStructureGenerator* structureGenerator) {
    const BiomeSourceEnvironment environment = environmentOf(biomeSource);
    densityfunction::Sampler beardifier =
        structureGenerator == nullptr ? nullptr : structureGenerator->beardifierForChunk(chunkX, chunkZ, seed);
    return terrainOf(router, environment, kNetherHeight, chunkX, chunkZ, std::move(beardifier),
                     [structureGenerator, seed](Chunk& chunk) {
                         if (structureGenerator != nullptr) {
                             structureGenerator->generateInto(chunk, seed);
                         }
                     });
}

void decorateNether(Chunk& chunk, const NetherBiomeSource& biomeSource, std::int64_t seed,
                    const NeighbourTerrain* neighbours) {
    generationMetrics::measure(generationMetrics::Phase::Decoration, [&] {
        const Chunk::ExternalStateScope scope{
            chunk, [neighbours](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
                return neighbours == nullptr ? std::nullopt : neighbours->blockStateIdAt(x, y, z);
            }};
        densityfunction::SamplerContext cache{densityfunction::SamplerContext::Options{.enableCaches = true}};
        const NetherBiomeLookup biomeLookup = biomeSource.cached(cache);
        decoration::nether::netherLocalDecoration::decorate(chunk, seed, biomeLookup);
        decoration::nether::netherSurfaceStructuresDecoration::decorate(chunk, seed, biomeLookup);
        decoration::nether::netherUndergroundDecoration::decorate(chunk, seed, biomeLookup);
        decoration::nether::netherVegetalDecoration::decorate(chunk, seed, biomeLookup);
    });
    if (generationMetrics::enabled()) {
        generationMetrics::countChunk();
    }
}

Chunk nether(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX, std::int32_t chunkZ,
             std::int64_t seed, NetherStructureGenerator* structureGenerator) {
    Chunk chunk = netherTerrain(router, biomeSource, chunkX, chunkZ, seed, structureGenerator);
    decorateNether(chunk, biomeSource, seed);
    return chunk;
}

Chunk endTerrain(const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX,
                 std::int32_t chunkZ, std::int64_t seed, EndCityStructureGenerator* structureGenerator) {
    const BiomeSourceEnvironment environment = environmentOf(biomeSource);
    return terrainOf(router, environment, kEndHeight, chunkX, chunkZ, nullptr,
                     [structureGenerator, seed](Chunk& chunk) {
                         if (structureGenerator != nullptr) {
                             structureGenerator->generateInto(chunk, seed);
                         }
                     });
}

void decorateEnd(Chunk& chunk, const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int64_t seed) {
    generationMetrics::measure(generationMetrics::Phase::Decoration, [&] {
        decoration::end::endRawDecoration::decorate(chunk, seed, biomeSource);
        decoration::end::endSurfaceStructuresDecoration::decorate(chunk, seed, router, biomeSource);
        decoration::end::endVegetalDecoration::decorate(chunk, seed, router, biomeSource);
        decoration::end::endTopLayerDecoration::decorate(chunk, biomeSource);
    });
    if (generationMetrics::enabled()) {
        generationMetrics::countChunk();
    }
}

Chunk end(const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX, std::int32_t chunkZ,
          std::int64_t seed, EndCityStructureGenerator* structureGenerator) {
    Chunk chunk = endTerrain(router, biomeSource, chunkX, chunkZ, seed, structureGenerator);
    decorateEnd(chunk, router, biomeSource, seed);
    return chunk;
}

} // namespace basalt::server::world::terrain::dimensionTerrainGenerator
