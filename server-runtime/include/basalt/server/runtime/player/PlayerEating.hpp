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

/// Starts what a mouthful leaves behind: the effects a golden apple or a stew carries, the ones a
/// bucket of milk clears, and the rolls the chancy ones make.
void applyConsumeEffectsFor(PlayerSession& player, const std::vector<server::registry::ConsumeEffect>& effects,
                            const std::function<float()>& nextFloat);

/// Swallows what is in that slot: the food and saturation, the effects, the stack it comes off and
/// whatever the wrapper leaves behind. Answers false when that slot holds nothing edible.
///
/// Shared because a mouthful is a mouthful: the Java session runs a clock of its own and swallows at
/// the end of it, while a Bedrock client eats on its own and tells the server it is done.
bool eatHeldItem(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                 const std::function<float()>& nextFloat);

} // namespace basalt::server::runtime::player
