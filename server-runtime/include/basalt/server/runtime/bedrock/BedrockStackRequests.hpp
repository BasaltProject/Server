#pragma once

#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <functional>
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
[[nodiscard]] StackRequestAnswer applyStackRequest(player::PlayerSession& session, BedrockWindows& windows,
                                                   world::LoadedWorld* world,
                                                   const wire::bedrock::BedrockStackRequest& request,
                                                   const std::function<void(const storage::StoredItemStack&)>& drop);

} // namespace basalt::server::runtime::bedrock
