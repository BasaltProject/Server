#pragma once

#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::player {

class PlayerSession;

struct BlockHit {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    wire::protocol::play::Direction direction = wire::protocol::play::Direction::Up;
    float clickX = 0.0F;
    float clickY = 0.0F;
    float clickZ = 0.0F;
    bool inside = false;
};

struct PlaceContext {
    BreakContext blocks;

    std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       const std::string& item)>
        placeAllowed;

    std::function<void(const PlayerSession& player)> sendInventory;

    std::function<bool()> nextBool;

    std::function<void(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId)>
        openSign;

    std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)> placed;
};

bool placeBlockFor(PlayerSession& player, world::LoadedWorld& world, const BlockHit& hit,
                   const wire::common::Identifier& heldBlock, std::int32_t heldSlot,
                   const storage::StoredItemStack& held, const PlaceContext& context);

} // namespace basalt::server::runtime::player
