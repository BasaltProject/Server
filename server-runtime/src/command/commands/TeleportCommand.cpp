#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/commands/TeleportCommand.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using wire::common::Identifier;
using wire::text::Component;

constexpr double kDegreesPerRadian = 57.2957795130823;
constexpr double kEyeHeight = 1.62;
/// parity: how far a sound of volume 1 carries before vanilla considers it out of earshot.
constexpr double kAudibleBlocksPerVolume = 16.0;

[[nodiscard]] const std::vector<player::PlayerSession*>& playersOf(const CommandContext& context,
                                                                   std::string_view name) {
    static const std::vector<player::PlayerSession*> none;
    const auto* found = context.get<std::vector<player::PlayerSession*>>(name);
    return found == nullptr ? none : *found;
}

/// parity: `TeleportCommand.formatDouble`, which is `String.format("%f")`: always six decimals.
[[nodiscard]] std::string formatCoordinate(double value) {
    const auto scaled = static_cast<std::int64_t>(std::llround(std::abs(value) * 1'000'000.0));
    const std::string fraction = std::to_string(scaled % 1'000'000);
    return (value < 0 ? "-" : "") + std::to_string(scaled / 1'000'000) + "." + std::string(6 - fraction.size(), '0') +
           fraction;
}

/// parity: `Entity.lookAt`: yaw clockwise from +Z, pitch positive downwards.
[[nodiscard]] std::pair<float, float> anglesFrom(double x, double y, double z, const std::array<double, 3>& target) {
    const double dx = target[0] - x;
    const double dy = target[1] - (y + kEyeHeight);
    const double dz = target[2] - z;
    return {static_cast<float>(-std::atan2(dx, dz) * kDegreesPerRadian),
            static_cast<float>(-std::atan2(dy, std::sqrt((dx * dx) + (dz * dz))) * kDegreesPerRadian)};
}

[[nodiscard]] Parsed<std::int32_t>
toPosition(const CommandContext& context, const std::vector<player::PlayerSession*>& targets, world::LoadedWorld* world,
           const std::optional<std::pair<float, float>>& facing, const std::optional<std::array<double, 3>>& lookAt) {
    const auto* coordinates = context.get<Coordinates>("location");
    if (coordinates == nullptr) {
        return 0;
    }
    // Resolved per target, because `~` means "relative to the entity being moved", not to whoever
    // typed the command. The reported position uses the source's own frame, which is what vanilla
    // prints.
    for (player::PlayerSession* target : targets) {
        const std::array<double, 3> at = coordinates->resolve({target->x, target->y, target->z});
        const std::pair<float, float> angles = facing.has_value()   ? *facing
                                               : lookAt.has_value() ? anglesFrom(at[0], at[1], at[2], *lookAt)
                                                                    : std::pair{target->yaw, target->pitch};
        // The world change first: it re-sends the position itself, so teleporting afterwards would
        // be a second, redundant move.
        const bool moved = world != nullptr && target->connection().switchWorld &&
                           target->connection().switchWorld(*world, at[0], at[1], at[2]);
        if (!moved && target->connection().teleportTo) {
            target->connection().teleportTo(at[0], at[1], at[2], angles.first, angles.second);
        }
    }
    const std::array<double, 3> reported = coordinates->resolve(context.source().position());
    const Component who = targets.size() == 1 ? Component::text(targets.front()->name())
                                              : Component::text(std::to_string(targets.size()));
    const Component message = Component::translatable(
        targets.size() == 1 ? "commands.teleport.success.location.single"
                            : "commands.teleport.success.location.multiple",
        {who, Component::text(formatCoordinate(reported[0])), Component::text(formatCoordinate(reported[1])),
         Component::text(formatCoordinate(reported[2]))});
    context.success(world == nullptr
                        ? message
                        : Component::text("").append(message).append(Component::text(" in " + world->name())),
                    true);
    return static_cast<std::int32_t>(targets.size());
}

