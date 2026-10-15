#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `ChiseledBookShelfBlock`.
namespace chiseledBookshelves {

inline constexpr std::int32_t kSize = 6;
inline constexpr std::int32_t kRows = 2;
inline constexpr std::int32_t kColumns = 3;

[[nodiscard]] const wire::common::Identifier& block();

[[nodiscard]] bool isBookshelf(std::string_view path);

[[nodiscard]] std::optional<std::int32_t> slotAt(std::int32_t stateId, std::string_view face, float clickX,
                                                 float clickY, float clickZ);

struct Interaction {
    BlockChange change;
    std::optional<wire::common::Identifier> taken;
};

/// parity: `addBook` only fills an empty slot and marks it as the one a comparator reads.
[[nodiscard]] std::optional<Interaction> putBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 std::int32_t slot, const wire::common::Identifier& book);

[[nodiscard]] std::optional<Interaction> takeBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t slot);

/// parity: `getAnalogOutputSignal`.
[[nodiscard]] std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data);

/// parity: `ChiseledBookShelfBlockEntity.updateState` runs on every slot write.
[[nodiscard]] std::optional<std::int32_t> slotsChanged(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::int32_t touchedSlot, std::int64_t gameTime);

} // namespace chiseledBookshelves

} // namespace basalt::server::runtime::world
