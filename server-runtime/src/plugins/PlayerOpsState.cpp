#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/PlayerOps.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace basalt::server::runtime::plugins::playerOps {
namespace {

constexpr double kMaxSpeed = 1.0;
constexpr std::int32_t kSimulationDistanceId = 0x71;
constexpr char kMovementSpeed[] = "minecraft:movement_speed";

void sendAbilities(player::PlayerSession& session) {
    if (!session.connection().sendPlayPacket) {
        return;
    }
    const wire::protocol::play::ClientboundPlayerAbilitiesPacket packet{.invulnerable = session.invulnerable(),
                                                                        .flying = session.flying,
                                                                        .mayFly = session.mayFly(),
                                                                        .instabuild = session.instabuild(),
                                                                        .flyingSpeed = session.extras.flySpeed,
                                                                        .walkingSpeed = session.extras.walkSpeed};
    session.connection().sendPlayPacket(wire::protocol::play::ClientboundPlayerAbilitiesPacket::kId,
                                        [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

void syncHealth(player::PlayerSession& session) {
    if (session.connection().syncHealth) {
        session.connection().syncHealth();
    }
}

void syncExperience(player::PlayerSession& session) {
    if (session.connection().syncExperience) {
        session.connection().syncExperience();
    }
}

} // namespace

std::optional<Value> state(Context& context, const Verb verb, const std::string_view name, const Args& args) {
    player::PlayerSession& session = context.session;
    player::PlayerExtras& extras = session.extras;
    const Value& first = arg(args, 0);
    if (name == "allow_flight") {
        if (verb == Verb::Get) {
            return session.mayFly();
        }
        extras.allowFlight = asBool(first);
        if (!session.mayFly()) {
            session.flying = false;
        }
        sendAbilities(session);
        return true;
    }
    if (name == "flying") {
        if (verb == Verb::Get) {
            return session.flying;
        }
        session.flying = asBool(first) && session.mayFly();
        sendAbilities(session);
        return session.flying == asBool(first);
    }
    if (name == "fly_speed") {
        if (verb == Verb::Get) {
            return static_cast<double>(extras.flySpeed) * 2.0;
        }
        extras.flySpeed = static_cast<float>(std::clamp(asReal(first), -kMaxSpeed, kMaxSpeed) / 2.0);
        sendAbilities(session);
        return true;
    }
    if (name == "walk_speed") {
        if (verb == Verb::Get) {
            return static_cast<double>(extras.walkSpeed) * 2.0;
        }
        extras.walkSpeed = static_cast<float>(std::clamp(asReal(first), -kMaxSpeed, kMaxSpeed) / 2.0);
        session.attributeBases.insert_or_assign(kMovementSpeed, static_cast<double>(extras.walkSpeed));
        sendAbilities(session);
        return true;
    }
    if (name == "sprinting") {
        if (verb == Verb::Get) {
            return session.sprinting;
        }
        session.sprinting = asBool(first);
        return true;
    }
    if (name == "sneaking") {
        if (verb == Verb::Get) {
            return session.sneaking;
        }
        session.sneaking = asBool(first);
        return true;
    }
    if (name == "swimming") {
        if (verb == Verb::Get) {
            return session.swimming;
        }
        session.swimming = asBool(first);
        return true;
    }
    if (name == "gliding") {
        if (verb == Verb::Get) {
            return session.fallFlying;
        }
        session.fallFlying = asBool(first);
        return true;
    }
    if (name == "exhaustion") {
        if (verb == Verb::Get) {
            return static_cast<double>(session.food.exhaustionLevel());
        }
        session.food.restore(session.food.foodLevel(), session.food.saturationLevel(),
                             static_cast<float>(std::max(0.0, asReal(first))));
        return true;
    }
    if (name == "saturation") {
        if (verb == Verb::Get) {
            return static_cast<double>(session.food.saturationLevel());
        }
        session.food.set(session.food.foodLevel(), std::clamp(static_cast<float>(asReal(first)), 0.0F,
                                                              static_cast<float>(session.food.foodLevel())));
        syncHealth(session);
        return true;
    }
    if (name == "exp") {
        if (verb == Verb::Get) {
            return static_cast<double>(session.experience.progress());
        }
        session.experience.restore(session.experience.level(),
                                   std::clamp(static_cast<float>(asReal(first)), 0.0F, 1.0F),
                                   session.experience.total());
        syncExperience(session);
        return true;
    }
    if (name == "total_experience") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.experience.total());
        }
        session.experience.reset();
        session.experience.givePoints(static_cast<std::int32_t>(std::max<std::int64_t>(0, asInt(first))));
        syncExperience(session);
        return true;
    }
    if (name == "exp_to_next_level") {
        return static_cast<std::int64_t>(session.experience.xpNeededForNextLevel());
    }
    if (name == "give_exp") {
        session.experience.givePoints(static_cast<std::int32_t>(asInt(first)));
        syncExperience(session);
        return true;
    }
    if (name == "give_levels") {
        session.experience.giveLevels(static_cast<std::int32_t>(asInt(first)));
        syncExperience(session);
        return true;
    }
    if (name == "health_scale") {
        if (verb == Verb::Get) {
            return extras.healthScale.has_value() ? Value{static_cast<double>(*extras.healthScale)} : Value{};
        }
        extras.healthScale = isNone(first) ? std::nullopt : std::optional{static_cast<float>(asReal(first, 20.0))};
        syncHealth(session);
        return true;
    }
    if (name == "send_health") {
        syncHealth(session);
        return true;
    }
    if (name == "send_experience") {
        syncExperience(session);
        return true;
    }
    if (name == "sleeping_ignored") {
        if (verb == Verb::Get) {
            return extras.sleepingIgnored;
        }
        extras.sleepingIgnored = asBool(first);
        return true;
    }
    if (name == "collidable") {
        if (verb == Verb::Get) {
            return extras.collidable;
        }
        extras.collidable = asBool(first, true);
        return true;
    }
    if (name == "affects_spawning") {
        if (verb == Verb::Get) {
            return extras.affectsSpawning;
        }
        extras.affectsSpawning = asBool(first, true);
        return true;
    }
    if (name == "flying_fall_damage") {
        if (verb == Verb::Get) {
            return extras.flyingFallDamage;
        }
        extras.flyingFallDamage = asBool(first);
        return true;
    }
    if (name == "exp_cooldown") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.expCooldown);
        }
        extras.expCooldown = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "view_distance") {
        if (verb == Verb::Get) {
            return extras.viewDistance.has_value() ? Value{static_cast<std::int64_t>(*extras.viewDistance)} : Value{};
        }
        if (!session.connection().overrideViewDistance) {
            return false;
        }
        extras.viewDistance =
            isNone(first) ? std::nullopt
                          : std::optional{static_cast<std::int32_t>(std::clamp<std::int64_t>(asInt(first), 2, 32))};
        session.connection().overrideViewDistance(extras.viewDistance);
        return true;
    }
    if (name == "simulation_distance") {
        if (verb == Verb::Get) {
            return extras.simulationDistance.has_value() ? Value{static_cast<std::int64_t>(*extras.simulationDistance)}
                                                         : Value{};
        }
        if (!session.connection().sendPlayPacket) {
            return false;
        }
        extras.simulationDistance =
            isNone(first) ? std::nullopt
                          : std::optional{static_cast<std::int32_t>(std::clamp<std::int64_t>(asInt(first), 2, 32))};
        const std::int32_t distance = extras.simulationDistance.value_or(10);
        session.connection().sendPlayPacket(kSimulationDistanceId, [distance](wire::io::ByteBuffer& buffer) {
            wire::protocol::varInt::write(buffer, distance);
        });
        return true;
    }
    if (name == "portal_cooldown") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.portalCooldown);
        }
        session.portalCooldown = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "warden_warning_level") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.wardenTracker.warningLevel);
        }
        session.wardenTracker.warningLevel = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "warden_warning_cooldown") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.wardenTracker.cooldownTicks);
        }
        session.wardenTracker.cooldownTicks = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "warden_time_since_warning") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.wardenTracker.ticksSinceLastWarning);
        }
        session.wardenTracker.ticksSinceLastWarning = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "input_flags") {
        return static_cast<std::int64_t>(extras.inputFlags);
    }
    if (name == "idle_ms") {
        const auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        return static_cast<std::int64_t>(now - extras.lastActionMillis);
    }
    if (name == "reset_idle") {
        extras.lastActionMillis =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        return true;
    }
    if (name == "using_item") {
        return session.usingItem;
    }
    if (name == "using_offhand") {
        return session.usingItem && session.usingOffhand;
    }
    if (name == "blocking") {
        return session.blockingSlot.has_value();
    }
    if (name == "sleep_ticks") {
        return static_cast<std::int64_t>(session.sleepTicks);
    }
    if (name == "seen_win_screen") {
        if (verb == Verb::Get) {
            return extras.seenWinScreen;
        }
        extras.seenWinScreen = asBool(first);
        return true;
    }
    if (name == "fishing_hook") {
        return static_cast<std::int64_t>(session.fishingHook.value_or(-1));
    }
    if (name == "enchantment_seed") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.enchantmentSeed);
        }
        session.enchantmentSeed = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "score") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.experience.score());
        }
        session.experience.setScore(static_cast<std::int32_t>(asInt(first)));
        return true;
    }
    if (name == "previous_gamemode") {
        constexpr std::array<std::string_view, 4> kNames{"survival", "creative", "adventure", "spectator"};
        const std::int32_t held = extras.previousGamemode;
        return held < 0 || held >= static_cast<std::int32_t>(kNames.size()) ? Value{}
                                                                            : Value{std::string{kNames[held]}};
    }
    if (name == "reset_attack_cooldown") {
        session.ticksSinceSwing = 0;
        return true;
    }
    if (name == "fake_experience") {
        if (!session.connection().sendPlayPacket) {
            return false;
        }
        const wire::protocol::play::SetExperiencePacket packet{.progress = static_cast<float>(asReal(first)),
                                                               .level = static_cast<std::int32_t>(asInt(arg(args, 1))),
                                                               .total = session.experience.total()};
        session.connection().sendPlayPacket(wire::protocol::play::SetExperiencePacket::kId,
                                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "fake_health") {
        if (!session.connection().sendPlayPacket) {
            return false;
        }
        const wire::protocol::play::SetHealthPacket packet{.health = static_cast<float>(asReal(first)),
                                                           .food = static_cast<std::int32_t>(asInt(arg(args, 1))),
                                                           .saturation = static_cast<float>(asReal(arg(args, 2)))};
        session.connection().sendPlayPacket(wire::protocol::play::SetHealthPacket::kId,
                                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "attack_cooldown") {
        const player::playerAttributes::DefinitionOf none = [](const wire::common::Identifier&) { return nullptr; };
        return static_cast<double>(player::combat::attackStrengthScale(
            session.ticksSinceSwing, player::playerAttributes::valueOf(session, "minecraft:attack_speed", none)));
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::playerOps
