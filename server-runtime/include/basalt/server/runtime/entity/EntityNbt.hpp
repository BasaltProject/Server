#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/world/ChunkTypes.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity::entityNbt {

/// An entity as it waits on disk between one run and the next.
///
/// parity: `Entity.saveWithoutId`, in the keys it shares with it (`id`, `Pos`, `Motion`,
/// `Rotation`, `Health`, `Age`) so a future reader of vanilla's own format recognises them. What is
/// only this server's, like a villager's level, keeps its own name.

/// The entity with this runtime id, for walking a vehicle and its riders.
using EntityById = std::function<ServerEntity*(std::int32_t entityId)>;

/// A fresh runtime id. A saved id is not worth keeping: every run hands out its own.
using AllocateEntityId = std::function<std::int32_t()>;

/// The protocol number of a type, or nothing for one this build does not know.
using ProtocolTypeOf = std::function<std::optional<std::int32_t>(const wire::common::Identifier& type)>;

[[nodiscard]] wire::nbt::NbtCompound encode(const ServerEntity& entity);

/// Encodes one vehicle root and all of its riders through vanilla's nested `Passengers` shape.
[[nodiscard]] wire::nbt::NbtCompound encodeTree(const ServerEntity& root, const EntityById& entityById);

/// The entity that tag describes, or nothing when this build has no such type. The runtime id and
/// the protocol type come from the caller.
[[nodiscard]] std::unique_ptr<ServerEntity> decode(const wire::nbt::NbtCompound& tag, std::int32_t entityId,
                                                   std::int32_t protocolType);

/// Restores one saved vehicle root and its nested passengers, allocating fresh runtime ids.
[[nodiscard]] std::vector<std::unique_ptr<ServerEntity>> decodeTree(const wire::nbt::NbtCompound& tag,
                                                                    const AllocateEntityId& allocateEntityId,
                                                                    const ProtocolTypeOf& protocolTypeOf);

/// Materializes one transformed structure entity through the same decoder used on chunk load.
[[nodiscard]] std::unique_ptr<ServerEntity> decodeGenerated(const server::world::ChunkGeneratedEntity& descriptor,
                                                            std::int32_t entityId, std::int32_t protocolType);

/// Materializes one generated vehicle root and its passenger descriptors with fresh runtime ids.
[[nodiscard]] std::vector<std::unique_ptr<ServerEntity>>
decodeGeneratedTree(const server::world::ChunkGeneratedEntity& descriptor, const AllocateEntityId& allocateEntityId,
                    const ProtocolTypeOf& protocolTypeOf);

} // namespace basalt::server::runtime::entity::entityNbt
