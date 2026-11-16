#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/runtime/world/Trades.hpp>

#include <string>
#include <vector>

namespace basalt::server::runtime::plugins::tradeEvents {

/// The offer as the trade events carry it: items by id and count, plus the numbers a recipe has.
void fill(server::plugins::PluginEvent& event, const world::trades::Offer& offer);

/// Applies what a listener edited; an item whose id did not change keeps its components.
void apply(const server::plugins::PluginEvent& event, world::trades::Offer& offer);

/// Lines of `item|count` for the loot events, and back.
[[nodiscard]] std::string linesOf(const std::vector<server::world::loot::ItemStack>& stacks);
[[nodiscard]] std::vector<server::world::loot::ItemStack> stacksOf(const std::string& lines);

} // namespace basalt::server::runtime::plugins::tradeEvents