[[nodiscard]] Parsed<std::int32_t> toPlayer(const CommandContext& context,
                                            const std::vector<player::PlayerSession*>& targets,
                                            player::PlayerSession& destination) {
    for (player::PlayerSession* target : targets) {
        // parity: `TeleportCommand` moves to the entity, and an entity in another dimension takes
        // you to that dimension. Teleporting the coordinates alone dropped you at their numbers in
        // your own world, next to nobody.
        if (target->worldName != destination.worldName) {
            const auto found = context.environment().options().worlds.find(destination.worldName);
            if (found != context.environment().options().worlds.end() && target->connection().switchWorld) {
                static_cast<void>(
                    target->connection().switchWorld(*found->second, destination.x, destination.y, destination.z));
            }
        }
        if (target->connection().teleportTo) {
            target->connection().teleportTo(destination.x, destination.y, destination.z, destination.yaw,
                                            destination.pitch);
        }
    }
    const Component who = targets.size() == 1 ? Component::text(targets.front()->name())
                                              : Component::text(std::to_string(targets.size()));
    context.success(Component::translatable(targets.size() == 1 ? "commands.teleport.success.entity.single"
                                                                : "commands.teleport.success.entity.multiple",
                                            {who, Component::text(destination.name())}),
                    true);
    return static_cast<std::int32_t>(targets.size());
}

/// The one player a `single = true` selector matched, or null when it matched nobody.
[[nodiscard]] player::PlayerSession* only(const CommandContext& context, std::string_view name) {
    const std::vector<player::PlayerSession*>& matched = playersOf(context, name);
    return matched.size() == 1 ? matched.front() : nullptr;
}

} // namespace

TeleportCommand::TeleportCommand() : Command("teleport", permissionLevel::kGamemasters, {"tp"}) {}

void TeleportCommand::build(const CommandNodeBuilder& root) const {
    /// The tail `<targets> <location>` shares with the bare `<location>` form.
    const auto destinations = [](const CommandNodeBuilder& node, bool named) {
        node.executes([named](const CommandContext& context) -> Parsed<std::int32_t> {
            if (named) {
                return toPosition(context, playersOf(context, "targets"), nullptr, std::nullopt, std::nullopt);
            }
            const Parsed<player::PlayerSession*> self = context.requireEntity();
            if (!self.has_value()) {
                return std::unexpected{self.error()};
            }
            return toPosition(context, {*self}, nullptr, std::nullopt, std::nullopt);
        });
        node.argument("world", worldArgument(), [named](const CommandNodeBuilder& world) {
            world.executes([named](const CommandContext& context) -> Parsed<std::int32_t> {
                auto* const* found = context.get<world::LoadedWorld*>("world");
                world::LoadedWorld* into = found == nullptr ? nullptr : *found;
                if (named) {
                    return toPosition(context, playersOf(context, "targets"), into, std::nullopt, std::nullopt);
                }
                const Parsed<player::PlayerSession*> self = context.requireEntity();
                if (!self.has_value()) {
                    return std::unexpected{self.error()};
                }
                return toPosition(context, {*self}, into, std::nullopt, std::nullopt);
            });
        });
    };

    root.argument("location", vec3Argument(),
                  [destinations](const CommandNodeBuilder& location) { destinations(location, false); });
    root.argument("destination", playersArgument(true), [](const CommandNodeBuilder& destination) {
        destination.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<player::PlayerSession*> self = context.requireEntity();
            if (!self.has_value()) {
                return std::unexpected{self.error()};
            }
            player::PlayerSession* to = only(context, "destination");
            return to == nullptr ? 0 : toPlayer(context, {*self}, *to);
        });
    });
    root.argument("targets", playersArgument(false), [destinations](const CommandNodeBuilder& targets) {
        targets.argument("location", vec3Argument(), [destinations](const CommandNodeBuilder& location) {
            destinations(location, true);
            location.argument("rotation", rotationArgument(), [](const CommandNodeBuilder& rotation) {
                rotation.executes([](const CommandContext& context) {
                    const auto* facing = context.get<std::pair<float, float>>("rotation");
                    return toPosition(context, playersOf(context, "targets"), nullptr,
                                      facing == nullptr ? std::nullopt : std::optional{*facing}, std::nullopt);
                });
            });
            location.literal("facing", [](const CommandNodeBuilder& facing) {
                /// parity: `teleport ... facing <pos>`, which turns each target towards a point.
                facing.argument("facingLocation", vec3Argument(), [](const CommandNodeBuilder& at) {
                    at.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                        const auto* target = context.get<Coordinates>("facingLocation");
                        if (target == nullptr) {
                            return 0;
                        }
                        return toPosition(context, playersOf(context, "targets"), nullptr, std::nullopt,
                                          target->resolve(context.source().position()));
                    });
                });
                /// parity: `teleport ... facing entity <entity> [<anchor>]`, aiming at its feet or
                /// its eyes.
                facing.literal("entity", [](const CommandNodeBuilder& entity) {
                    const auto aim = [](const CommandContext& context, bool eyes) -> Parsed<std::int32_t> {
                        const auto* matched = context.get<std::vector<CommandTarget>>("facingEntity");
                        if (matched == nullptr || matched->size() != 1) {
                            return 0;
                        }
                        const CommandTarget& at = matched->front();
                        return toPosition(context, playersOf(context, "targets"), nullptr, std::nullopt,
                                          std::array<double, 3>{at.x(), at.y() + (eyes ? kEyeHeight : 0.0), at.z()});
                    };
                    entity.argument("facingEntity", entitiesArgument(true), [aim](const CommandNodeBuilder& named) {
                        named.executes([aim](const CommandContext& context) { return aim(context, false); });
                        named.argument("facingAnchor", entityAnchorArgument(), [aim](const CommandNodeBuilder& anchor) {
                            anchor.executes([aim](const CommandContext& context) {
                                const auto* which = context.get<EntityAnchor>("facingAnchor");
                                return aim(context, which != nullptr && *which == EntityAnchor::Eyes);
                            });
                        });
                    });
                });
            });
        });
        targets.argument("destination", playersArgument(true), [](const CommandNodeBuilder& destination) {
            destination.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                player::PlayerSession* to = only(context, "destination");
                return to == nullptr ? 0 : toPlayer(context, playersOf(context, "targets"), *to);
            });
        });
    });
}

