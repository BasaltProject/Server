#pragma once

#include <basalt/server/runtime/player/FoodData.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

/// What one tick of a player's own body needs from the server around it.
struct VitalsContext {
    Difficulty difficulty = Difficulty::Easy;
    /// parity: `GameRules.NATURAL_HEALTH_REGENERATION`, read from the player's world.
    bool naturalRegeneration = true;

    /// What a plugin says about a food level that has just changed on its own. Unset means nobody
    /// objects; a number is the level to use instead.
    std::function<std::optional<std::int32_t>(const PlayerSession& player, std::int32_t now, std::int32_t before)>
        foodChanged;
};

/// One tick of a player's body: the hurt cooldown running out, the swing clock, and hunger with
/// what it heals or costs. Everything it changes lives on the session.
///
/// Shared because a body is a body: the Java session used to do this in its own turn and a Bedrock
/// player has no turn, so a hit left them invulnerable for good and they never got hungry.
void tickVitals(PlayerSession& player, const VitalsContext& context);

/// The difficulty as the food clock wants it, from the word the config carries.
[[nodiscard]] Difficulty difficultyOf(std::string_view name);

} // namespace basalt::server::runtime::player
