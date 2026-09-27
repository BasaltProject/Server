#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime {

player::pvp::Context BasaltServer::meleeContext(const player::BreakContext& blocks) {
    return player::pvp::Context{.registry = &playerRegistry_,
                                .enchantments = &vanillaData_.enchantments,
                                .entityTags = &vanillaData_.entityTags,
                                .teams = &teams_,
                                .blocks = &blocks,
                                .sweptMob = {},
                                .nextSeed = blocks.nextSeed,
                                .explode = [this](world::LoadedWorld& world, const world::postAttack::Explosion& blast,
                                                  double x, double y, double z) {
                                    std::vector<player::PlayerSession*> watchers;
                                    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
                                        if (player->worldName == world.name()) {
                                            watchers.push_back(player);
                                        }
                                    }
                                    combat_->gust(world, watchers, world.state().entities(),
                                                  CombatSystem::gustOf(blast, x, y, z));
                                }};
}

void BasaltServer::bedrockAttackPlayer(player::PlayerSession& session, player::PlayerSession& victim,
                                       world::LoadedWorld& world, bool falling) {
    if (victim.dead || victim.gamemode == wire::protocol::play::Gamemode::Spectator) {
        return;
    }
    const wire::common::Identifier playerType = wire::common::Identifier::vanilla("player");
    if (!player::combat::withinAttackReach(session.x, session.y, session.z, victim.x, victim.y, victim.z,
                                           registry::entityTypeShapes::widthOf(playerType),
                                           registry::entityTypeShapes::heightOf(playerType))) {
        return;
    }
    world::RealEnchantments* enchantments = &vanillaData_.enchantments;
    const player::playerAttributes::DefinitionOf definitionOf = [enchantments](const wire::common::Identifier& id) {
        return enchantments->definitionOf(id);
    };
    const float strength = player::combat::attackStrengthScale(
        session.ticksSinceSwing, player::playerAttributes::valueOf(session, "minecraft:attack_speed", definitionOf));
    session.ticksSinceSwing = 0;
    const auto base =
        static_cast<float>(player::playerAttributes::valueOf(session, "minecraft:attack_damage", definitionOf));
    const player::BreakContext blocks = breakContext();
    static_cast<void>(player::pvp::attack(
        session, victim, world, meleeContext(blocks),
        player::pvp::Swing{.strength = strength,
                           .base = base,
                           .falling = player::pvp::criticalPossible(session, world, falling),
                           .weapon = session.inventory.heldItem(),
                           .weaponSlot = player::PlayerInventory::kHotbarStart + session.inventory.heldSlot(),
                           .slowEnoughToSweep = true}));
}

} // namespace basalt::server::runtime
