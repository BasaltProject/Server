#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/EffectCommand.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;
using wire::text::kGray;
using wire::text::kWhite;

constexpr std::int32_t kTicksPerSecond = 20;
constexpr std::int32_t kDefaultSeconds = 30;
constexpr std::int32_t kInfinite = -1;
constexpr std::int32_t kMaxSeconds = 1'000'000;
constexpr std::int32_t kMaxAmplifier = 255;
/// parity: `HealOrHarmMobEffect`, three hearts' worth doubled by each level.
constexpr float kInstantAmount = 6.0F;

/// The targets as players, refusing anything else by name rather than silently doing nothing.
[[nodiscard]] std::expected<std::vector<player::PlayerSession*>, CommandSyntaxError>
playersOf(const CommandContext& context) {
    std::vector<player::PlayerSession*> players;
    const auto* matched = context.get<std::vector<CommandTarget>>("targets");
    if (matched == nullptr) {
        return players;
    }
    for (const CommandTarget& target : *matched) {
        player::PlayerSession* session = target.player();
        if (session == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component =
                    Component::text("Only a player carries effects in this build; " + target.name() + " does not.")}};
        }
        players.push_back(session);
    }
    return players;
}

/// The name a report uses: the one target, or how many there were.
[[nodiscard]] Component whoOf(const std::vector<player::PlayerSession*>& targets) {
    return targets.size() == 1 ? Component::text(targets.front()->name())
                               : Component::text(std::to_string(targets.size()));
}

[[nodiscard]] Parsed<std::int32_t> give(const CommandContext& context, std::int32_t durationTicks,
                                        std::int32_t amplifier, bool visible) {
    const auto* effect = context.get<Identifier>("effect");
    if (effect == nullptr || !world::mobEffects::protocolIdOf(*effect).has_value()) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable(
                "effect.effectNotFound", {Component::text(effect == nullptr ? std::string{} : effect->toString())})}};
    }
    const std::expected<std::vector<player::PlayerSession*>, CommandSyntaxError> targets = playersOf(context);
    if (!targets.has_value()) {
        return std::unexpected{targets.error()};
    }
    for (player::PlayerSession* session : *targets) {
        // parity: the two instantaneous effects act once and are never carried.
        if (*effect == world::mobEffects::instantHealth()) {
            if (session->connection().heal) {
                session->connection().heal(kInstantAmount * static_cast<float>(1 << amplifier));
            }
            continue;
        }
        if (*effect == world::mobEffects::instantDamage()) {
            if (session->connection().hurt) {
                session->connection().hurt(kInstantAmount * static_cast<float>(1 << amplifier), "magic",
                                           world::damageTypes::kMagic, std::nullopt);
            }
            continue;
        }
        const world::ActiveEffect active{.effect = *effect,
                                         .amplifier = amplifier,
                                         .remainingTicks = durationTicks,
                                         .visible = visible,
                                         .showIcon = visible};
        session->effects.insert_or_assign(*effect, active);
        if (session->connection().sendEffect) {
            session->connection().sendEffect(active);
        }
    }
    context.success(Component::translatable("commands.effect.give.success.single",
                                            {Component::text(effect->path()), whoOf(*targets)}),
                    true);
    return static_cast<std::int32_t>(targets->size());
}

[[nodiscard]] Parsed<std::int32_t> clear(const CommandContext& context,
                                         const std::vector<player::PlayerSession*>& targets, const Identifier* effect) {
    std::int32_t cleared = 0;
    for (player::PlayerSession* session : targets) {
        std::vector<Identifier> removed;
        if (effect == nullptr) {
            for (const auto& [id, active] : session->effects) {
                removed.push_back(id);
            }
        } else if (session->effects.contains(*effect)) {
            removed.push_back(*effect);
        }
        for (const Identifier& id : removed) {
            session->effects.erase(id);
            if (session->connection().sendEffectRemoved) {
                session->connection().sendEffectRemoved(id);
            }
            cleared++;
        }
    }
    context.success(Component::translatable("commands.effect.clear.everything.success.single", {whoOf(targets)}), true);
    return cleared;
}

} // namespace

EffectCommand::EffectCommand() : Command("effect", permissionLevel::kGamemasters) {}