PlaySoundCommand::PlaySoundCommand(std::function<std::int64_t()> nextSeed)
    : Command("playsound", permissionLevel::kGamemasters), nextSeed_(std::move(nextSeed)) {}

void PlaySoundCommand::build(const CommandNodeBuilder& root) const {
    const auto play = [this](const CommandContext& context, play::SoundSource source, bool positioned, float volume,
                             float pitch, float minVolume) -> Parsed<std::int32_t> {
        const auto* sound = context.get<Identifier>("sound");
        if (sound == nullptr) {
            return 0;
        }
        const std::vector<player::PlayerSession*>& targets = playersOf(context, "targets");
        const auto* coordinates = positioned ? context.get<Coordinates>("pos") : nullptr;
        const std::array<double, 3> origin =
            coordinates == nullptr ? context.source().position() : coordinates->resolve(context.source().position());
        // parity: the sound carries 16 blocks per unit of volume, and beyond that vanilla moves it
        // to just within earshot rather than dropping it, but only if a minimum volume was given.
        const double range = static_cast<double>(volume) * kAudibleBlocksPerVolume;
        std::int32_t played = 0;
        for (const player::PlayerSession* target : targets) {
            const double dx = origin[0] - target->x;
            const double dy = origin[1] - target->y;
            const double dz = origin[2] - target->z;
            const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
            std::array<double, 3> at = origin;
            float effective = volume;
            if (distance > range) {
                if (minVolume <= 0.0F) {
                    continue;
                }
                const double scale = range / distance;
                at = {target->x + (dx * scale), target->y + (dy * scale), target->z + (dz * scale)};
                effective = minVolume;
            }
            const play::SoundPacket packet{.sound = sound->toString(),
                                           .source = source,
                                           .x = at[0],
                                           .y = at[1],
                                           .z = at[2],
                                           .volume = effective,
                                           .pitch = pitch,
                                           .seed = nextSeed_ ? nextSeed_() : 0};
            if (target->connection().sendPlayPacket) {
                target->connection().sendPlayPacket(play::SoundPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
            played++;
        }
        if (played == 0) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.playsound.failed")}};
        }
        context.success(played == 1 ? Component::translatable("commands.playsound.success.single",
                                                              {Component::text(sound->toString()),
                                                               Component::text(targets.front()->name())})
                                    : Component::translatable("commands.playsound.success.multiple",
                                                              {Component::text(sound->toString()),
                                                               Component::text(std::to_string(played))}),
                        true);
        return played;
    };

    root.argument("sound", resourceLocationArgument(), [play](const CommandNodeBuilder& sound) {
        sound.suggestsFromClient(std::string{suggestionProviders::kAvailableSounds});
        for (std::uint8_t ordinal = 0; ordinal <= static_cast<std::uint8_t>(play::SoundSource::Ui); ordinal++) {
            const auto source = static_cast<play::SoundSource>(ordinal);
            sound.literal(std::string{play::soundSourceCommandName(source)}, [play,
                                                                              source](const CommandNodeBuilder& mixer) {
                mixer.argument("targets", playersArgument(false), [play, source](const CommandNodeBuilder& targets) {
                    targets.executes([play, source](const CommandContext& context) {
                        return play(context, source, false, 1.0F, 1.0F, 0.0F);
                    });
                    targets.argument("pos", vec3Argument(), [play, source](const CommandNodeBuilder& pos) {
                        pos.executes([play, source](const CommandContext& context) {
                            return play(context, source, true, 1.0F, 1.0F, 0.0F);
                        });
                        pos.argument("volume", floatArgument(0.0F), [play, source](const CommandNodeBuilder& volume) {
                            const auto read = [](const CommandContext& context, std::string_view name, float fallback) {
                                const auto* value = context.get<float>(name);
                                return value == nullptr ? fallback : *value;
                            };
                            volume.executes([play, source, read](const CommandContext& context) {
                                return play(context, source, true, read(context, "volume", 1.0F), 1.0F, 0.0F);
                            });
                            volume.argument("pitch", floatArgument(0.0F, 2.0F),
                                            [play, source, read](const CommandNodeBuilder& pitch) {
                                                pitch.executes([play, source, read](const CommandContext& context) {
                                                    return play(context, source, true, read(context, "volume", 1.0F),
                                                                read(context, "pitch", 1.0F), 0.0F);
                                                });
                                                pitch.argument("minVolume", floatArgument(0.0F, 1.0F),
                                                               [play, source, read](const CommandNodeBuilder& minimum) {
                                                                   minimum.executes([play, source, read](
                                                                                        const CommandContext& context) {
                                                                       return play(context, source, true,
                                                                                   read(context, "volume", 1.0F),
                                                                                   read(context, "pitch", 1.0F),
                                                                                   read(context, "minVolume", 0.0F));
                                                                   });
                                                               });
                                            });
                        });
                    });
                });
            });
        }
    });
}

StopSoundCommand::StopSoundCommand() : Command("stopsound", permissionLevel::kGamemasters) {}

void StopSoundCommand::build(const CommandNodeBuilder& root) const {
    const auto stop = [](const CommandContext& context, std::optional<play::SoundSource> source,
                         bool named) -> Parsed<std::int32_t> {
        const std::vector<player::PlayerSession*>& targets = playersOf(context, "targets");
        const auto* sound = named ? context.get<Identifier>("sound") : nullptr;
        const play::StopSoundPacket packet{.sound = sound == nullptr ? std::nullopt : std::optional{sound->toString()},
                                           .source = source};
        for (const player::PlayerSession* target : targets) {
            if (target->connection().sendPlayPacket) {
                target->connection().sendPlayPacket(play::StopSoundPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        const Component named_ = sound == nullptr ? Component{} : Component::text(sound->toString());
        const Component mixer =
            source.has_value() ? Component::text(std::string{play::soundSourceCommandName(*source)}) : Component{};
        if (source.has_value() && sound != nullptr) {
            context.success(Component::translatable("commands.stopsound.success.source.sound", {named_, mixer}), true);
        } else if (source.has_value()) {
            context.success(Component::translatable("commands.stopsound.success.source.any", {mixer}), true);
        } else if (sound != nullptr) {
            context.success(Component::translatable("commands.stopsound.success.sourceless.sound", {named_}), true);
        } else {
            context.success(Component::translatable("commands.stopsound.success.sourceless.any"), true);
        }
        return static_cast<std::int32_t>(targets.size());
    };

    root.argument("targets", playersArgument(false), [stop](const CommandNodeBuilder& targets) {
        targets.executes([stop](const CommandContext& context) { return stop(context, std::nullopt, false); });
        targets.literal("*", [stop](const CommandNodeBuilder& any) {
            any.executes([stop](const CommandContext& context) { return stop(context, std::nullopt, false); });
            any.argument("sound", resourceLocationArgument(), [stop](const CommandNodeBuilder& sound) {
                sound.suggestsFromClient(std::string{suggestionProviders::kAvailableSounds});
                sound.executes([stop](const CommandContext& context) { return stop(context, std::nullopt, true); });
            });
        });
        for (std::uint8_t ordinal = 0; ordinal <= static_cast<std::uint8_t>(play::SoundSource::Ui); ordinal++) {
            const auto source = static_cast<play::SoundSource>(ordinal);
            targets.literal(std::string{play::soundSourceCommandName(source)}, [stop, source](
                                                                                   const CommandNodeBuilder& mixer) {
                mixer.executes([stop, source](const CommandContext& context) { return stop(context, source, false); });
                mixer.argument("sound", resourceLocationArgument(), [stop, source](const CommandNodeBuilder& sound) {
                    sound.suggestsFromClient(std::string{suggestionProviders::kAvailableSounds});
                    sound.executes(
                        [stop, source](const CommandContext& context) { return stop(context, source, true); });
                });
            });
        }
    });
}

} // namespace basalt::server::runtime::command::commands
