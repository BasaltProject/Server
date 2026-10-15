#pragma once

namespace basalt::wire::nbt {
class NbtCompound;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::entity::villager::villagerNbt {

void write(const ServerEntity& villager, wire::nbt::NbtCompound& tag);

void read(const wire::nbt::NbtCompound& tag, ServerEntity& villager);

} // namespace basalt::server::runtime::entity::villager::villagerNbt
