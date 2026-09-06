#pragma once

#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/server/runtime/world/WorldMetadata.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/NeighbourTerrain.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {
class StrongholdStructureGenerator;
class VillageStructureGenerator;
} // namespace basalt::server::world::terrain

namespace basalt::server::runtime::world {

/// Builds one correctly seeded normal-terrain generator for each loaded world.
///
/// The pieces of a world's generator (the router, the surface environment, every structure
/// generator and their caches) are held here for the life of the factory: they are shared by every
/// generation worker, and each of them has to outlive the chunk function that closes over it.
class VanillaWorldGeneratorFactory {
public:
    /// The jar as this reads it. Callbacks rather than `ServerJarAssets`, the same as every other
    /// registry here: this needs the operator's own copy of the game, and a test has none.
    struct Jar {
        std::function<std::optional<std::string>(std::string_view name)> readText;
        StructureTemplates::ReadEntry readEntry;
        StructureTemplates::DataIds dataIds;
    };

    /// A world's terrain, and the structure generator behind it when there is one worth naming.
    /// `generate` is unset when the jar cannot supply what the dimension needs: a generator without the
    /// operator's own noise files would crash on its first chunk rather than make a wrong one.
    ///
    /// The generator is handed back because `/place structure` and `/locate structure` need the very
    /// object generation uses: a second one built from the same seed would still be a second one.
    struct Generators {
        /// `beforeDecoration` runs on the finished terrain, before the chunk decorates itself: it is
        /// where a write a neighbour queued for this chunk gets delivered, which is the point vanilla
        /// has it because there the neighbour is already a proto chunk.
        std::function<server::world::Chunk(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t gameTime,
                                           const std::string& difficulty,
                                           const std::function<void(server::world::Chunk&)>& beforeDecoration)>
            generate;
        server::world::terrain::VillageStructureGenerator* villages = nullptr;
        server::world::terrain::StrongholdStructureGenerator* strongholds = nullptr;
        /// parity: `randomState().sampler().findSpawnPosition()`, in chunk coordinates.
        std::function<std::pair<std::int32_t, std::int32_t>()> spawnChunk;
        /// Installs the lookup that lets a neighbour read see what another chunk already queued for
        /// it, the way vanilla's proto chunk already holds that write.
        std::function<void(server::world::terrain::NeighbourTerrainCache::QueuedAt)> useQueuedWrites;
    };

    /// What the structure caches hold across every world built by this factory.
    struct CacheStats {
        std::size_t templates = 0;
        std::int64_t templateBlocks = 0;
        std::size_t assemblies = 0;
        std::int64_t assemblyPieces = 0;
    };

    explicit VanillaWorldGeneratorFactory(Jar jar, wire::common::Logger logger = wire::common::Logger{"Worldgen"});

    VanillaWorldGeneratorFactory(const VanillaWorldGeneratorFactory&) = delete;
    VanillaWorldGeneratorFactory& operator=(const VanillaWorldGeneratorFactory&) = delete;
    ~VanillaWorldGeneratorFactory();

    [[nodiscard]] CacheStats cacheStats() const;

    [[nodiscard]] Generators create(const config::WorldDeclaration& declaration, const WorldMetadata& metadata);

private:
    struct Built;

    [[nodiscard]] Generators overworld(std::int64_t seed, bool spawnMobs);
    [[nodiscard]] Generators nether(std::int64_t seed, bool spawnMobs);
    [[nodiscard]] Generators end(std::int64_t seed);

    Jar jar_;
    wire::common::Logger logger_;
    /// One entry per world built, kept alive for the process: a chunk function closes over its own.
    std::vector<std::unique_ptr<Built>> built_;
};

} // namespace basalt::server::runtime::world
