#pragma once

#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Anvils.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::bedrock {

/// What to tell the client after its request: whether it stood, and what the windows it named hold
/// now. A refused request carries nothing, and the caller re-sends the whole inventory instead.
struct StackRequestAnswer {
    bool accepted = false;
    std::vector<wire::bedrock::BedrockResponseContainer> containers;
    /// The merchant this request bought from, when it was a trade, and what the deal was worth to
    /// it. The offer's own use is counted where the window keeps it; the experience belongs to the
    /// mob, which lives where the world is.
    std::optional<std::int32_t> tradedWith;
    std::int32_t tradeExperience = 0;
    /// What an anvil charged, when the request took its result. The levels and the block wearing
    /// out belong where the world is, so only the number comes back.
    std::optional<std::int32_t> anvilCost;
};

/// Moves stacks around one player's windows, as their client asked. Every line is applied in order
/// and the first that cannot be ends it: a request is all or nothing on the client too. `world` is
/// where an open block's window lives and `drop` puts a stack into it; without either, refused.
/// What the request needs of the rest of the server: where a dropped stack goes, and what a recipe
/// number means. A recipe nobody can name is a request that is refused.
struct StackRequestContext {
    std::function<void(const storage::StoredItemStack&)> drop;
    /// The stack that recipe makes, by the number this server sent the client at login.
    std::function<std::optional<storage::StoredItemStack>(std::int32_t recipeNetId)> resultOf;
    /// What an anvil makes of the two things in it and what it charges, asked where the registries
    /// are. Unset leaves an anvil making nothing.
    std::function<world::anvils::Result(const storage::StoredItemStack* input, const storage::StoredItemStack* addition,
                                        const std::optional<std::string>& name)>
        anvilResult;
    /// Whether this player can pay that many levels, which only survival ever answers no to.
    std::function<bool(std::int32_t cost)> canPay;
};

[[nodiscard]] StackRequestAnswer applyStackRequest(player::PlayerSession& session, BedrockWindows& windows,
                                                   world::LoadedWorld* world,
                                                   const wire::bedrock::BedrockStackRequest& request,
                                                   const StackRequestContext& context);

} // namespace basalt::server::runtime::bedrock
