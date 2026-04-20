#pragma once

#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

bool toggleBlockFor(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                    std::int32_t z, float yaw);

bool tuneNoteBlockFor(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                      std::int32_t z, PlayerSession* player = nullptr);

bool ringBellFor(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                 wire::protocol::play::Direction face, float clickY, PlayerSession* player = nullptr);

bool cycleBlockFor(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                   std::int32_t z);

} // namespace basalt::server::runtime::player
