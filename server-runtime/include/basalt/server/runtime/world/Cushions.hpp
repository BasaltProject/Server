#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// 26.3's cushion: an entity put on top of a block that a player sits on.
///
/// parity: `CushionItem.useOn` and `Cushion`. What it rests on is read off collision boxes, where
/// vanilla asks the outline shape, which only differs for blocks that have an outline and no
/// collision (a flower, tall grass).
namespace cushions {

/// parity: `EntityTypes.CUSHION`'s `sized(1.0F, 0.25F)`.
inline constexpr double kWidth = 1.0;
inline constexpr double kHeight = 0.25;

/// parity: `Cushion.DATA_COLOR`, right after `Entity`'s eight fields; `DyeColor` goes as its id.
inline constexpr std::uint8_t kColorDataIndex = 8;

/// parity: `EntityDataSerializers.DYE_COLOR`'s place in the serializer list.
inline constexpr std::int32_t kDyeColorSerializer = 43;

[[nodiscard]] const wire::common::Identifier& type();

[[nodiscard]] bool isCushion(const wire::common::Identifier& type);

/// The dye of a `<colour>_cushion` item, or nothing for any other item.
[[nodiscard]] std::optional<std::string_view> colourOfItem(const wire::common::Identifier& item);

/// parity: `DyeColor`'s ids, `white` being zero.
[[nodiscard]] std::optional<std::int32_t> dyeIdOf(std::string_view colour);

/// The colour a cushion carries, `white` when it says nothing. parity: `Cushion.DEFAULT_COLOR`.
[[nodiscard]] std::string_view colourOf(const entity::ServerEntity& cushion);

/// The item it drops. parity: `Cushion.getCushionItemStackWithData`.
[[nodiscard]] wire::common::Identifier itemOf(const entity::ServerEntity& cushion);

/// Whether a block tag holds a block, for `causes_suffocation` and `cushion_uses_collision_shape`.
using Tagged = std::function<bool(std::string_view tag, const wire::common::Identifier& block)>;

/// parity: `Cushion.canBePlacedAt` for the box whose bottom middle is `x, y, z`: something to rest
/// on below, a resting slice not buried in collision, and not wholly inside suffocating blocks.
[[nodiscard]] bool canBePlacedAt(const WorldState& world, double x, double y, double z, const Tagged& tagged);

/// parity: `Cushion.wouldSurviveAt`, what the check interval asks of one already down.
[[nodiscard]] bool survivesAt(const WorldState& world, double x, double y, double z, const Tagged& tagged);

/// parity: `serverLevel.getEntitiesOfClass(Cushion.class, spawnAABB)`: one cushion per spot.
[[nodiscard]] bool occupied(const WorldState& world, double x, double y, double z);

/// parity: `Direction.fromYRot(rotation).toYRot()`, the quarter turn a new cushion is laid at.
[[nodiscard]] float snappedYaw(float yaw);

/// Where a ray from `from` to `to` first meets a block's collision boxes: the face it crossed and
/// the height it met it at. parity: `VoxelShape.clip`, including its answer for a ray that starts
/// inside, which is used by `CushionItem.recalculateContextForSpecialCollisionShapes`.
struct Clip {
    bool up = false;
    double y = 0.0;
};

[[nodiscard]] std::optional<Clip> clipCollision(std::int32_t stateId, std::int32_t blockX, std::int32_t blockY,
                                                std::int32_t blockZ, const std::array<double, 3>& from,
                                                const std::array<double, 3>& to);

/// The tag a new cushion is saved with. parity: `Cushion.addAdditionalSaveData`.
[[nodiscard]] wire::nbt::NbtCompound freshTag(std::string_view colour, const std::array<std::int32_t, 3>& blockPos);

} // namespace cushions

} // namespace basalt::server::runtime::world
