#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class WorldState;

/// A beehive or bee nest's block entity: the bees inside, how long each has been in, and the flower
/// it remembers. Everything lives in the block entity's own saved form.
///
/// parity: `BeehiveBlockEntity`, less the release, which needs the entity machinery.
namespace beehives {

inline constexpr std::size_t kMaxOccupants = 3;
/// parity: `MIN_OCCUPATION_TICKS_NECTAR` and `MIN_OCCUPATION_TICKS_NECTARLESS`.
inline constexpr std::int32_t kMinTicksNectar = 2400;
inline constexpr std::int32_t kMinTicksNectarless = 600;
/// parity: `BeehiveBlock.MAX_HONEY_LEVELS`.
inline constexpr std::int32_t kMaxHoney = 5;
/// parity: the `stayOutOfHiveCountdown` a sedated hive gives the bees a player lets out.
inline constexpr std::int32_t kSedatedStayOut = 400;
/// parity: `angerNearbyBees`' box, eight across and six up round the hive.
inline constexpr double kAngerReachXZ = 8.0;
inline constexpr double kAngerReachY = 6.0;

/// parity: `BeehiveBlockEntity.BeeReleaseStatus`.
enum class Release : std::uint8_t { HoneyDelivered, BeeReleased, Emergency };

using Pos = std::array<std::int32_t, 3>;

/// parity: the `beehives` block tag.
[[nodiscard]] bool isHive(std::string_view blockPath);

[[nodiscard]] std::size_t occupantCount(const wire::nbt::NbtCompound& data);

[[nodiscard]] bool isFull(const wire::nbt::NbtCompound& data);

/// parity: `storeBee(Occupant.of(bee))`. `bee` is the saved bee with its id, not yet stripped.
void addOccupant(wire::nbt::NbtCompound& data, wire::nbt::NbtCompound bee);

/// Counts every bee's stay on by one. parity: `BeeData.tick`, whose answer is whether the bee's time
/// is up; those indices are left in `due` in order.
void tickOccupants(wire::nbt::NbtCompound& data, std::vector<std::size_t>& due);

/// The occupant at `index`, as `entity_data`, `ticks_in_hive` and `min_ticks_in_hive`.
[[nodiscard]] const wire::nbt::NbtCompound* occupant(const wire::nbt::NbtCompound& data, std::size_t index);

/// Takes the occupants at `indices`, which are ascending.
void removeOccupants(wire::nbt::NbtCompound& data, const std::vector<std::size_t>& indices);

[[nodiscard]] std::optional<Pos> flowerOf(const wire::nbt::NbtCompound& data);

void setFlower(wire::nbt::NbtCompound& data, const Pos& flower);

/// parity: `BeehiveBlockEntity.isFireNearby`, fire in the three by three by three round the hive.
[[nodiscard]] bool isFireNearby(const WorldState& world, const Pos& hive);

/// parity: `CampfireBlock.isSmokeyPos`, which is what sedates a hive.
[[nodiscard]] bool isSmokey(const WorldState& world, const Pos& at);

} // namespace beehives

} // namespace basalt::server::runtime::world
