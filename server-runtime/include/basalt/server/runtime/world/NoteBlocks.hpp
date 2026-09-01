#pragma once

#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// The note block: which instrument it plays and when it plays it.
///
/// parity: `NoteBlock`. The instrument is not chosen when the note is played, it is a property of
/// the state, written when the block is placed and rewritten whenever what is above or below it
/// changes.
namespace noteBlocks {

/// parity: `NoteBlock.NOTE_VOLUME`, loud enough to carry.
inline constexpr float kNoteVolume = 3.0F;

/// parity: `NoteBlock.NOTE`, two octaves.
inline constexpr std::int32_t kNotes = 25;

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `NoteBlock.setInstrument`: a head on top wins, and otherwise the block underneath.
[[nodiscard]] const registry::NoteBlockInstrument& instrumentAt(const WorldState& world, std::int32_t x, std::int32_t y,
                                                                std::int32_t z);

/// parity: `NoteBlock.updateShape` on the Y axis, which is the only one that matters here.
[[nodiscard]] std::optional<std::int32_t> retune(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 std::int32_t stateId);

/// parity: `NoteBlock.getStateForPlacement`, which picks the instrument before the block lands.
[[nodiscard]] std::int32_t placed(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::int32_t stateId);

/// parity: `NoteBlock.playNote`: a note block with something solid on top stays quiet, unless the
/// instrument is the kind that lives up there in the first place.
[[nodiscard]] std::vector<BlockEffect> playNote(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                std::int32_t stateId);

/// parity: `NoteBlock.useWithoutItem`, which moves the note on one and plays it.
[[nodiscard]] std::optional<BlockChange> tune(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `NoteBlock.attack`, which plays the note without changing it.
[[nodiscard]] std::vector<BlockEffect> strike(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace noteBlocks

} // namespace basalt::server::runtime::world
