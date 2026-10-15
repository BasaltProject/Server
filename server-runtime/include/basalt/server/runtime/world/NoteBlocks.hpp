#pragma once

#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `NoteBlock`.
namespace noteBlocks {

inline constexpr float kNoteVolume = 3.0F;

inline constexpr std::int32_t kNotes = 25;

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `NoteBlock.setInstrument`.
[[nodiscard]] const registry::NoteBlockInstrument& instrumentAt(const WorldState& world, std::int32_t x, std::int32_t y,
                                                                std::int32_t z);

/// parity: `NoteBlock.updateShape` on the Y axis.
[[nodiscard]] std::optional<std::int32_t> retune(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 std::int32_t stateId);

/// parity: `NoteBlock.playNote`.
[[nodiscard]] std::vector<BlockEffect> playNote(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                std::int32_t stateId);

/// parity: `NoteBlock.useWithoutItem`, which moves the note on one and plays it.
[[nodiscard]] std::optional<BlockChange> tune(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `NoteBlock.attack`, which plays the note without changing it.
[[nodiscard]] std::vector<BlockEffect> strike(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace noteBlocks

} // namespace basalt::server::runtime::world
