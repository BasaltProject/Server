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
#include <functional>
#include <memory>

namespace basalt::server::world::terrain {

struct TerrainStructureGenerators {
    VillageStructureGenerator* village = nullptr;
    VillageStructureGenerator* camp = nullptr;
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

/// parity: `ChunkGenerator.applyBiomeDecoration`.
namespace realTerrainGenerator {

class GeneratedTerrain {
public:
    GeneratedTerrain(Chunk chunk, std::unique_ptr<densityfunction::SamplerContext> cache,
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
    std::unique_ptr<densityfunction::SamplerContext> cache_;
    std::unique_ptr<surface::SurfaceRuleEnvironment> environment_;
};

/// parity: `NoiseBasedChunkGenerator`'s biomes.
[[nodiscard]] GeneratedTerrain buildTerrain(const NoiseRouter& router,
                                            const surface::SurfaceRuleEnvironment& environment, Chunk chunk,
                                            densityfunction::Sampler beardifier);

[[nodiscard]] GeneratedTerrain generateTerrain(const NoiseRouter& router,
                                               const surface::SurfaceRuleEnvironment& environment, std::int32_t chunkX,
                                               std::int32_t chunkZ, std::int64_t seed = 0,
                                               const TerrainStructureGenerators& generators = {});

void decorate(GeneratedTerrain& terrain, std::int64_t seed = 0, const NeighbourTerrain* neighbours = nullptr,
              const TerrainStructureGenerators& generators = {}, const structure::TemplateLoader& templates = nullptr);

[[nodiscard]] GeneratedTerrain adopt(const surface::SurfaceRuleEnvironment& environment, Chunk chunk);

[[nodiscard]] Chunk generate(const NoiseRouter& router, const surface::SurfaceRuleEnvironment& environment,
                             std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed = 0,
                             const TerrainStructureGenerators& generators = {},
                             const structure::TemplateLoader& templates = nullptr);

} // namespace realTerrainGenerator

} // namespace basalt::server::world::terrain