void EffectCommand::build(const CommandNodeBuilder& root) const {
    root.literal("clear", [](const CommandNodeBuilder& form) {
        form.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            player::PlayerSession* self = context.source().player();
            return clear(context,
                         self == nullptr ? std::vector<player::PlayerSession*>{}
                                         : std::vector<player::PlayerSession*>{self},
                         nullptr);
        });
        form.argument("targets", entitiesArgument(), [](const CommandNodeBuilder& targets) {
            targets.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto players = playersOf(context);
                return players.has_value() ? clear(context, *players, nullptr)
                                           : Parsed<std::int32_t>{std::unexpected{players.error()}};
            });
            targets.argument("effect", resourceArgument("minecraft:mob_effect"), [](const CommandNodeBuilder& effect) {
                effect.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto players = playersOf(context);
                    if (!players.has_value()) {
                        return std::unexpected{players.error()};
                    }
                    return clear(context, *players, context.get<Identifier>("effect"));
                });
            });
        });
    });

    /// The two duration forms below hang the same three depths off whichever length they name.
    const auto lengths = [](const CommandNodeBuilder& node,
                            const std::function<std::int32_t(const CommandContext&)>& ticks) {
        node.executes([ticks](const CommandContext& context) { return give(context, ticks(context), 0, true); });
        node.argument("amplifier", integerArgument(0, kMaxAmplifier), [ticks](const CommandNodeBuilder& amplifier) {
            amplifier.executes([ticks](const CommandContext& context) {
                const auto* level = context.get<std::int32_t>("amplifier");
                return give(context, ticks(context), level == nullptr ? 0 : *level, true);
            });
            amplifier.argument("hideParticles", boolArgument(), [ticks](const CommandNodeBuilder& hidden) {
                hidden.executes([ticks](const CommandContext& context) {
                    const auto* level = context.get<std::int32_t>("amplifier");
                    const auto* hide = context.get<bool>("hideParticles");
                    return give(context, ticks(context), level == nullptr ? 0 : *level, hide == nullptr || !*hide);
                });
            });
        });
    };

    root.literal("give", [lengths](const CommandNodeBuilder& form) {
        form.argument("targets", entitiesArgument(), [lengths](const CommandNodeBuilder& targets) {
            targets.argument("effect", resourceArgument("minecraft:mob_effect"),
                             [lengths](const CommandNodeBuilder& effect) {
                                 effect.executes([](const CommandContext& context) {
                                     return give(context, kDefaultSeconds * kTicksPerSecond, 0, true);
                                 });
                                 effect.literal("infinite", [lengths](const CommandNodeBuilder& infinite) {
                                     lengths(infinite, [](const CommandContext&) { return kInfinite; });
                                 });
                                 effect.argument("seconds", integerArgument(0, kMaxSeconds),
                                                 [lengths](const CommandNodeBuilder& seconds) {
                                                     lengths(seconds, [](const CommandContext& context) {
                                                         const auto* value = context.get<std::int32_t>("seconds");
                                                         return value == nullptr ? 0 : *value * kTicksPerSecond;
                                                     });
                                                 });
                             });
        });
    });
}

WorldCommand::WorldCommand() : Command("world", permissionLevel::kGamemasters) {}

