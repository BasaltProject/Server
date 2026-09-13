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

/// Where a click landed, in the terms both editions send it in: the block, which face, and where on
/// that face.
struct BlockHit {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    wire::protocol::play::Direction direction = wire::protocol::play::Direction::Up;
    float clickX = 0.0F;
    float clickY = 0.0F;
    float clickZ = 0.0F;
    /// Whether the click came from inside the block, which is what a scaffolding climb sends.
    bool inside = false;
};

/// What putting a block down needs of the server, on top of what changing one needs.
///
/// Placing is the same everywhere: the same state is worked out from where the click landed, the
/// same second half goes down with a door, the same neighbours are told and the same stack shrinks.
/// What differs is only the screen a new sign opens on, which is a callback.
struct PlaceContext {
    BreakContext blocks;

    std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       const std::string& item)>
        placeAllowed;

    /// Puts the whole bag back on the client, which is what a refusal costs: it already took the
    /// item out of its own hand.
    std::function<void(const PlayerSession& player)> sendInventory;

    /// A coin flip, for the one block whose placement asks for one.
    std::function<bool()> nextBool;

    /// Opens the editor over a sign that has just gone down, where that edition has one.
    std::function<void(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId)>
        openSign;
};

/// Puts the held block where the click says, as a player placing it does. Answers whether anything
/// went down.
bool placeBlockFor(PlayerSession& player, world::LoadedWorld& world, const BlockHit& hit,
                   const wire::common::Identifier& heldBlock, std::int32_t heldSlot,
                   const storage::StoredItemStack& held, const PlaceContext& context);

} // namespace basalt::server::runtime::player
