#pragma once

#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;

/// A door, a trapdoor, a gate, a lever or a button, flipped by hand: the state, what the neighbours
/// owe it, the noise it makes and what a sculk sensor hears. Answers whether anything moved.
///
/// Shared because both doors flip the same blocks: the Java session runs it through its own handler
/// and the Bedrock one has no handler at all.
bool toggleBlockFor(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                    std::int32_t z, float yaw);

/// A note block moved on one note and played. Answers whether there was one there.
bool tuneNoteBlockFor(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                      std::int32_t z);

/// A bell rung by hand, which only answers a side that can swing it: `face` is the side that was
/// clicked and `clickY` how far up it, both as the click arrived.
bool ringBellFor(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                 wire::protocol::play::Direction face, float clickY);

/// A repeater, a comparator or a daylight detector, moved on one step by hand.
bool cycleBlockFor(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                   std::int32_t z);

} // namespace basalt::server::runtime::player
