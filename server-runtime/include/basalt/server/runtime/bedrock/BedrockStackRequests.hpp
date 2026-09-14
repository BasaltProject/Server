#pragma once

#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::bedrock {

/// What to tell the client after its request: whether it stood, and what the windows it named hold
/// now. A refused request carries nothing, and the caller re-sends the whole inventory instead.
struct StackRequestAnswer {
    bool accepted = false;
    std::vector<wire::bedrock::BedrockResponseContainer> containers;
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
};

[[nodiscard]] StackRequestAnswer applyStackRequest(player::PlayerSession& session, BedrockWindows& windows,
                                                   world::LoadedWorld* world,
                                                   const wire::bedrock::BedrockStackRequest& request,
                                                   const StackRequestContext& context);

} // namespace basalt::server::runtime::bedrock
