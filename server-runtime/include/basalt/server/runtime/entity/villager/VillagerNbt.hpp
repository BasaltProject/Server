#pragma once

namespace basalt::wire::nbt {
class NbtCompound;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

/// A villager's own save data beyond what every entity writes.
///
/// parity: `Villager.addAdditionalSaveData` and `readAdditionalSaveData`, with the brain's
/// serializable memories under `Brain.memories` as `Brain.pack` writes them.
namespace basalt::server::runtime::entity::villager::villagerNbt {

void write(const ServerEntity& villager, wire::nbt::NbtCompound& tag);

void read(const wire::nbt::NbtCompound& tag, ServerEntity& villager);

} // namespace basalt::server::runtime::entity::villager::villagerNbt
