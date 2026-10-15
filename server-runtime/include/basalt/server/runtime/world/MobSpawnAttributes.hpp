#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {
enum class AttributeOperation : std::uint8_t;
}

namespace basalt::server::runtime::world {

enum class PandaGene : std::uint8_t;

namespace mobSpawnAttributes {

void write(std::string_view type, random::RandomSource& random, const wire::nbt::NbtCompound& data,
           std::map<std::string, double>& attributes);

/// parity: `Panda.getRandomGene`.
[[nodiscard]] PandaGene pandaGene(std::int32_t roll);

} // namespace mobSpawnAttributes

namespace spawnAttributeNbt {

void setBase(wire::nbt::NbtCompound& data, std::string_view attributeId, double base);

void addModifier(wire::nbt::NbtCompound& data, std::string_view attributeId, double base, std::string_view modifierId,
                 double amount, entity::AttributeOperation operation);

} // namespace spawnAttributeNbt

} // namespace basalt::server::runtime::world
