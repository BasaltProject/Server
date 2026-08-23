#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/IceAndSnow.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/world/terrain/BiomeClimateData.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::iceAndSnow {
namespace {

namespace temperature = server::world::terrain::biomeTemperature;

/// parity: `Biome.shouldFreeze(level, pos, true)`.
[[nodiscard]] bool shouldFreeze(LoadedWorld& world, const std::string& biome, std::int32_t x, std::int32_t y,
                                std::int32_t z) {
    if (!temperature::coldEnoughToSnow(biome, x, y, z)) {
        return false;
    }
    const server::world::ChunkHeight height = config::dimensionFacts(world.declaration().dimension).chunkHeight;
    if (y < height.minY() || y > height.maxY() || worldLight::blockLight(world.state(), x, y, z) >= 10) {
        return false;
    }
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const wire::common::Identifier* block =
        state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    if (block == nullptr || block->path() != "water") {
        return false;
    }
    const auto waterAt = [&world](std::int32_t wx, std::int32_t wz, std::int32_t wy) {
        const std::optional<std::int32_t> at = world.state().blockState(wx, wy, wz);
        return at.has_value() && fluids::of(*at) == Submersion::Water;
    };
    return !(waterAt(x - 1, z, y) && waterAt(x + 1, z, y) && waterAt(x, z - 1, y) && waterAt(x, z + 1, y));
}

/// parity: `Biome.shouldSnow`.
[[nodiscard]] bool shouldSnow(LoadedWorld& world, const std::string& biome, std::int32_t x, std::int32_t y,
                              std::int32_t z, std::int32_t snowDefault) {
    if (!server::world::terrain::biomeClimateFor(biome).hasPrecipitation ||
        !temperature::coldEnoughToSnow(biome, x, y, z)) {
        return false;
    }
    const server::world::ChunkHeight height = config::dimensionFacts(world.declaration().dimension).chunkHeight;
    if (y < height.minY() || y > height.maxY() || worldLight::blockLight(world.state(), x, y, z) >= 10) {
        return false;
    }
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    if (!state.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*state);
    const bool airOrSnow = registry::blocks::isAir(*state) || (block != nullptr && block->path() == "snow");
    return airOrSnow && blockSurvival::isSupported(world.state(), x, y, z, snowDefault);
}

void precipitate(LoadedWorld& world, std::int32_t x, std::int32_t z, std::int32_t maxHeight,
                 std::vector<BlockChange>& changes) {
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return;
    }
    const std::int32_t top = chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::MotionBlocking);
    const std::optional<wire::common::Identifier> biomeId = naturalSpawner::biomeAt(world, x, top, z);
    if (!biomeId.has_value()) {
        return;
    }
    const std::string biome = biomeId->toString();
    if (shouldFreeze(world, biome, x, top - 1, z)) {
        const registry::BlockState* ice = registry::blocks::named("ice");
        if (ice != nullptr && world.state().setBlockState(x, top - 1, z, ice->stateId)) {
            changes.push_back(BlockChange{.x = x, .y = top - 1, .z = z, .stateId = ice->stateId});
        }
    }
    if (!world.weather().raining || maxHeight <= 0) {
        return;
    }
    const registry::BlockState* snow = registry::blocks::named("snow");
    if (snow == nullptr || !shouldSnow(world, biome, x, top, z, snow->stateId)) {
        return;
    }
    const std::optional<std::int32_t> state = world.state().blockState(x, top, z);
    std::int32_t next = snow->stateId;
    if (state.has_value() && !registry::blocks::isAir(*state)) {
        const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*state);
        const auto layers = properties.find("layers");
        const std::int32_t current = layers == properties.end() ? 1 : std::stoi(layers->second);
        if (current >= std::min(maxHeight, 8)) {
            return;
        }
        next =
            registry::blockStateDefinitions::stateIdOf(snow->block, {{"layers", std::to_string(current + 1)}}, *state)
                .value_or(*state);
    }
    if (world.state().setBlockState(x, top, z, next)) {
        changes.push_back(BlockChange{.x = x, .y = top, .z = z, .stateId = next});
    }
}

} // namespace

std::vector<BlockChange> tick(LoadedWorld& world, std::span<const std::array<std::int32_t, 2>> chunks,
                              const std::int32_t speed, random::RandomSource& random) {
    std::vector<BlockChange> changes;
    const std::int32_t maxHeight = world.gameRules().number(*gameRules::byId("max_snow_accumulation_height"));
    for (const std::array<std::int32_t, 2>& position : chunks) {
        for (std::int32_t step = 0; step < speed; step++) {
            if (random.nextInt(48) != 0) {
                continue;
            }
            precipitate(world, (position[0] << 4) + random.nextInt(16), (position[1] << 4) + random.nextInt(16),
                        maxHeight, changes);
        }
    }
    return changes;
}

} // namespace basalt::server::runtime::world::iceAndSnow
