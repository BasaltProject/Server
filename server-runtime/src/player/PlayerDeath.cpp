#include <basalt/server/runtime/player/PlayerDeath.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <vector>

namespace basalt::server::runtime::player {

using wire::text::Component;

void killPlayer(PlayerSession& session, const std::string_view deathMessageId, const std::optional<Component>& killer,
                const DeathContext& context) {
    if (session.dead) {
        return;
    }
    session.dead = true;
    session.setHealth(0.0F);
    // parity: `ServerPlayer.die` puts the shoulder parrots back in the world.
    session.shoulderDropPending = true;
    // parity: `LivingEntity.die` empties the absorption pool.
    session.absorption = 0.0F;
    session.stats.increment(storage::stats::deaths());
    // parity: `getKillCredit`.
    session.killerEntityId = session.lastHurtByPlayerTime.load() > 0 ? session.lastHurtByPlayerId : std::nullopt;
    if (session.killerEntityId.has_value()) {
        session.stats.increment(storage::stats::killedBy("minecraft:player"));
    } else if (session.lastHurtByMobType.has_value() && session.lastHurtByMobTime.load() > 0) {
        session.stats.increment(storage::stats::killedBy(session.lastHurtByMobType->toString()));
    }
    session.deathsToScore.fetch_add(1);
    session.stats.reset(storage::stats::custom("time_since_death"));
    session.stats.reset(storage::stats::timeSinceRest());
    if (session.connection().syncHealth) {
        session.connection().syncHealth();
    }

    // parity: `DamageSource.getLocalizedDeathMessage`.
    std::vector<Component> arguments{Component::text(session.name())};
    if (killer.has_value()) {
        arguments.push_back(*killer);
    }
    const Component message = Component::translatable("death.attack." + std::string{deathMessageId}, arguments);
    if (context.died) {
        context.died(session, wire::text::resolvePlainText(message, context.lang));
    }
    if (session.connection().sendCombatDeath) {
        session.connection().sendCombatDeath(message);
    }
    context.logger.info([&] { return wire::text::resolvePlainText(message, context.lang); });

    if (context.registry == nullptr) {
        return;
    }
    for (const PlayerSession* player : context.registry->snapshot()) {
        if (player->connection().sendSystemMessage) {
            player->connection().sendSystemMessage(message);
        }
    }
}

} // namespace basalt::server::runtime::player
