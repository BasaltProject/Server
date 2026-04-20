#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/commands/GroupCommands.hpp>
#include <basalt/server/storage/PlayerRepository.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

[[nodiscard]] bool sameName(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](char a, char b) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(a))) ==
               static_cast<char>(std::tolower(static_cast<unsigned char>(b)));
    });
}

[[nodiscard]] std::vector<std::string> candidates(const SuggestionContext& context, bool operators) {
    std::set<std::string, std::less<>> ops;
    if (storage::PlayerRepository* repository = context.environment().options().playerRepository;
        repository != nullptr) {
        for (const storage::PlayerSummary& summary :
             repository->findInGroup(context.environment().groups().operatorGroup().name)) {
            ops.insert(summary.name);
        }
    }
    std::vector<std::string> named;
    for (const player::PlayerSession* player : context.environment().players().snapshot()) {
        if (ops.contains(player->name()) == operators) {
            named.push_back(player->name());
        }
    }
    return named;
}

[[nodiscard]] std::optional<CommandSyntaxError> assign(const CommandContext& context, const std::string& playerName,
                                                       const std::string& group) {
    storage::PlayerRepository* repository = context.environment().options().playerRepository;
    if (repository == nullptr) {
        return CommandSyntaxError{.component = Component::text("This server has no player database.")};
    }
    player::PlayerSession* online = nullptr;
    for (player::PlayerSession* candidate : context.environment().players().snapshot()) {
        if (sameName(candidate->name(), playerName)) {
            online = candidate;
            break;
        }
    }
    std::string uuid;
    if (online != nullptr) {
        uuid = online->uuid().toString();
    } else if (const std::optional<storage::StoredPlayer> stored = repository->loadByName(playerName);
               stored.has_value()) {
        uuid = stored->uuid;
    } else {
        return CommandSyntaxError{.component = Component::text("No player named '" + playerName +
                                                               "' is online or has ever joined this server.")};
    }

    repository->setGroup(uuid, group);

    if (online != nullptr) {
        online->group = group;
        if (online->connection().sendCommandTree) {
            online->connection().sendCommandTree();
        }
    }
    return std::nullopt;
}

void buildAssignment(const CommandNodeBuilder& root, bool toOperator, const std::string& key) {
    root.argument("targets", gameProfileArgument(), [toOperator, key](const CommandNodeBuilder& targets) {
        // parity: `OpCommand` completes with who is online and not an operator yet.
        targets.suggestsFromServer([toOperator](const SuggestionContext& context) {
            return context.matching(candidates(context, !toOperator));
        });
        targets.executes([toOperator, key](const CommandContext& context) -> Parsed<std::int32_t> {
            const auto* name = context.get<std::string>("targets");
            if (name == nullptr) {
                return 0;
            }
            const config::GroupsConfig& groups = context.environment().groups();
            const std::string group = toOperator ? groups.operatorGroup().name : groups.defaultGroup().name;
            if (const std::optional<CommandSyntaxError> failure = assign(context, *name, group); failure.has_value()) {
                return std::unexpected{*failure};
            }
            context.success(Component::translatable(key, {Component::text(*name)}), true);
            return 1;
        });
    });
}

} // namespace

OpCommand::OpCommand() : Command("op", permissionLevel::kAdmins) {}

void OpCommand::build(const CommandNodeBuilder& root) const { buildAssignment(root, true, "commands.op.success"); }

DeopCommand::DeopCommand() : Command("deop", permissionLevel::kAdmins) {}

void DeopCommand::build(const CommandNodeBuilder& root) const { buildAssignment(root, false, "commands.deop.success"); }

GroupCommand::GroupCommand() : Command("group", permissionLevel::kAdmins) {}

void GroupCommand::build(const CommandNodeBuilder& root) const {
    root.argument("player", gameProfileArgument(), [](const CommandNodeBuilder& player) {
        player.argument("group", wordArgument(), [](const CommandNodeBuilder& group) {
            group.suggestsFromServer([](const SuggestionContext& context) {
                return context.matching(context.environment().groups().names());
            });
            group.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* playerName = context.get<std::string>("player");
                const auto* groupName = context.get<std::string>("group");
                if (playerName == nullptr || groupName == nullptr) {
                    return 0;
                }
                std::string wanted = *groupName;
                std::ranges::transform(wanted, wanted.begin(), [](char c) {
                    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                });
                std::vector<std::string> declared = context.environment().groups().names();
                if (std::ranges::find(declared, wanted) == declared.end()) {
                    std::ranges::sort(declared);
                    std::string list;
                    for (const std::string& name : declared) {
                        list += list.empty() ? name : ", " + name;
                    }
                    return std::unexpected{CommandSyntaxError{
                        .component =
                            Component::text("Unknown group '" + wanted + "'. Declared groups: " + list + ".")}};
                }
                if (const std::optional<CommandSyntaxError> failure = assign(context, *playerName, wanted);
                    failure.has_value()) {
                    return std::unexpected{*failure};
                }
                context.success(Component::text("Put " + *playerName + " in group " + wanted + "."), true);
                return 1;
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
