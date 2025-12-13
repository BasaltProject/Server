#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/commands/HudCommands.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using wire::text::Component;

void sendTo(const player::PlayerSession& player, std::int32_t packetId,
            const std::function<void(wire::io::ByteBuffer&)>& write) {
    if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(packetId, write);
    }
}

[[nodiscard]] const std::vector<player::PlayerSession*>& targetsOf(const CommandContext& context,
                                                                   std::string_view name) {
    static const std::vector<player::PlayerSession*> none;
    const auto* found = context.get<std::vector<player::PlayerSession*>>(name);
    return found == nullptr ? none : *found;
}

/// parity: every `/title` form reports with the same pair of keys, one for a single target.
void reportTitle(const CommandContext& context, const std::vector<player::PlayerSession*>& targets,
                 const std::string& key) {
    context.success(targets.size() == 1 ? Component::translatable("commands.title." + key + ".single",
                                                                  {Component::text(targets.front()->name())})
                                        : Component::translatable("commands.title." + key + ".multiple",
                                                                  {Component::text(std::to_string(targets.size()))}),
                    true);
}

/// parity: `ExperienceCommand`'s own message, which names the amount and then who it applied to.
[[nodiscard]] Component experienceMessage(const std::string& action, bool points, std::int32_t amount,
                                          const std::vector<player::PlayerSession*>& targets) {
    const std::string key = "commands.experience." + action + (points ? ".points" : ".levels") + ".success.";
    const Component count = Component::text(std::to_string(amount));
    return targets.size() == 1
               ? Component::translatable(key + "single", {count, Component::text(targets.front()->name())})
               : Component::translatable(key + "multiple", {count, Component::text(std::to_string(targets.size()))});
}

/// Where a block coordinate lands, or the floor of where the asker stands when none was given.
[[nodiscard]] std::array<std::int32_t, 3> blockAt(const BlockCoordinates* coordinates,
                                                  const std::array<double, 3>& from) {
    return coordinates != nullptr ? coordinates->resolve(from)
                                  : std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(from[0])),
                                                                static_cast<std::int32_t>(std::floor(from[1])),
                                                                static_cast<std::int32_t>(std::floor(from[2]))};
}

} // namespace

TitleCommand::TitleCommand() : Command("title", permissionLevel::kGamemasters) {}

void TitleCommand::build(const CommandNodeBuilder& root) const {
    root.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
        for (const bool resetTimes : {false, true}) {
            targets.literal(resetTimes ? "reset" : "clear", [resetTimes](const CommandNodeBuilder& form) {
                form.executes([resetTimes](const CommandContext& context) -> Parsed<std::int32_t> {
                    const std::vector<player::PlayerSession*>& matched = targetsOf(context, "targets");
                    const play::ClearTitlesPacket packet{.resetTimes = resetTimes};
                    for (const player::PlayerSession* target : matched) {
                        sendTo(*target, play::ClearTitlesPacket::kId,
                               [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                    }
                    reportTitle(context, matched, resetTimes ? "reset" : "cleared");
                    return static_cast<std::int32_t>(matched.size());
                });
            });
        }
        for (const std::string_view kind : {"title", "subtitle", "actionbar"}) {
            targets.literal(std::string{kind}, [kind](const CommandNodeBuilder& form) {
                form.argument("title", componentArgument(), [kind](const CommandNodeBuilder& text) {
                    text.executes([kind](const CommandContext& context) -> Parsed<std::int32_t> {
                        const std::vector<player::PlayerSession*>& matched = targetsOf(context, "targets");
                        const auto* body = context.get<Component>("title");
                        const play::TitleTextPacket packet{.text = body == nullptr ? Component{} : *body};
                        const std::int32_t id = kind == "title"      ? play::kSetTitleTextId
                                                : kind == "subtitle" ? play::kSetSubtitleTextId
                                                                     : play::kSetActionBarTextId;
                        for (const player::PlayerSession* target : matched) {
                            sendTo(*target, id, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                        }
                        reportTitle(context, matched, "show." + std::string{kind});
                        return static_cast<std::int32_t>(matched.size());
                    });
                });
            });
        }
        targets.literal("times", [](const CommandNodeBuilder& times) {
            times.argument("fadeIn", timeArgument(0), [](const CommandNodeBuilder& fadeIn) {
                fadeIn.argument("stay", timeArgument(0), [](const CommandNodeBuilder& stay) {
                    stay.argument("fadeOut", timeArgument(0), [](const CommandNodeBuilder& fadeOut) {
                        fadeOut.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                            const std::vector<player::PlayerSession*>& matched = targetsOf(context, "targets");
                            const auto ticks = [&context](std::string_view name) {
                                const auto* value = context.get<std::int32_t>(name);
                                return value == nullptr ? 0 : *value;
                            };
                            const play::SetTitlesAnimationPacket packet{
                                .fadeIn = ticks("fadeIn"), .stay = ticks("stay"), .fadeOut = ticks("fadeOut")};
                            for (const player::PlayerSession* target : matched) {
                                sendTo(*target, play::SetTitlesAnimationPacket::kId,
                                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                            }
                            reportTitle(context, matched, "times");
                            return static_cast<std::int32_t>(matched.size());
                        });
                    });
                });
            });
        });
    });
}

