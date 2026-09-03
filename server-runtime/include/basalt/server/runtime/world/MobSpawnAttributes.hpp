#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <map>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {
enum class AttributeOperation : std::uint8_t;
}

namespace basalt::server::runtime::world {

enum class PandaGene : std::uint8_t;

/// Random attribute bases selected by concrete mob finalizers before the common mob layer.
namespace mobSpawnAttributes {

void write(std::string_view type, random::RandomSource& random, const wire::nbt::NbtCompound& data,
           std::map<std::string, double>& attributes);

/// parity: `Panda.getRandomGene`, whose roll decides the gene and whose gaps are all `weak`.
[[nodiscard]] PandaGene pandaGene(std::int32_t roll);

} // namespace mobSpawnAttributes

/// Small NBT updater for concrete finalizers that add attributes after `Mob.finalizeSpawn`.
namespace spawnAttributeNbt {

void setBase(wire::nbt::NbtCompound& data, std::string_view attributeId, double base);

void addModifier(wire::nbt::NbtCompound& data, std::string_view attributeId, double base, std::string_view modifierId,
                 double amount, entity::AttributeOperation operation);

} // namespace spawnAttributeNbt

} // namespace basalt::server::runtime::world
