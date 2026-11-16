#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// Lets a listener veto or resize a harvest: block path, item path and the count.
using HarvestGate = std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                                       std::string_view block, std::string_view item, std::int32_t& count)>;

/// parity: `SweetBerryBushBlock.useWithoutItem` and `CaveVines.use`; false when the block has nothing to pick.
bool harvestBerries(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                    std::int32_t y, std::int32_t z, const HarvestGate& gate);

} // namespace basalt::server::runtime::player
