#include <basalt/server/runtime/registry/VanillaBiomes.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/Precipitation.hpp>
#include <basalt/server/world/terrain/BiomeClimateData.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::world::precipitation {

bool rainingAt(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (!world.weather().raining || !naturalSpawner::canSeeSky(world, x, y, z)) {
        return false;
    }
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr || chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::MotionBlocking) > y) {
        return false;
    }
    const std::optional<wire::common::Identifier> biome = naturalSpawner::biomeAt(world, x, y, z);
    if (!biome.has_value()) {
        return false;
    }
    const std::string id = biome->toString();
    return server::world::terrain::biomeClimateFor(id).hasPrecipitation &&
           !server::world::terrain::biomeTemperature::coldEnoughToSnow(id, x, y, z);
}

bool entityInRain(LoadedWorld& world, double x, double y, double z, double height) {
    const std::int32_t bx = wire::common::mth::floor(x);
    const std::int32_t by = wire::common::mth::floor(y);
    const std::int32_t bz = wire::common::mth::floor(z);
    // parity: the second probe keeps the block's own x and z, not the entity's.
    return rainingAt(world, bx, by, bz) || rainingAt(world, bx, wire::common::mth::floor(y + height), bz);
}

bool increasedFireBurnout(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    static const std::set<std::string, std::less<>> kBiomes = [] {
        std::set<std::string, std::less<>> found;
        for (const registry::vanillaBiomes::Entry& entry : registry::vanillaBiomes::all()) {
            for (const auto& [name, attribute] : entry.attributes) {
                if (name == "minecraft:gameplay/increased_fire_burnout" && attribute.flag) {
                    found.insert(entry.path);
                }
            }
        }
        return found;
    }();
    const std::optional<wire::common::Identifier> biome = naturalSpawner::biomeAt(world, x, y, z);
    return biome.has_value() && kBiomes.contains(biome->path());
}

} // namespace basalt::server::runtime::world::precipitation
