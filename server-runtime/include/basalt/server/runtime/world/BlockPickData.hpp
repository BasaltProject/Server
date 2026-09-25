#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>

namespace basalt::server::world {
class ChunkBlockEntity;
}

namespace basalt::server::runtime::world {

/// What a creative player's middle click copies from a block onto the stack it picks.
namespace blockPickData {

/// parity: `ServerGamePacketListenerImpl.addBlockDataToItem`: the block entity's implicit components
/// (`collectImplicitComponents`), then what is left of its tag as `block_entity_data`.
void addBlockData(const server::world::ChunkBlockEntity& entity, wire::nbt::NbtCompound& components);

/// parity: the `getCloneItemStack` overrides that look at the block, the ones that do even without
/// the data asked for (a banner's patterns, a pot's sherds) and the ones that do only with it.
void addCloneComponents(std::int32_t stateId, const server::world::ChunkBlockEntity* entity, bool includeData,
                        wire::nbt::NbtCompound& components);

} // namespace blockPickData
} // namespace basalt::server::runtime::world
