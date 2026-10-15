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

/// parity: `CushionItem.useOn` and `Cushion`.
namespace cushions {

inline constexpr double kWidth = 1.0;
inline constexpr double kHeight = 0.25;

/// parity: `Cushion.DATA_COLOR`.
inline constexpr std::uint8_t kColorDataIndex = 8;

[[nodiscard]] const wire::common::Identifier& type();

[[nodiscard]] bool isCushion(const wire::common::Identifier& type);

[[nodiscard]] std::optional<std::string_view> colourOfItem(const wire::common::Identifier& item);

/// parity: `DyeColor`'s ids, `white` being zero.
[[nodiscard]] std::optional<std::int32_t> dyeIdOf(std::string_view colour);

/// The colour a cushion carries, `white` when it says nothing. parity: `Cushion.DEFAULT_COLOR`.
[[nodiscard]] std::string_view colourOf(const entity::ServerEntity& cushion);

/// The item it drops. parity: `Cushion.getCushionItemStackWithData`.
[[nodiscard]] wire::common::Identifier itemOf(const entity::ServerEntity& cushion);

using Tagged = std::function<bool(std::string_view tag, const wire::common::Identifier& block)>;

/// parity: `Cushion.canBePlacedAt`.
[[nodiscard]] bool canBePlacedAt(const WorldState& world, double x, double y, double z, const Tagged& tagged);

/// parity: `Cushion.wouldSurviveAt`, what the check interval asks of one already down.
[[nodiscard]] bool survivesAt(const WorldState& world, double x, double y, double z, const Tagged& tagged);

/// parity: `serverLevel.getEntitiesOfClass(Cushion.class, spawnAABB)`.
[[nodiscard]] bool occupied(const WorldState& world, double x, double y, double z);

/// parity: `Direction.fromYRot(rotation).toYRot()`.
[[nodiscard]] float snappedYaw(float yaw);

/// parity: `VoxelShape.clip`.
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
