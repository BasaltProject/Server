#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world::mobBuckets {

/// parity: the seven `MobBucketItem` registrations.
struct Kind {
    std::string_view item;
    std::string_view entity;
    std::string_view fillSound;
    std::string_view emptySound;
    bool water = true;
};

[[nodiscard]] const Kind* ofItem(const wire::common::Identifier& item);

[[nodiscard]] const Kind* ofEntity(const wire::common::Identifier& type);

/// parity: `Bucketable.canBePickedUpWithBucket`.
[[nodiscard]] bool pickedUpWith(const Kind& kind, const wire::common::Identifier& held);

[[nodiscard]] bool isFish(const wire::common::Identifier& type);

/// parity: `Bucketable.saveDefaultDataToBucketTag` and each mob's `saveToBucketTag`.
[[nodiscard]] storage::StoredItemStack bucketOf(const Kind& kind, const entity::ServerEntity& mob);

/// parity: `MobBucketItem.spawn`, which reads the stack's components and its `bucket_entity_data`.
[[nodiscard]] std::unique_ptr<entity::ServerEntity> spawnFrom(const Kind& kind,
                                                              const wire::nbt::NbtCompound& components,
                                                              std::int32_t entityId, std::int32_t protocolType,
                                                              double x, double y, double z, float yaw);

/// parity: `TropicalFish.Pattern.getPackedId`.
[[nodiscard]] std::optional<std::int32_t> tropicalPatternId(std::string_view name);

} // namespace basalt::server::runtime::world::mobBuckets
