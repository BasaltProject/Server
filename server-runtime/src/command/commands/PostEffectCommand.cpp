#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/commands/PostEffectCommand.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

[[nodiscard]] const std::vector<CommandTarget>& targetsOf(const CommandContext& context, std::string_view name) {
    static const std::vector<CommandTarget> none;
    const auto* found = context.get<std::vector<CommandTarget>>(std::string{name});
    return found == nullptr ? none : *found;
}

/// parity: `ServerPlayer.sendPostEffects`, the whole list, an empty one clearing the screen.
void send(const player::PlayerSession& player) {
    if (!player.connection().sendPlayPacket) {
        return;
    }
    const wire::protocol::configuration::PostEffectsPacket packet{.effects = player.postEffects};
    player.connection().sendPlayPacket(wire::protocol::configuration::PostEffectsPacket::kPlayId,
                                       [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

/// parity: `CommandResponseTracker`: fail when nobody changed, name the one, count the many.
[[nodiscard]] Parsed<std::int32_t> respond(const CommandContext& context, std::string_view verb,
                                           const std::vector<player::PlayerSession*>& changed,
                                           const std::optional<std::string>& effect) {
    const std::string key = "commands.posteffect." + std::string{verb};
    if (changed.empty()) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable(key + ".failed")}};
    }
    std::vector<Component> arguments;
    if (effect.has_value()) {
        arguments.push_back(Component::text(*effect));
    }
    const bool single = changed.size() == 1;
    arguments.push_back(
        Component::text(single ? std::string{changed.front()->name()} : std::to_string(changed.size())));
    context.success(Component::translatable(key + (single ? ".success.single" : ".success.multiple"), arguments), true);
    return static_cast<std::int32_t>(changed.size());
}

/// One change to every target that is a player, and the new list sent to each one it changed.
template <typename Change>
[[nodiscard]] Parsed<std::int32_t> change(const CommandContext& context, std::string_view verb,
                                          const std::optional<std::string>& effect, Change&& apply) {
    std::vector<player::PlayerSession*> changed;
    for (const CommandTarget& target : targetsOf(context, "targets")) {
        player::PlayerSession* session = target.player();
        if (session != nullptr && apply(*session)) {
            send(*session);
            changed.push_back(session);
        }
    }
    return respond(context, verb, changed, effect);
}

[[nodiscard]] std::optional<std::string> effectOf(const CommandContext& context) {
    const auto* id = context.get<wire::common::Identifier>("posteffect");
    return id == nullptr ? std::nullopt : std::optional{id->toString()};
}

} // namespace

PostEffectCommand::PostEffectCommand() : Command("posteffect", permissionLevel::kGamemasters) {}

void PostEffectCommand::build(const CommandNodeBuilder& root) const {
    root.literal("add", [](const CommandNodeBuilder& add) {
        add.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
            targets.argument("posteffect", resourceLocationArgument(), [](const CommandNodeBuilder& effect) {
                effect.executes([](const CommandContext& context) {
                    const std::optional<std::string> added = effectOf(context);
                    return change(context, "add", added, [&added](player::PlayerSession& player) {
                        if (!added.has_value() ||
                            std::ranges::find(player.postEffects, *added) != player.postEffects.end()) {
                            return false;
                        }
                        player.postEffects.push_back(*added);
                        return true;
                    });
                });
            });
        });
    });
    root.literal("clear", [](const CommandNodeBuilder& clear) {
        clear.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
            targets.executes([](const CommandContext& context) {
                return change(context, "clear", std::nullopt, [](player::PlayerSession& player) {
                    const bool had = !player.postEffects.empty();
                    player.postEffects.clear();
                    return had;
                });
            });
        });
    });
    root.literal("list", [](const CommandNodeBuilder& list) {
        list.argument("target", playersArgument(true), [](const CommandNodeBuilder& target) {
            target.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const std::vector<CommandTarget>& targets = targetsOf(context, "target");
                player::PlayerSession* player = targets.empty() ? nullptr : targets.front().player();
                if (player == nullptr) {
                    return 0;
                }
                const Component who = Component::text(std::string{player->name()});
                if (player->postEffects.empty()) {
                    context.success(Component::translatable("commands.posteffect.list.empty", {who}), false);
                    return 0;
                }
                std::string names;
                for (const std::string& effect : player->postEffects) {
                    names += names.empty() ? effect : ", " + effect;
                }
                context.success(
                    Component::translatable(
                        "commands.posteffect.list.success",
                        {who, Component::text(std::to_string(player->postEffects.size())), Component::text(names)}),
                    false);
                return static_cast<std::int32_t>(player->postEffects.size());
            });
        });
    });
    root.literal("remove", [](const CommandNodeBuilder& remove) {
        remove.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
            targets.argument("posteffect", resourceLocationArgument(), [](const CommandNodeBuilder& effect) {
                effect.executes([](const CommandContext& context) {
                    const std::optional<std::string> removed = effectOf(context);
                    return change(context, "remove", removed, [&removed](player::PlayerSession& player) {
                        const auto found = removed.has_value() ? std::ranges::find(player.postEffects, *removed)
                                                               : player.postEffects.end();
                        if (found == player.postEffects.end()) {
                            return false;
                        }
                        player.postEffects.erase(found);
                        return true;
                    });
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
