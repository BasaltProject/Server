#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>

#include <cmath>
#include <limits>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::protocol::play::DisplaySlot;

[[nodiscard]] std::int32_t increment(std::int32_t value) {
    return value == std::numeric_limits<std::int32_t>::max() ? value : value + 1;
}

/// parity: `LivingEntity.getArmorValue`, the armour attribute the worn pieces add up to.
[[nodiscard]] std::int32_t armorOf(const PlayerSession& player) {
    float armor = 0.0F;
    for (std::int32_t slot = player::PlayerInventory::kArmourStart;
         slot < player::PlayerInventory::kArmourStart + player::PlayerInventory::kArmourSlots; ++slot) {
        const std::optional<storage::StoredItemStack>& stack = player.inventory.at(slot);
        if (!stack.has_value()) {
            continue;
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(stack->id);
        if (const registry::ArmourPiece* piece = id.has_value() ? registry::itemArmour::of(*id) : nullptr;
            piece != nullptr) {
            armor += piece->armour;
        }
    }
    return static_cast<std::int32_t>(std::floor(armor));
}

} // namespace

void BasaltServer::updateCriterion(std::string_view criterion, std::string_view holder,
                                   const std::function<std::int32_t(std::int32_t)>& update) {
    const std::vector<std::pair<std::string, std::int32_t>> touched =
        scoreboard_.forAllObjectives(criterion, holder, update);
    if (touched.empty()) {
        return;
    }
    const std::vector<PlayerSession*> online = playerRegistry_.snapshot();
    for (const auto& [objective, value] : touched) {
        // parity: `ServerScoreboard.onScoreChanged` only tells clients about an objective on show.
        bool shown = false;
        for (const DisplaySlot slot : {DisplaySlot::List, DisplaySlot::Sidebar, DisplaySlot::BelowName}) {
            const std::string* displayed = scoreboard_.displayed(slot);
            shown = shown || (displayed != nullptr && *displayed == objective);
        }
        if (!shown) {
            continue;
        }
        for (PlayerSession* player : online) {
            if (player->connection().sendScore) {
                player->connection().sendScore(std::string{holder}, objective, value);
            }
        }
    }
}

void BasaltServer::awardKillScore(PlayerSession& killer, const std::string& victim, bool victimIsPlayer) {
    if (killer.name() == victim) {
        return;
    }
    updateCriterion("totalKillCount", killer.name(), increment);
    if (victimIsPlayer) {
        player::statAwards::award(killer, "player_kills");
        updateCriterion("playerKillCount", killer.name(), increment);
    } else {
        player::statAwards::award(killer, "mob_kills");
    }
    // parity: `handleTeamKill`, by the colour of the other side's team.
    if (const scoreboard::Team* team = teams_.teamOf(victim); team != nullptr && team->color.has_value()) {
        updateCriterion("teamkill." + std::string{scoreboard::Objective::teamColorName(*team->color)}, killer.name(),
                        increment);
    }
    if (const scoreboard::Team* team = teams_.teamOf(killer.name()); team != nullptr && team->color.has_value()) {
        updateCriterion("killedByTeam." + std::string{scoreboard::Objective::teamColorName(*team->color)}, victim,
                        increment);
    }
}

void BasaltServer::tickScoreCriteria() {
    const std::vector<PlayerSession*> online = playerRegistry_.snapshot();
    const auto byEntityId = [&online](std::int32_t entityId) -> PlayerSession* {
        for (PlayerSession* player : online) {
            if (player->entityId() == entityId) {
                return player;
            }
        }
        return nullptr;
    };
    for (PlayerSession* player : online) {
        // parity: `LivingEntity.baseTick`, the memory counting down once a tick.
        if (player->lastHurtByPlayerTime.load() > 0) {
            player->lastHurtByPlayerTime.fetch_sub(1);
        }
        if (player->lastHurtByMobTime.load() > 0) {
            player->lastHurtByMobTime.fetch_sub(1);
        }
        for (std::int32_t deaths = player->deathsToScore.exchange(0); deaths > 0; --deaths) {
            updateCriterion("deathCount", player->name(), increment);
            if (player->killerEntityId.has_value()) {
                if (PlayerSession* killer = byEntityId(*player->killerEntityId); killer != nullptr) {
                    awardKillScore(*killer, player->name(), true);
                }
            }
            player->killerEntityId.reset();
        }
        PlayerSession::RecordedScores& recorded = player->recordedScores;
        const auto set = [](std::int32_t value) { return [value](std::int32_t) { return value; }; };
        const float health = player->health() + player->absorption;
        if (health != recorded.health) {
            recorded.health = health;
            updateCriterion("health", player->name(), set(static_cast<std::int32_t>(std::ceil(health))));
        }
        if (player->food.foodLevel() != recorded.food) {
            recorded.food = player->food.foodLevel();
            updateCriterion("food", player->name(), set(recorded.food));
        }
        if (player->airSupply != recorded.air) {
            recorded.air = player->airSupply;
            updateCriterion("air", player->name(), set(recorded.air));
        }
        if (const std::int32_t armor = armorOf(*player); armor != recorded.armor) {
            recorded.armor = armor;
            updateCriterion("armor", player->name(), set(armor));
        }
        if (player->experience.total() != recorded.experience) {
            recorded.experience = player->experience.total();
            updateCriterion("xp", player->name(), set(recorded.experience));
        }
        if (player->experience.level() != recorded.level) {
            recorded.level = player->experience.level();
            updateCriterion("level", player->name(), set(recorded.level));
        }
    }
    // parity: `ServerPlayer.awardStat` and `resetStat`, which reach the objectives counting them.
    for (PlayerSession* player : online) {
        for (const auto& [key, change] : player->stats.takeChanges()) {
            const std::string criterion = scoreboard::Objective::criterionOfStat(key);
            if (change.reset) {
                updateCriterion(criterion, player->name(), [](std::int32_t) { return 0; });
            }
            if (change.delta != 0) {
                const auto delta = static_cast<std::uint32_t>(change.delta);
                updateCriterion(criterion, player->name(), [delta](std::int32_t value) {
                    return static_cast<std::int32_t>(static_cast<std::uint32_t>(value) + delta);
                });
            }
        }
    }
    std::vector<PendingKill> kills;
    {
        const std::lock_guard<std::mutex> guard(pendingKillsLock_);
        kills.swap(pendingKills_);
    }
    for (const PendingKill& kill : kills) {
        if (PlayerSession* killer = playerRegistry_.get(kill.killer); killer != nullptr) {
            awardKillScore(*killer, kill.victim, false);
            // parity: `ServerPlayer.awardKillScore` ends in `PLAYER_KILLED_ENTITY.trigger`.
            if (advancementTriggers_ != nullptr) {
                advancementTriggers_->fire(
                    "player_killed_entity", killer->name(), kill.killer.toString(),
                    world::TriggerEvent{.entities = {{"entity", kill.victimFacts}}, .damage = kill.killingBlow});
            }
        }
    }
}

} // namespace basalt::server::runtime
