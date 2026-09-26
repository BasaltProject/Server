#pragma once

#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/world/StructureLocator.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/server/runtime/world/WorldMetadata.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/DensityFunctionJson.hpp>
#include <basalt/server/world/terrain/ProtoChunks.hpp>
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
/// Owns each world's generator pieces (router, surface environment, structure generators, caches):
/// every generation worker shares them and they must outlive the chunk functions that capture them.
class VanillaWorldGeneratorFactory {
public:
    /// The jar as this reads it. Callbacks rather than `ServerJarAssets`, the same as every other
    /// registry here: this needs the operator's own copy of the game, and a test has none.
    struct Jar {
        std::function<std::optional<std::string>(std::string_view name)> readText;
        StructureTemplates::ReadEntry readEntry;
        StructureTemplates::DataIds dataIds;
    };

    /// A world's terrain and its structure generator, if any. The pipeline is unset when the jar
    /// cannot supply what the dimension needs. The structure generator is the very one generation
    /// uses, which `/place structure` and `/locate structure` need.
    struct Generators {
        /// The steps from terrain to decoration. The loading, finishing and saving hooks are the world's.
        server::world::terrain::ProtoChunks::Hooks pipeline;
        /// parity: `ChunkGenerator.spawnOriginalMobs`, run once the chunk is full.
        std::function<void(server::world::Chunk&, std::int64_t gameTime, const std::string& difficulty)> spawnMobs;
        server::world::terrain::VillageStructureGenerator* villages = nullptr;
        server::world::terrain::VillageStructureGenerator* camps = nullptr;
        /// The single-generator structures `/locate` can find.
        std::vector<StructureLocator> locators;
        server::world::terrain::StrongholdStructureGenerator* strongholds = nullptr;
        /// parity: `randomState().sampler().findSpawnPosition()`, in chunk coordinates.
        std::function<std::pair<std::int32_t, std::int32_t>()> spawnChunk;
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

    /// `settings` is the `noise_settings` the preset names: overworld, amplified or large_biomes.
    [[nodiscard]] Generators overworld(std::int64_t seed, bool spawnMobs, std::string_view settings);
    /// The jar's `data/` as the worldgen loaders read it.
    [[nodiscard]] server::world::densityfunction::WorldgenReader worldgenReader() const;
    /// parity: the `flat` preset: its layers in plains, with the villages it overrides back in.
    [[nodiscard]] Generators flat(std::int64_t seed, bool spawnMobs);
    [[nodiscard]] Generators nether(std::int64_t seed, bool spawnMobs);
    [[nodiscard]] Generators end(std::int64_t seed);

    Jar jar_;
    wire::common::Logger logger_;
    /// One entry per world built, kept alive for the process: a chunk function closes over its own.
    std::vector<std::unique_ptr<Built>> built_;
};

} // namespace basalt::server::runtime::world