namespace {

[[nodiscard]] std::int32_t playersIn(const CommandContext& context, const world::LoadedWorld& world) {
    std::int32_t counted = 0;
    for (const player::PlayerSession* player : context.environment().players().snapshot()) {
        if (player->worldName == world.name()) {
            counted++;
        }
    }
    return counted;
}

[[nodiscard]] std::int32_t listWorlds(const CommandContext& context) {
    std::vector<world::LoadedWorld*> worlds;
    for (const auto& [name, world] : context.environment().options().worlds) {
        worlds.push_back(world);
    }
    std::ranges::sort(worlds, {}, [](const world::LoadedWorld* world) { return world->name(); });
    const player::PlayerSession* runner = context.source().player();
    context.source().sendSystemMessage(Component::text(std::to_string(worlds.size()) + " world(s) loaded:", kGray));
    for (const world::LoadedWorld* world : worlds) {
        const std::string marker = runner != nullptr && runner->worldName == world->name() ? " <-- you are here" : "";
        context.source().sendSystemMessage(
            Component::text("  " + world->name(), kWhite)
                .append(Component::text("  " + std::string{config::worldTypeName(world->declaration().type)} + "  " +
                                            std::to_string(playersIn(context, *world)) + " player(s)" + marker,
                                        kGray)));
    }
    return static_cast<std::int32_t>(worlds.size());
}

/// Puts the targets in the world, at its spawn.
///
/// At the spawn rather than at the same coordinates: the two worlds share nothing, and the same x/z
/// in another world is as likely to be inside a mountain as anywhere else. `/tp` is still there for
/// landing somewhere exact.
[[nodiscard]] Parsed<std::int32_t>
sendTo(const CommandContext& context, const std::vector<player::PlayerSession*>& targets, world::LoadedWorld& world) {
    const std::array<double, 3>& spawn = world.spawn();
    std::int32_t moved = 0;
    for (player::PlayerSession* target : targets) {
        if (target->worldName == world.name()) {
            if (targets.size() == 1) {
                return std::unexpected{CommandSyntaxError{
                    .component = Component::text(target->name() + " is already in " + world.name() + ".")}};
            }
            continue;
        }
        if (target->connection().switchWorld && target->connection().switchWorld(world, spawn[0], spawn[1], spawn[2])) {
            moved++;
        }
    }
    if (moved == 0) {
        return std::unexpected{CommandSyntaxError{.component = Component::text("Nobody moved.")}};
    }
    std::string who = std::to_string(moved) + " players";
    if (moved == 1) {
        for (const player::PlayerSession* target : targets) {
            if (target->worldName == world.name()) {
                who = target->name();
                break;
            }
        }
    }
    context.success(Component::text("Moved " + who + " to " + world.name() + "."), true);
    return moved;
}

} // namespace

void WorldCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) -> Parsed<std::int32_t> { return listWorlds(context); });
    root.literal("list", [](const CommandNodeBuilder& list) {
        list.executes([](const CommandContext& context) -> Parsed<std::int32_t> { return listWorlds(context); });
    });
    root.literal("info", [](const CommandNodeBuilder& info) {
        info.argument("world", worldArgument(), [](const CommandNodeBuilder& named) {
            named.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                auto* const* found = context.get<world::LoadedWorld*>("world");
                if (found == nullptr || *found == nullptr) {
                    return 0;
                }
                const world::LoadedWorld& world = **found;
                const std::array<double, 3>& spawn = world.spawn();
                const std::vector<std::string> lines{
                    "world: " + world.name(), "type: " + std::string{config::worldTypeName(world.declaration().type)},
                    "dimension: " + std::string{config::dimensionName(world.declaration().dimension)},
                    // Which set of dimensions this world belongs to, which is what decides where its
                    // portals come out. Blank for an overworld, which is a set of its own.
                    "linked to: " + world.declaration().linkedTo.value_or("-"),
                    "seed: " + std::to_string(world.metadata().seed),
                    "spawn: " + std::to_string(spawn[0]) + ", " + std::to_string(spawn[1]) + ", " +
                        std::to_string(spawn[2]),
                    "chunks loaded: " + std::to_string(world.state().loadedChunkCount()),
                    "players: " + std::to_string(playersIn(context, world))};
                for (const std::string& line : lines) {
                    context.source().sendSystemMessage(Component::text(line, kGray));
                }
                return 1;
            });
        });
    });
    // `/world <name>` sends whoever ran it, which is the form anyone actually types.
    root.argument("world", worldArgument(), [](const CommandNodeBuilder& named) {
        named.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<player::PlayerSession*> self = context.requirePlayer();
            if (!self.has_value()) {
                return std::unexpected{self.error()};
            }
            auto* const* found = context.get<world::LoadedWorld*>("world");
            return found == nullptr || *found == nullptr ? 0 : sendTo(context, {*self}, **found);
        });
        named.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
            targets.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* matched = context.get<std::vector<player::PlayerSession*>>("targets");
                auto* const* found = context.get<world::LoadedWorld*>("world");
                if (matched == nullptr || found == nullptr || *found == nullptr) {
                    return 0;
                }
                return sendTo(context, *matched, **found);
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
