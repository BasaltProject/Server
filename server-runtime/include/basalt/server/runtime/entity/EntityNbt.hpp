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

/// parity: `Entity.saveWithoutId`.

using EntityById = std::function<ServerEntity*(std::int32_t entityId)>;

using AllocateEntityId = std::function<std::int32_t()>;

using AllocateEntityIdFor = std::function<std::int32_t(const wire::common::Identifier& type)>;

using ProtocolTypeOf = std::function<std::optional<std::int32_t>(const wire::common::Identifier& type)>;

[[nodiscard]] wire::nbt::NbtCompound encode(const ServerEntity& entity);

[[nodiscard]] wire::nbt::NbtCompound encodeTree(const ServerEntity& root, const EntityById& entityById);

[[nodiscard]] std::unique_ptr<ServerEntity> decode(const wire::nbt::NbtCompound& tag, std::int32_t entityId,
                                                   std::int32_t protocolType);

[[nodiscard]] std::vector<std::unique_ptr<ServerEntity>> decodeTree(const wire::nbt::NbtCompound& tag,
                                                                    const AllocateEntityId& allocateEntityId,
                                                                    const ProtocolTypeOf& protocolTypeOf);

[[nodiscard]] std::vector<std::unique_ptr<ServerEntity>> decodeTree(const wire::nbt::NbtCompound& tag,
                                                                    const AllocateEntityIdFor& allocateEntityId,
                                                                    const ProtocolTypeOf& protocolTypeOf);

[[nodiscard]] std::unique_ptr<ServerEntity> decodeGenerated(const server::world::ChunkGeneratedEntity& descriptor,
                                                            std::int32_t entityId, std::int32_t protocolType);

[[nodiscard]] std::vector<std::unique_ptr<ServerEntity>>
decodeGeneratedTree(const server::world::ChunkGeneratedEntity& descriptor, const AllocateEntityId& allocateEntityId,
                    const ProtocolTypeOf& protocolTypeOf);

[[nodiscard]] std::vector<std::unique_ptr<ServerEntity>>
decodeGeneratedTree(const server::world::ChunkGeneratedEntity& descriptor, const AllocateEntityIdFor& allocateEntityId,
                    const ProtocolTypeOf& protocolTypeOf);

[[nodiscard]] double numericDouble(const wire::nbt::NbtCompound& tag, std::string_view key);

[[nodiscard]] std::int32_t numericInt(const wire::nbt::NbtCompound& tag, std::string_view key);

} // namespace basalt::server::runtime::entity::entityNbt
