#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class WorldState;

namespace beehives {

inline constexpr std::size_t kMaxOccupants = 3;
inline constexpr std::int32_t kMinTicksNectar = 2400;
inline constexpr std::int32_t kMinTicksNectarless = 600;
inline constexpr std::int32_t kMaxHoney = 5;
inline constexpr std::int32_t kSedatedStayOut = 400;
inline constexpr double kAngerReachXZ = 8.0;
inline constexpr double kAngerReachY = 6.0;

enum class Release : std::uint8_t { HoneyDelivered, BeeReleased, Emergency };

using Pos = std::array<std::int32_t, 3>;

[[nodiscard]] bool isHive(std::string_view blockPath);

[[nodiscard]] std::size_t occupantCount(const wire::nbt::NbtCompound& data);

[[nodiscard]] bool isFull(const wire::nbt::NbtCompound& data);

/// parity: `storeBee(Occupant.of(bee))`.
void addOccupant(wire::nbt::NbtCompound& data, wire::nbt::NbtCompound bee);

void tickOccupants(wire::nbt::NbtCompound& data, std::vector<std::size_t>& due);

[[nodiscard]] const wire::nbt::NbtCompound* occupant(const wire::nbt::NbtCompound& data, std::size_t index);

void removeOccupants(wire::nbt::NbtCompound& data, const std::vector<std::size_t>& indices);

[[nodiscard]] std::optional<Pos> flowerOf(const wire::nbt::NbtCompound& data);

void setFlower(wire::nbt::NbtCompound& data, const Pos& flower);

[[nodiscard]] bool isFireNearby(const WorldState& world, const Pos& hive);

[[nodiscard]] bool isSmokey(const WorldState& world, const Pos& at);

} // namespace beehives

} // namespace basalt::server::runtime::world
