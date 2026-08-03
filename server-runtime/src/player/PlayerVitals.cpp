#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerVitals.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>

#include <algorithm>
#include <string>

namespace basalt::server::runtime::player {
namespace {

using wire::protocol::play::Gamemode;

constexpr std::string_view kDeathStarve = "starve";

} // namespace

Difficulty difficultyOf(const std::string_view name) {
    if (name == "peaceful") {
        return Difficulty::Peaceful;
    }
    if (name == "normal") {
        return Difficulty::Normal;
    }
    if (name == "hard") {
        return Difficulty::Hard;
    }
    return Difficulty::Easy;
}

void tickVitals(PlayerSession& player, const VitalsContext& context) {
    if (player.invulnerableTicks > 0) {
        player.invulnerableTicks--;
    }
    // parity: `Player.tick`, which counts this up for as long as the player is not swinging.
    player.ticksSinceSwing++;
    if (context.armorWatch) {
        context.armorWatch(player);
    }
    if (player.dead || player.gamemode == Gamemode::Creative || player.gamemode == Gamemode::Spectator) {
        return;
    }

    const float healthBefore = player.health();
    const std::int32_t foodBefore = player.food.foodLevel();
    const float saturationBefore = player.food.saturationLevel();
    player.food.tick(
        context.difficulty, player.health(), player.maxHealth(),
        [&player, &context](const float amount, const bool fast) {
            float healed = amount;
            if (context.healed) {
                // parity: `FoodData.tick` heals with `RegainReason.SATIATED`.
                const std::optional<float> allowed = context.healed(player, amount, "satiated", fast);
                if (!allowed.has_value()) {
                    return;
                }
                healed = *allowed;
            }
            player.setHealth(std::min(player.health() + healed, player.maxHealth()));
        },
        [&player](const float amount) {
            if (player.connection().hurt) {
                player.connection().hurt(amount, kDeathStarve, world::damageTypes::kStarve, std::nullopt);
            }
        },
        context.naturalRegeneration);
    if (player.food.foodLevel() != foodBefore && context.foodChanged) {
        const std::optional<std::int32_t> wanted = context.foodChanged(player, player.food.foodLevel(), foodBefore);
        if (wanted.has_value() && *wanted != player.food.foodLevel()) {
            player.food.set(*wanted, player.food.saturationLevel());
        }
    }
    // Only when something moved: a health packet every tick is twenty a second per player.
    if (player.health() != healthBefore || player.food.foodLevel() != foodBefore ||
        player.food.saturationLevel() != saturationBefore) {
        if (player.connection().syncHealth) {
            player.connection().syncHealth();
        }
    }
}

} // namespace basalt::server::runtime::player
