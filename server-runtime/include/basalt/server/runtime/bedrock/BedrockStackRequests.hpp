#pragma once

#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Anvils.hpp>
#include <basalt/server/runtime/world/Grindstones.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::bedrock {

struct BeaconChoice {
    std::optional<std::int32_t> primary;
    std::optional<std::int32_t> secondary;
};

struct StackRequestAnswer {
    bool accepted = false;
    std::vector<wire::bedrock::BedrockResponseContainer> containers;
    std::optional<std::int32_t> tradedWith;
    std::int32_t tradeExperience = 0;
    std::optional<std::int32_t> anvilCost;
    std::optional<std::int32_t> grindstoneWorth;
    std::optional<std::int32_t> enchantCost;
    std::optional<BeaconChoice> beacon;
};

struct StackRequestContext {
    std::function<void(const storage::StoredItemStack&)> drop;
    std::function<std::optional<storage::StoredItemStack>(std::int32_t recipeNetId)> resultOf;
    std::function<world::anvils::Result(const storage::StoredItemStack* input, const storage::StoredItemStack* addition,
                                        const std::optional<std::string>& name)>
        anvilResult;
    std::function<bool(std::int32_t cost)> canPay;
    std::function<world::grindstones::Result(const storage::StoredItemStack* input,
                                             const storage::StoredItemStack* addition)>
        grindstoneResult;
    std::function<std::optional<storage::StoredItemStack>(const storage::StoredItemStack* templateStack,
                                                          const storage::StoredItemStack* base,
                                                          const storage::StoredItemStack* addition)>
        smithingResult;
    std::function<std::optional<storage::StoredItemStack>(
        const storage::StoredItemStack& banner, const storage::StoredItemStack& dye,
        const storage::StoredItemStack* patternItem, const std::string& pattern)>
        loomResult;
    std::function<std::optional<storage::StoredItemStack>(const storage::StoredItemStack& map,
                                                          const storage::StoredItemStack& additional)>
        cartographyResult;
};

[[nodiscard]] StackRequestAnswer applyStackRequest(player::PlayerSession& session, BedrockWindows& windows,
                                                   world::LoadedWorld* world,
                                                   const wire::bedrock::BedrockStackRequest& request,
                                                   const StackRequestContext& context);

} // namespace basalt::server::runtime::bedrock
