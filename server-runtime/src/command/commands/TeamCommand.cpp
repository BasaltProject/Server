#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/TeamCommand.hpp>

#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using scoreboard::Team;
using wire::text::Component;

[[nodiscard]] std::string joined(const std::vector<std::string>& names) {
    std::string text;
    for (const std::string& name : names) {
        text += text.empty() ? name : ", " + name;
    }
    return text;
}

[[nodiscard]] CommandSyntaxError unchanged(std::string_view what) {
    return CommandSyntaxError{.component =
                                  Component::translatable("commands.team.option." + std::string{what} + ".unchanged")};
}

/// parity: `TeamColor`'s own lowercase names, which is how `/team modify color` reports one.
[[nodiscard]] std::string teamColorName(play::TeamColor color) {
    static constexpr std::array<std::string_view, 16> kNames{
        "black",     "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple",  "gold",   "gray",
        "dark_gray", "blue",      "green",      "aqua",      "red",      "light_purple", "yellow", "white"};
    return std::string{kNames[static_cast<std::size_t>(color)]};
}

} // namespace

TeamCommand::TeamCommand(scoreboard::Teams& teams) : Command("team", permissionLevel::kGamemasters), teams_(&teams) {}

void TeamCommand::build(const CommandNodeBuilder& root) const {
    const auto teamOf = [this](const CommandContext& context) -> std::expected<Team*, CommandSyntaxError> {
        const auto* name = context.get<std::string>("team");
        Team* team = name == nullptr ? nullptr : teams_->get(*name);
        if (team == nullptr) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable(
                                       "team.notFound", {Component::text(name == nullptr ? std::string{} : *name)})}};
        }
        return team;
    };
    const auto broadcast = [](const CommandContext& context, const play::SetPlayerTeamPacket& packet) {
        for (const player::PlayerSession* player : context.environment().players().snapshot()) {
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(play::SetPlayerTeamPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
    };
    const auto changed = [broadcast](const CommandContext& context, const Team& team) {
        broadcast(context, play::SetPlayerTeamPacket{.teamName = team.name,
                                                     .method = play::SetPlayerTeamPacket::kMethodChange,
                                                     .parameters = team.parameters()});
    };

    root.literal("list", [this, teamOf](const CommandNodeBuilder& list) {
        list.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const std::vector<const Team*> all = std::as_const(*teams_).all();
            std::vector<std::string> named;
            for (const Team* team : all) {
                named.push_back(team->name);
            }
            context.success(all.empty() ? Component::translatable("commands.team.list.teams.empty")
                                        : Component::translatable("commands.team.list.teams.success",
                                                                  {Component::text(std::to_string(all.size())),
                                                                   Component::text(joined(named))}));
            return static_cast<std::int32_t>(all.size());
        });
        list.argument("team", teamArgument(), [teamOf](const CommandNodeBuilder& named) {
            named.executes([teamOf](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto team = teamOf(context);
                if (!team.has_value()) {
                    return std::unexpected{team.error()};
                }
                context.success(
                    (*team)->members.empty()
                        ? Component::translatable("commands.team.list.members.empty", {(*team)->displayName})
                        : Component::translatable("commands.team.list.members.success",
                                                  {(*team)->displayName,
                                                   Component::text(std::to_string((*team)->members.size())),
                                                   Component::text(joined((*team)->members))}));
                return static_cast<std::int32_t>((*team)->members.size());
            });
        });
    });

    root.literal("add", [this, broadcast](const CommandNodeBuilder& form) {
        const auto add = [this, broadcast](const CommandContext& context, bool named) -> Parsed<std::int32_t> {
            const auto* name = context.get<std::string>("team");
            Team* team = name == nullptr ? nullptr : teams_->add(*name);
            if (team == nullptr) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.team.add.duplicate")}};
            }
            if (named) {
                if (const auto* label = context.get<Component>("displayName"); label != nullptr) {
                    team->displayName = *label;
                }
            }
            broadcast(context, play::SetPlayerTeamPacket{.teamName = team->name,
                                                         .method = play::SetPlayerTeamPacket::kMethodAdd,
                                                         .parameters = team->parameters()});
            context.success(Component::translatable("commands.team.add.success", {team->displayName}), true);
            return static_cast<std::int32_t>(teams_->all().size());
        };
        form.argument("team", wordArgument(), [add](const CommandNodeBuilder& name) {
            name.executes([add](const CommandContext& context) { return add(context, false); });
            name.argument("displayName", componentArgument(), [add](const CommandNodeBuilder& label) {
                label.executes([add](const CommandContext& context) { return add(context, true); });
            });
        });
    });

    root.literal("remove", [this, teamOf, broadcast](const CommandNodeBuilder& form) {
        form.argument("team", teamArgument(), [this, teamOf, broadcast](const CommandNodeBuilder& named) {
            named.executes([this, teamOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto team = teamOf(context);
                if (!team.has_value()) {
                    return std::unexpected{team.error()};
                }
                const std::string name = (*team)->name;
                const Component label = (*team)->displayName;
                static_cast<void>(teams_->remove(name));
                broadcast(context, play::SetPlayerTeamPacket{.teamName = name,
                                                             .method = play::SetPlayerTeamPacket::kMethodRemove});
                context.success(Component::translatable("commands.team.remove.success", {label}), true);
                return static_cast<std::int32_t>(teams_->all().size());
            });
        });
    });

    root.literal("empty", [teamOf, broadcast](const CommandNodeBuilder& form) {
        form.argument("team", teamArgument(), [teamOf, broadcast](const CommandNodeBuilder& named) {
            named.executes([teamOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto team = teamOf(context);
                if (!team.has_value()) {
                    return std::unexpected{team.error()};
                }
                if ((*team)->members.empty()) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable("commands.team.empty.unchanged")}};
                }
                const std::vector<std::string> removed = (*team)->members;
                (*team)->members.clear();
                broadcast(context, play::SetPlayerTeamPacket{.teamName = (*team)->name,
                                                             .method = play::SetPlayerTeamPacket::kMethodLeave,
                                                             .members = removed});
                context.success(
                    Component::translatable("commands.team.empty.success",
                                            {Component::text(std::to_string(removed.size())), (*team)->displayName}),
                    true);
                return static_cast<std::int32_t>(removed.size());
            });
        });
    });

    root.literal("join", [this, teamOf, broadcast](const CommandNodeBuilder& form) {
        const auto join = [this, teamOf, broadcast](const CommandContext& context,
                                                    std::vector<std::string> members) -> Parsed<std::int32_t> {
            const auto team = teamOf(context);
            if (!team.has_value()) {
                return std::unexpected{team.error()};
            }
            for (const std::string& member : members) {
                const Team* previous = teams_->join(**team, member);
                if (previous != nullptr && previous->name != (*team)->name) {
                    broadcast(context, play::SetPlayerTeamPacket{.teamName = previous->name,
                                                                 .method = play::SetPlayerTeamPacket::kMethodLeave,
                                                                 .members = {member}});
                }
            }
            broadcast(context, play::SetPlayerTeamPacket{.teamName = (*team)->name,
                                                         .method = play::SetPlayerTeamPacket::kMethodJoin,
                                                         .members = members});
            context.success(
                members.size() == 1
                    ? Component::translatable("commands.team.join.success.single",
                                              {Component::text(members.front()), (*team)->displayName})
                    : Component::translatable("commands.team.join.success.multiple",
                                              {Component::text(std::to_string(members.size())), (*team)->displayName}),
                true);
            return static_cast<std::int32_t>(members.size());
        };
        form.argument("team", teamArgument(), [join](const CommandNodeBuilder& named) {
            named.executes([join](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<player::PlayerSession*> self = context.requirePlayer();
                if (!self.has_value()) {
                    return std::unexpected{self.error()};
                }
                return join(context, {(*self)->name()});
            });
            named.argument("members", scoreHolderArgument(), [join](const CommandNodeBuilder& members) {
                members.executes([join](const CommandContext& context) {
                    const auto* named = context.get<std::vector<std::string>>("members");
                    return join(context, named == nullptr ? std::vector<std::string>{} : *named);
                });
            });
        });
    });

    root.literal("leave", [this, broadcast](const CommandNodeBuilder& form) {
        form.argument("members", scoreHolderArgument(), [this, broadcast](const CommandNodeBuilder& members) {
            members.executes([this, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* named = context.get<std::vector<std::string>>("members");
                if (named == nullptr) {
                    return 0;
                }
                std::int32_t left = 0;
                for (const std::string& member : *named) {
                    const Team* previous = teams_->leave(member);
                    if (previous == nullptr) {
                        continue;
                    }
                    left++;
                    broadcast(context, play::SetPlayerTeamPacket{.teamName = previous->name,
                                                                 .method = play::SetPlayerTeamPacket::kMethodLeave,
                                                                 .members = {member}});
                }
                context.success(named->size() == 1
                                    ? Component::translatable("commands.team.leave.success.single",
                                                              {Component::text(named->front())})
                                    : Component::translatable("commands.team.leave.success.multiple",
                                                              {Component::text(std::to_string(named->size()))}),
                                true);
                return left;
            });
        });
    });

    root.literal("modify", [teamOf, changed](const CommandNodeBuilder& form) {
        form.argument("team", teamArgument(), [teamOf, changed](const CommandNodeBuilder& named) {
            /// The three text fields differ only in which one they land on and what they then say.
            for (const auto& [what, field] : std::array<std::pair<std::string_view, Component Team::*>, 3>{
                     {{"displayName", &Team::displayName}, {"prefix", &Team::prefix}, {"suffix", &Team::suffix}}}) {
                named.literal(std::string{what}, [what, field, teamOf, changed](const CommandNodeBuilder& one) {
                    one.argument(std::string{what}, componentArgument(),
                                 [what, field, teamOf, changed](const CommandNodeBuilder& value) {
                                     value.executes([what, field, teamOf,
                                                     changed](const CommandContext& context) -> Parsed<std::int32_t> {
                                         const auto team = teamOf(context);
                                         if (!team.has_value()) {
                                             return std::unexpected{team.error()};
                                         }
                                         const auto* wanted = context.get<Component>(what);
                                         if (wanted == nullptr) {
                                             return 0;
                                         }
                                         // parity: only the display name refuses a repeat; the two
                                         // affixes take one silently.
                                         if (what == "displayName" && (*team)->*field == *wanted) {
                                             return std::unexpected{unchanged("name")};
                                         }
                                         (*team)->*field = *wanted;
                                         changed(context, **team);
                                         context.success(Component::translatable(
                                                             "commands.team.option." +
                                                                 std::string{what == "displayName" ? "name" : what} +
                                                                 ".success",
                                                             {(*team)->*field}),
                                                         what == "displayName");
                                         return 0;
                                     });
                                 });
                });
            }

            named.literal("color", [teamOf, changed](const CommandNodeBuilder& colours) {
                const auto set = [teamOf, changed](const CommandContext& context,
                                                   std::optional<play::TeamColor> colour) -> Parsed<std::int32_t> {
                    const auto team = teamOf(context);
                    if (!team.has_value()) {
                        return std::unexpected{team.error()};
                    }
                    if ((*team)->color == colour) {
                        return std::unexpected{unchanged("color")};
                    }
                    (*team)->color = colour;
                    changed(context, **team);
                    context.success(
                        colour.has_value()
                            ? Component::translatable("commands.team.option.color.success",
                                                      {(*team)->displayName, Component::text(teamColorName(*colour))})
                            : Component::translatable("commands.team.option.color.clear.success",
                                                      {(*team)->displayName}),
                        true);
                    return 0;
                };
                colours.literal("reset", [set](const CommandNodeBuilder& reset) {
                    reset.executes([set](const CommandContext& context) { return set(context, std::nullopt); });
                });
                colours.argument("value", teamColorArgument(), [set](const CommandNodeBuilder& value) {
                    value.executes([set](const CommandContext& context) {
                        const auto* colour = context.get<play::TeamColor>("value");
                        return colour == nullptr ? Parsed<std::int32_t>{0} : set(context, *colour);
                    });
                });
            });

            /// The two flags differ only in which field they move and the half of the key they name.
            for (const auto& [what, field] : std::array<std::pair<std::string_view, bool Team::*>, 2>{
                     {{"friendlyfire", &Team::friendlyFire},
                      {"seeFriendlyInvisibles", &Team::seeFriendlyInvisibles}}}) {
                named.literal(
                    what == "friendlyfire" ? "friendlyFire" : std::string{what},
                    [what, field, teamOf, changed](const CommandNodeBuilder& one) {
                        one.argument(
                            "allowed", boolArgument(),
                            [what, field, teamOf, changed](const CommandNodeBuilder& allowed) {
                                allowed.executes([what, field, teamOf,
                                                  changed](const CommandContext& context) -> Parsed<std::int32_t> {
                                    const auto team = teamOf(context);
                                    if (!team.has_value()) {
                                        return std::unexpected{team.error()};
                                    }
                                    const auto* wanted = context.get<bool>("allowed");
                                    if (wanted == nullptr) {
                                        return 0;
                                    }
                                    const std::string key = "commands.team.option." + std::string{what} + ".";
                                    if ((*team)->*field == *wanted) {
                                        return std::unexpected{CommandSyntaxError{
                                            .component = Component::translatable(key + "already" +
                                                                                 (*wanted ? "Enabled" : "Disabled"))}};
                                    }
                                    (*team)->*field = *wanted;
                                    changed(context, **team);
                                    context.success(Component::translatable(key + (*wanted ? "enabled" : "disabled"),
                                                                            {(*team)->displayName}),
                                                    true);
                                    return 0;
                                });
                            });
                    });
            }

            /// The two visibilities take the same four words and land on different fields.
            for (const auto& [what, field] :
                 std::array<std::pair<std::string_view, play::NameTagVisibility Team::*>, 2>{
                     {{"nametagVisibility", &Team::nameTagVisibility},
                      {"deathMessageVisibility", &Team::deathMessageVisibility}}}) {
                named.literal(std::string{what}, [what, field, teamOf, changed](const CommandNodeBuilder& one) {
                    for (std::uint8_t ordinal = 0;
                         ordinal <= static_cast<std::uint8_t>(play::NameTagVisibility::HideForOwnTeam); ordinal++) {
                        const auto visibility = static_cast<play::NameTagVisibility>(ordinal);
                        one.literal(
                            std::string{play::nameTagVisibilityName(visibility)},
                            [what, field, visibility, teamOf, changed](const CommandNodeBuilder& value) {
                                value.executes([what, field, visibility, teamOf,
                                                changed](const CommandContext& context) -> Parsed<std::int32_t> {
                                    const auto team = teamOf(context);
                                    if (!team.has_value()) {
                                        return std::unexpected{team.error()};
                                    }
                                    if ((*team)->*field == visibility) {
                                        return std::unexpected{unchanged(what)};
                                    }
                                    (*team)->*field = visibility;
                                    changed(context, **team);
                                    context.success(
                                        Component::translatable(
                                            "commands.team.option." + std::string{what} + ".success",
                                            {(*team)->displayName,
                                             Component::text(std::string{play::nameTagVisibilityName(visibility)})}),
                                        true);
                                    return 0;
                                });
                            });
                    }
                });
            }

            named.literal("collisionRule", [teamOf, changed](const CommandNodeBuilder& rules) {
                for (std::uint8_t ordinal = 0; ordinal <= static_cast<std::uint8_t>(play::CollisionRule::PushOwnTeam);
                     ordinal++) {
                    const auto rule = static_cast<play::CollisionRule>(ordinal);
                    rules.literal(std::string{play::collisionRuleName(rule)}, [rule, teamOf,
                                                                               changed](const CommandNodeBuilder& one) {
                        one.executes([rule, teamOf, changed](const CommandContext& context) -> Parsed<std::int32_t> {
                            const auto team = teamOf(context);
                            if (!team.has_value()) {
                                return std::unexpected{team.error()};
                            }
                            if ((*team)->collisionRule == rule) {
                                return std::unexpected{unchanged("collisionRule")};
                            }
                            (*team)->collisionRule = rule;
                            changed(context, **team);
                            context.success(
                                Component::translatable("commands.team.option.collisionRule.success",
                                                        {(*team)->displayName,
                                                         Component::text(std::string{play::collisionRuleName(rule)})}),
                                true);
                            return 0;
                        });
                    });
                }
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
