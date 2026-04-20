#pragma once

#include <basalt/server/registry/ItemFood.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

void applyConsumeEffectsFor(PlayerSession& player, const std::vector<server::registry::ConsumeEffect>& effects,
                            const std::function<float()>& nextFloat);

bool eatHeldItem(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                 const std::function<float()>& nextFloat);

} // namespace basalt::server::runtime::player
