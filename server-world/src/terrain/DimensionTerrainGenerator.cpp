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

using ChunkPass = std::function<void(Chunk&, densityfunction::SamplerContext&)>;
using QuartBiome = std::function<std::string_view(std::int32_t, std::int32_t, std::int32_t)>;

/// A biome source as the terrain pipeline reads it, the names interned so references stay valid.
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

[[nodiscard]] Chunk generate(const NoiseRouter& router, const surface::SurfaceRuleEnvironment& environment,
                             ChunkHeight bounds, std::int32_t chunkX, std::int32_t chunkZ,
                             densityfunction::Sampler beardifier, const ChunkPass& structurePass,
                             const ChunkPass& decorationPass,
                             const realTerrainGenerator::BeforeDecoration& beforeDecoration) {
    realTerrainGenerator::GeneratedTerrain terrain =
        realTerrainGenerator::buildTerrain(router, environment, Chunk{chunkX, chunkZ, bounds}, std::move(beardifier));
    Chunk& chunk = terrain.chunk();
    densityfunction::SamplerContext context{densityfunction::SamplerContext::Options{.enableCaches = true}};
    generationMetrics::measure(generationMetrics::Phase::Structures, [&] { structurePass(chunk, context); });
    if (beforeDecoration) {
        beforeDecoration(chunk);
    }
    generationMetrics::measure(generationMetrics::Phase::Decoration, [&] { decorationPass(chunk, context); });
    if (generationMetrics::enabled()) {
        generationMetrics::countChunk();
    }
    return std::move(chunk);
}

/// The Nether's passes, shared by the full generator and by the undecorated one the neighbour cache
/// hands out.
[[nodiscard]] Chunk generateNether(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                                   std::int32_t chunkZ, std::int64_t seed, NetherStructureGenerator* structureGenerator,
                                   const ChunkPass& decorationPass,
                                   const realTerrainGenerator::BeforeDecoration& beforeDecoration = {}) {
    const BiomeSourceEnvironment environment{
        [&biomeSource](std::int32_t x, std::int32_t y, std::int32_t z) { return biomeSource.biomeAtQuart(x, y, z); },
        [&biomeSource](std::int32_t x, std::int32_t y, std::int32_t z) { return biomeSource.blockBiomeAt(x, y, z); }};
    densityfunction::Sampler beardifier =
        structureGenerator == nullptr ? nullptr : structureGenerator->beardifierForChunk(chunkX, chunkZ, seed);
    return generate(
        router, environment, kNetherHeight, chunkX, chunkZ, std::move(beardifier),
        [structureGenerator, seed](Chunk& chunk, densityfunction::SamplerContext&) {
            if (structureGenerator != nullptr) {
                structureGenerator->generateInto(chunk, seed);
            }
        },
        decorationPass, beforeDecoration);
}

} // namespace

Chunk netherTerrain(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                    std::int32_t chunkZ, std::int64_t seed, NetherStructureGenerator* structureGenerator) {
    return generateNether(router, biomeSource, chunkX, chunkZ, seed, structureGenerator,
                          [](Chunk&, densityfunction::SamplerContext&) {});
}

Chunk nether(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX, std::int32_t chunkZ,
             std::int64_t seed, NetherStructureGenerator* structureGenerator, NeighbourTerrainCache* neighbours,
             const realTerrainGenerator::BeforeDecoration& beforeDecoration) {
    const std::unique_ptr<NeighbourTerrain> around =
        neighbours == nullptr ? nullptr : neighbours->around(chunkX, chunkZ);
    return generateNether(
        router, biomeSource, chunkX, chunkZ, seed, structureGenerator,
        [&biomeSource, seed, &around](Chunk& chunk, densityfunction::SamplerContext& cache) {
            // One reader for the whole phase, like the Overworld's. Without it a body reaching past
            // the edge sees nothing, and the delivery guard of every cross-chunk write is filled in
            // with air and refused.
            const Chunk::ExternalStateScope scope{
                chunk, [&around](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
                    return around == nullptr ? std::nullopt : around->blockStateIdAt(x, y, z);
                }};
            const NetherBiomeLookup biomeLookup = biomeSource.cached(cache);
            decoration::nether::netherLocalDecoration::decorate(chunk, seed, biomeLookup);
            decoration::nether::netherSurfaceStructuresDecoration::decorate(chunk, seed, biomeLookup);
            decoration::nether::netherUndergroundDecoration::decorate(chunk, seed, biomeLookup);
            decoration::nether::netherVegetalDecoration::decorate(chunk, seed, biomeLookup);
        },
        beforeDecoration);
}

Chunk end(const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX, std::int32_t chunkZ,
          std::int64_t seed, EndCityStructureGenerator* structureGenerator,
          const realTerrainGenerator::BeforeDecoration& beforeDecoration) {
    const BiomeSourceEnvironment environment{
        [&biomeSource](std::int32_t x, std::int32_t y, std::int32_t z) { return biomeSource.biomeAtQuart(x, y, z); },
        [&biomeSource](std::int32_t x, std::int32_t y, std::int32_t z) { return biomeSource.blockBiomeAt(x, y, z); }};
    return generate(
        router, environment, kEndHeight, chunkX, chunkZ, nullptr,
        [structureGenerator, seed](Chunk& chunk, densityfunction::SamplerContext&) {
            if (structureGenerator != nullptr) {
                structureGenerator->generateInto(chunk, seed);
            }
        },
        [&router, &biomeSource, seed](Chunk& chunk, densityfunction::SamplerContext&) {
            decoration::end::endRawDecoration::decorate(chunk, seed, biomeSource);
            decoration::end::endSurfaceStructuresDecoration::decorate(chunk, seed, router, biomeSource);
            decoration::end::endVegetalDecoration::decorate(chunk, seed, router, biomeSource);
            decoration::end::endTopLayerDecoration::decorate(chunk, biomeSource);
        },
        beforeDecoration);
}

} // namespace basalt::server::world::terrain::dimensionTerrainGenerator
