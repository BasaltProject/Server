#include <basalt/server/runtime/player/PlayerCombatMessage.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/text/ClickEvent.hpp>
#include <basalt/wire/text/HoverEvent.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace basalt::server::runtime::player {

namespace {

using Record = PlayerSession::CombatRecord;
using wire::text::Component;

constexpr float kSignificantFall = 5.0F;

[[nodiscard]] bool isFall(std::string_view type) {
    return type == "minecraft:fall" || type == "minecraft:ender_pearl" || type == "minecraft:stalagmite";
}

[[nodiscard]] bool alwaysFall(std::string_view type) {
    return type == "minecraft:outside_border" || type == "minecraft:out_of_world";
}

[[nodiscard]] bool fallVariants(std::string_view type) {
    return type == "minecraft:fall" || type == "minecraft:ender_pearl";
}

/// parity: `CombatTracker.getMostSignificantFall`.
[[nodiscard]] const Record* mostSignificantFall(const std::vector<Record>& records) {
    const Record* result = nullptr;
    const Record* alternative = nullptr;
    float altDamage = 0.0F;
    float bestFall = 0.0F;
    for (std::size_t at = 0; at < records.size(); ++at) {
        const Record& entry = records[at];
        const bool fake = alwaysFall(entry.damageType);
        const float fall = fake ? std::numeric_limits<float>::max() : entry.fallDistance;
        if ((isFall(entry.damageType) || fake) && fall > 0.0F && (result == nullptr || fall > bestFall)) {
            result = at > 0 ? &records[at - 1] : &entry;
            bestFall = fall;
        }
        if (!entry.fallLocation.empty() && (alternative == nullptr || entry.damage > altDamage)) {
            alternative = &entry;
            altDamage = entry.damage;
        }
    }
    if (bestFall > kSignificantFall && result != nullptr) {
        return result;
    }
    return altDamage > kSignificantFall && alternative != nullptr ? alternative : nullptr;
}

[[nodiscard]] std::optional<DeathLabel> labelFor(const DeathContext& context, std::int32_t id) {
    return id < 0 || !context.labelOf ? std::nullopt : context.labelOf(id);
}

/// parity: `ItemStack.getDisplayName` of a stack that has a custom name.
[[nodiscard]] Component itemName(const Component& held) {
    return Component::text("[").append(held).append(Component::text("]"));
}

[[nodiscard]] Component assistedFall(const Component& victim, const std::optional<DeathLabel>& attacker,
                                     const Component& attackerName, std::string_view withItem,
                                     std::string_view without) {
    if (attacker.has_value() && attacker->heldItem.has_value()) {
        return Component::translatable(std::string{withItem}, {victim, attackerName, itemName(*attacker->heldItem)});
    }
    return Component::translatable(std::string{without}, {victim, attackerName});
}

[[nodiscard]] Component fallMessage(const Component& victim, const Record& knockOff, const Record& killing,
                                    const DeathContext& context) {
    if (!isFall(knockOff.damageType) && !alwaysFall(knockOff.damageType)) {
        const std::optional<DeathLabel> killer = labelFor(context, killing.causeId);
        const std::optional<DeathLabel> attacker = labelFor(context, knockOff.causeId);
        if (attacker.has_value() && (!killer.has_value() || !(attacker->name == killer->name))) {
            return assistedFall(victim, attacker, attacker->name, "death.fell.assist.item", "death.fell.assist");
        }
        if (killer.has_value()) {
            return assistedFall(victim, killer, killer->name, "death.fell.finish.item", "death.fell.finish");
        }
        return Component::translatable("death.fell.killer", {victim});
    }
    const std::string location = knockOff.fallLocation.empty() ? "generic" : knockOff.fallLocation;
    return Component::translatable("death.fell.accident." + location, {victim});
}

} // namespace

Component trackedDeathMessage(const PlayerSession& session, const std::string_view deathMessageId,
                              const std::optional<entity::DamageSource>& source, const DeathContext& context) {
    const Component victim = Component::text(session.name());
    const std::vector<Record>& records = session.combatRecords;
    const std::string id = "death.attack." + std::string{deathMessageId};
    if (records.empty()) {
        return Component::translatable("death.attack.generic", {victim});
    }
    const Record& killing = records.back();
    if (fallVariants(killing.damageType)) {
        if (const Record* knockOff = mostSignificantFall(records); knockOff != nullptr) {
            return fallMessage(victim, *knockOff, killing, context);
        }
    }
    if (killing.damageType == "minecraft:bad_respawn_point") {
        const Component link =
            Component::text("[")
                .append(Component::translatable(id + ".link"))
                .append(Component::text("]"))
                .clickEvent(wire::text::ClickEvent::openUrl("https://bugs.mojang.com/browse/MCPE-28723"))
                .hoverEvent(wire::text::HoverEvent::showText(Component::text("MCPE-28723")));
        return Component::translatable(id + ".message", {victim, link});
    }
    const std::int32_t named = killing.causeId >= 0 ? killing.causeId : killing.directId;
    if (killing.causeId < 0 && killing.directId < 0) {
        // parity: `getKillCredit` names who last hurt the player when nothing in the hit does.
        if (session.lastHurtByPlayerTime.load() > 0 && session.lastHurtByPlayerId.has_value()) {
            if (const std::optional<DeathLabel> credit = labelFor(context, *session.lastHurtByPlayerId);
                credit.has_value()) {
                return Component::translatable(id + ".player", {victim, credit->name});
            }
        } else if (session.lastHurtByMobType.has_value() && session.lastHurtByMobTime.load() > 0) {
            return Component::translatable(
                id + ".player",
                {victim, Component::translatable("entity.minecraft." + session.lastHurtByMobType->path())});
        }
        return Component::translatable(id, {victim});
    }
    std::optional<DeathLabel> label = labelFor(context, named);
    if (!label.has_value() && source.has_value() && source->name.has_value()) {
        label = DeathLabel{.name = *source->name, .heldItem = std::nullopt};
    }
    if (!label.has_value()) {
        return Component::translatable(id, {victim});
    }
    const std::optional<DeathLabel> holder = labelFor(context, killing.causeId);
    if (holder.has_value() && holder->heldItem.has_value()) {
        return Component::translatable(id + ".item", {victim, label->name, itemName(*holder->heldItem)});
    }
    return Component::translatable(id, {victim, label->name});
}

} // namespace basalt::server::runtime::player