ExperienceCommand::ExperienceCommand() : Command("experience", permissionLevel::kGamemasters, {"xp"}) {}

void ExperienceCommand::build(const CommandNodeBuilder& root) const {
    const auto give = [](const CommandContext& context, bool points) -> Parsed<std::int32_t> {
        const std::vector<player::PlayerSession*>& matched = targetsOf(context, "target");
        const auto* amount = context.get<std::int32_t>("amount");
        if (amount == nullptr) {
            return 0;
        }
        for (player::PlayerSession* target : matched) {
            if (points) {
                target->experience.givePoints(*amount);
            } else {
                target->experience.giveLevels(*amount);
            }
            if (target->connection().syncExperience) {
                target->connection().syncExperience();
            }
        }
        context.success(experienceMessage("add", points, *amount, matched), true);
        return static_cast<std::int32_t>(matched.size());
    };
    const auto assign = [](const CommandContext& context, bool points) -> Parsed<std::int32_t> {
        const std::vector<player::PlayerSession*>& matched = targetsOf(context, "target");
        const auto* amount = context.get<std::int32_t>("amount");
        if (amount == nullptr) {
            return 0;
        }
        std::int32_t applied = 0;
        for (player::PlayerSession* target : matched) {
            if (points) {
                // parity: setting more points than the current level is worth is refused outright
                // rather than silently rolling over into the next level.
                if (*amount >= target->experience.xpNeededForNextLevel()) {
                    continue;
                }
                target->experience.setPoints(*amount);
            } else {
                target->experience.setLevels(*amount);
            }
            if (target->connection().syncExperience) {
                target->connection().syncExperience();
            }
            applied++;
        }
        if (applied == 0) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.experience.set.points.invalid")}};
        }
        context.success(experienceMessage("set", points, *amount, matched), true);
        return static_cast<std::int32_t>(matched.size());
    };

    for (const bool adding : {true, false}) {
        root.literal(adding ? "add" : "set", [adding, give, assign](const CommandNodeBuilder& form) {
            form.argument("target", playersArgument(false), [adding, give, assign](const CommandNodeBuilder& target) {
                target.argument(
                    "amount", adding ? integerArgument() : integerArgument(0),
                    [adding, give, assign](const CommandNodeBuilder& amount) {
                        const auto run = [adding, give, assign](const CommandContext& context, bool points) {
                            return adding ? give(context, points) : assign(context, points);
                        };
                        amount.executes([run](const CommandContext& context) { return run(context, true); });
                        amount.literal("points", [run](const CommandNodeBuilder& points) {
                            points.executes([run](const CommandContext& context) { return run(context, true); });
                        });
                        amount.literal("levels", [run](const CommandNodeBuilder& levels) {
                            levels.executes([run](const CommandContext& context) { return run(context, false); });
                        });
                    });
            });
        });
    }
    root.literal("query", [](const CommandNodeBuilder& query) {
        query.argument("target", playersArgument(true), [](const CommandNodeBuilder& target) {
            for (const bool points : {true, false}) {
                target.literal(points ? "points" : "levels", [points](const CommandNodeBuilder& form) {
                    form.executes([points](const CommandContext& context) -> Parsed<std::int32_t> {
                        const std::vector<player::PlayerSession*>& matched = targetsOf(context, "target");
                        if (matched.size() != 1) {
                            return 0;
                        }
                        const player::PlayerSession& one = *matched.front();
                        const std::int32_t result =
                            points
                                ? static_cast<std::int32_t>(one.experience.progress() *
                                                            static_cast<float>(one.experience.xpNeededForNextLevel()))
                                : one.experience.level();
                        context.success(Component::translatable(
                            points ? "commands.experience.query.points" : "commands.experience.query.levels",
                            {Component::text(one.name()), Component::text(std::to_string(result))}));
                        return result;
                    });
                });
            }
        });
    });
}

