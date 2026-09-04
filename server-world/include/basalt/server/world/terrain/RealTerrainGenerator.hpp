#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/BuriedTreasureStructureGenerator.hpp>
#include <basalt/server/world/terrain/DesertPyramidStructureGenerator.hpp>
#include <basalt/server/world/terrain/IglooStructureGenerator.hpp>
#include <basalt/server/world/terrain/JigsawStructureGenerator.hpp>
#include <basalt/server/world/terrain/JungleTempleStructureGenerator.hpp>
#include <basalt/server/world/terrain/MineshaftStructureGenerator.hpp>
#include <basalt/server/world/terrain/NeighbourTerrain.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/OceanMonumentStructureGenerator.hpp>
#include <basalt/server/world/terrain/OceanRuinStructureGenerator.hpp>
#include <basalt/server/world/terrain/RuinedPortalStructureGenerator.hpp>
#include <basalt/server/world/terrain/ShipwreckStructureGenerator.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>
#include <basalt/server/world/terrain/SwampHutStructureGenerator.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>
#include <basalt/server/world/terrain/WoodlandMansionStructureGenerator.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <memory>

namespace basalt::server::world::terrain {

/// Every structure generator a chunk may run, none of them required. A world that has not built one
/// leaves its slot null and the step is skipped.
struct TerrainStructureGenerators {
    VillageStructureGenerator* village = nullptr;
    MineshaftStructureGenerator* mineshaft = nullptr;
    RuinedPortalStructureGenerator* ruinedPortal = nullptr;
    SwampHutStructureGenerator* swampHut = nullptr;
    DesertPyramidStructureGenerator* desertPyramid = nullptr;
    JungleTempleStructureGenerator* jungleTemple = nullptr;
    WoodlandMansionStructureGenerator* mansion = nullptr;
    OceanMonumentStructureGenerator* oceanMonument = nullptr;
    StrongholdStructureGenerator* stronghold = nullptr;
    JigsawStructureGenerator* pillagerOutpost = nullptr;
    JigsawStructureGenerator* trailRuins = nullptr;
    BuriedTreasureStructureGenerator* buriedTreasure = nullptr;
    ShipwreckStructureGenerator* shipwreck = nullptr;
    OceanRuinStructureGenerator* oceanRuin = nullptr;
    IglooStructureGenerator* igloo = nullptr;
    JigsawStructureGenerator* ancientCity = nullptr;
    JigsawStructureGenerator* trialChamber = nullptr;
};

/// Chunk shape from an `OverworldNoiseRouter`'s final density through `DensityChunkSampler`, then
/// surface rules, structures and independently seeded decoration dispatchers, in that order.
/// parity: `ChunkGenerator.applyBiomeDecoration`, whose decoration seed is the plain world seed.
///
/// Build the router once per world: it is dozens of nested nodes. Gap: decoration only for the
/// biomes ported so far.
namespace realTerrainGenerator {

/// A chunk carrying everything a configured feature may read before decoration runs.
class GeneratedTerrain {
public:
    GeneratedTerrain(Chunk chunk, std::unique_ptr<densityfunction::DensityCache> cache,
                     std::unique_ptr<surface::SurfaceRuleEnvironment> environment);
    ~GeneratedTerrain();
    GeneratedTerrain(GeneratedTerrain&&) noexcept;
    GeneratedTerrain& operator=(GeneratedTerrain&&) noexcept;
    GeneratedTerrain(const GeneratedTerrain&) = delete;
    GeneratedTerrain& operator=(const GeneratedTerrain&) = delete;

    [[nodiscard]] Chunk& chunk() noexcept { return chunk_; }

    [[nodiscard]] const surface::SurfaceRuleEnvironment& environment() const noexcept { return *environment_; }

private:
    Chunk chunk_;
    /// The cache outlives the environment that reads it, so both travel with the chunk.
    std::unique_ptr<densityfunction::DensityCache> cache_;
    std::unique_ptr<surface::SurfaceRuleEnvironment> environment_;
};

/// Everything a chunk holds before its own decoration: noise shape, surface rules, carvers and
/// structures. A neighbour is generated only this far when a feature needs to read across the chunk
/// edge, so reading one never recurses back into decoration.
[[nodiscard]] GeneratedTerrain generateTerrain(const OverworldNoiseRouter& router,
                                               const surface::SurfaceRuleEnvironment& environment, std::int32_t chunkX,
                                               std::int32_t chunkZ, std::int64_t seed = 0,
                                               const TerrainStructureGenerators& generators = {});

/// The decoration steps of one already-generated terrain, in their official order.
void decorate(GeneratedTerrain& terrain, std::int64_t seed = 0, const NeighbourTerrain* neighbours = nullptr,
              const TerrainStructureGenerators& generators = {}, const structure::TemplateLoader& templates = nullptr);

[[nodiscard]] Chunk generate(const OverworldNoiseRouter& router, const surface::SurfaceRuleEnvironment& environment,
                             std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed = 0,
                             const TerrainStructureGenerators& generators = {},
                             NeighbourTerrainCache* neighbours = nullptr,
                             const structure::TemplateLoader& templates = nullptr);

} // namespace realTerrainGenerator

} // namespace basalt::server::world::terrain
