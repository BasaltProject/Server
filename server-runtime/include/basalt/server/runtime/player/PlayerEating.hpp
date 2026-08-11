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

/// What a drink that clears every effect spares.
struct ClearScope {
    bool keepBeneficial = false;
    bool keepOmens = false;
};

void applyConsumeEffectsFor(PlayerSession& player, const std::vector<server::registry::ConsumeEffect>& effects,
                            const std::function<float()>& nextFloat, ClearScope scope = {});

bool eatHeldItem(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                 const std::function<float()>& nextFloat);

} // namespace basalt::server::runtime::player