SpawnPointCommand::SpawnPointCommand() : Command("spawnpoint", permissionLevel::kGamemasters) {}

namespace {

[[nodiscard]] Parsed<std::int32_t> setSpawnPoint(const CommandContext& context,
                                                 const std::vector<player::PlayerSession*>& targets,
                                                 const BlockCoordinates* coordinates, float angle) {
    storage::AccessRepository* access = context.environment().options().access;
    if (access == nullptr) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::text("This server has no player database to remember a spawn point in.")}};
    }
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return std::unexpected{world.error()};
    }
    for (const player::PlayerSession* target : targets) {
        const std::array<std::int32_t, 3> at = blockAt(coordinates, {target->x, target->y, target->z});
        access->setSpawnPoint(
            storage::StoredSpawnPoint{.uuid = target->uuid().toString(),
                                      .world = target->worldName.empty() ? (*world)->name() : target->worldName,
                                      // The half block is the middle of it, which is where a respawn puts feet.
                                      .x = at[0] + 0.5,
                                      .y = static_cast<double>(at[1]),
                                      .z = at[2] + 0.5,
                                      .yaw = angle});
    }
    const std::array<std::int32_t, 3> reported = blockAt(coordinates, context.source().position());
    std::vector<Component> args{Component::text(std::to_string(reported[0])),
                                Component::text(std::to_string(reported[1])),
                                Component::text(std::to_string(reported[2])), Component::text(std::to_string(angle)),
                                Component::text((*world)->name())};
    args.push_back(targets.size() == 1 ? Component::text(targets.front()->name())
                                       : Component::text(std::to_string(targets.size())));
    context.success(Component::translatable(targets.size() == 1 ? "commands.spawnpoint.success.single"
                                                                : "commands.spawnpoint.success.multiple",
                                            std::move(args)),
                    true);
    return static_cast<std::int32_t>(targets.size());
}

} // namespace

void SpawnPointCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
        const Parsed<player::PlayerSession*> self = context.requirePlayer();
        if (!self.has_value()) {
            return std::unexpected{self.error()};
        }
        return setSpawnPoint(context, {*self}, nullptr, 0.0F);
    });
    root.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
        targets.executes([](const CommandContext& context) {
            return setSpawnPoint(context, targetsOf(context, "targets"), nullptr, 0.0F);
        });
        targets.argument("pos", blockPosArgument(), [](const CommandNodeBuilder& pos) {
            pos.executes([](const CommandContext& context) {
                return setSpawnPoint(context, targetsOf(context, "targets"), context.get<BlockCoordinates>("pos"),
                                     0.0F);
            });
            pos.argument("rotation", rotationArgument(), [](const CommandNodeBuilder& rotation) {
                rotation.executes([](const CommandContext& context) {
                    const auto* facing = context.get<std::pair<float, float>>("rotation");
                    return setSpawnPoint(context, targetsOf(context, "targets"), context.get<BlockCoordinates>("pos"),
                                         facing == nullptr ? 0.0F : facing->first);
                });
            });
        });
    });
}

ConfiguredCommand::ConfiguredCommand(config::CustomCommand definition)
    : Command(definition.name, permissionLevel::kAll, definition.aliases,
              definition.permission.empty() ? std::nullopt : std::optional{definition.permission}),
      definition_(std::move(definition)) {}

void ConfiguredCommand::build(const CommandNodeBuilder& root) const {
    root.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
        const wire::common::placeholder::PlaceholderContext placeholders =
            wire::common::placeholder::PlaceholderContext{}.with(placeholder::playerSlot(), context.source().player());
        for (const std::string& line : definition_.message) {
            const std::string text =
                wire::common::placeholder::substitute(line, context.environment().placeholders(), placeholders);
            context.source().sendSystemMessage(wire::text::miniMessage::parse(text));
        }
        return 1;
    });
}

} // namespace basalt::server::runtime::command::commands
