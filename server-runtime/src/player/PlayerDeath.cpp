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
    // parity: `LivingEntity.die` empties the absorption pool, so respawning starts without it.
    session.absorption = 0.0F;
    session.stats.increment(storage::stats::deaths());
    // parity: `getKillCredit`, the player who hit them within the last hundred ticks.
    session.killerEntityId = session.lastHurtByPlayerTime.load() > 0 ? session.lastHurtByPlayerId : std::nullopt;
    session.deathsToScore.fetch_add(1);
    session.stats.reset(storage::stats::timeSinceRest());
    if (session.connection().syncHealth) {
        session.connection().syncHealth();
    }

    // parity: `DamageSource.getLocalizedDeathMessage`, whose second argument is whoever did it.
    // Half of vanilla's death messages name them, and one built without it reads as `%2$s`.
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
    // Through the connection's own way of saying something rather than a packet of Java's: for a
    // player on that edition it is the same call, and for one on Bedrock it is the only one there
    // is. Without this a death is announced to half the server.
    for (const PlayerSession* player : context.registry->snapshot()) {
        if (player->connection().sendSystemMessage) {
            player->connection().sendSystemMessage(message);
        }
    }
}

} // namespace basalt::server::runtime::player
