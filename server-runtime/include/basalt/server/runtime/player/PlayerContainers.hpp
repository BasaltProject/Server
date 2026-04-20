#pragma once

#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::player {

struct OpenersContext {
    BreakContext blocks;
    /// parity: `ChestBlockEntity.playSound` rolls a tenth of a tone before it plays.
    std::function<float()> nextFloat;
    std::function<std::int64_t()> nextSeed;
};

/// parity: `ContainerOpenersCounter`, which names the player on the first open and the last close.
void announceOpeners(const OpenersContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                     std::int32_t z, std::string_view block, std::int32_t openers,
                     std::optional<wire::common::Uuid> opener = std::nullopt);

} // namespace basalt::server::runtime::player
