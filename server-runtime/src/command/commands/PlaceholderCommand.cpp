#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/commands/PlaceholderCommand.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace placeholders = wire::common::placeholder;

using placeholders::PlaceholderContext;
using placeholders::PlaceholderKind;
using placeholders::PlaceholderValue;
using wire::text::Component;
using wire::text::kGray;
using wire::text::kRed;
using wire::text::kWhite;

[[nodiscard]] Component hint(const std::string& text) { return Component::text(text, kGray); }

[[nodiscard]] Component problem(const std::string& name, const PlaceholderValue& value) {
    if (value.kind == PlaceholderKind::Unavailable && value.reason.has_value()) {
        return Component::text("%" + name + "% cannot be filled in here: " + *value.reason + ".", kRed);
    }
    if (value.kind == PlaceholderKind::Unknown) {
        return Component::text("%" + name + "% is not a placeholder this server knows.", kRed);
    }
    return Component::text("%" + name + "% cannot be filled in here.", kRed);
}

[[nodiscard]] bool sameName(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](char a, char b) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(a))) ==
               static_cast<char>(std::tolower(static_cast<unsigned char>(b)));
    });
}

[[nodiscard]] std::expected<player::PlayerSession*, CommandSyntaxError> onlinePlayer(const CommandContext& context,
                                                                                     const std::string& name) {
    if (name == "me") {
        player::PlayerSession* self = context.source().player();
        if (self == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component =
                    Component::text("The console is not a player. Name one, or use --null for no player at all.")}};
        }
        return self;
    }
    for (player::PlayerSession* player : context.environment().players().snapshot()) {
        if (sameName(player->name(), name)) {
            return player;
        }
    }
    return std::unexpected{CommandSyntaxError{
        .component = Component::text("'" + name + "' is not online. Only online players can be asked about.")}};
}

[[nodiscard]] std::expected<PlaceholderContext, CommandSyntaxError> subjectOf(const CommandContext& context) {
    const auto* subject = context.get<std::string>("subject");
    if (subject == nullptr || *subject == "--null") {
        return PlaceholderContext{};
    }
    const auto player = onlinePlayer(context, *subject);
    if (!player.has_value()) {
        return std::unexpected{player.error()};
    }
    return PlaceholderContext{}.with(placeholder::playerSlot(), *player);
}

[[nodiscard]] std::vector<std::string> subjects(const SuggestionContext& context) {
    std::vector<std::string> named;
    for (const player::PlayerSession* player : context.environment().players().snapshot()) {
        named.push_back(player->name());
    }
    named.emplace_back("me");
    named.emplace_back("--null");
    return named;
}

[[nodiscard]] std::string substitute(const CommandContext& context, const PlaceholderContext& placeholderContext,
                                     const std::string& text) {
    return placeholders::substitute(
        text, context.environment().placeholders(), placeholderContext,
        [&context](std::string_view name, const PlaceholderValue& value) -> std::optional<std::string> {
            context.source().sendSystemMessage(problem(std::string{name}, value));
            return std::nullopt;
        });
}

[[nodiscard]] std::string textOf(const CommandContext& context, std::string_view name) {
    const auto* found = context.get<std::string>(name);
    return found == nullptr ? std::string{} : *found;
}

} // namespace

PlaceholderCommand::PlaceholderCommand() : Command("placeholder", permissionLevel::kAdmins, {"placeholders"}) {}

void PlaceholderCommand::build(const CommandNodeBuilder& root) const {
    enum class Destination : std::uint8_t { Caller, Everyone, Dispatcher };
    const auto parseForm = [](const CommandNodeBuilder& node, Destination where) {
        node.argument("subject", wordArgument(), [where](const CommandNodeBuilder& subject) {
            subject.suggestsFromServer(
                [](const SuggestionContext& context) { return context.matching(subjects(context)); });
            subject.argument("text", greedyStringArgument(), [where](const CommandNodeBuilder& text) {
                text.executes([where](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto placeholderContext = subjectOf(context);
                    if (!placeholderContext.has_value()) {
                        return std::unexpected{placeholderContext.error()};
                    }
                    const std::string parsed = substitute(context, *placeholderContext, textOf(context, "text"));
                    if (where == Destination::Everyone) {
                        context.environment().broadcast(wire::text::miniMessage::parse(parsed));
                        return 1;
                    }
                    if (where == Destination::Dispatcher) {
                        context.source().sendSystemMessage(hint("Running: /" + parsed));
                        return context.dispatcher().execute(context.source(),
                                                            parsed.starts_with('/') ? parsed.substr(1) : parsed);
                    }
                    context.source().sendSystemMessage(wire::text::miniMessage::parse(parsed));
                    return 1;
                });
            });
        });
    };
    root.literal("parse", [parseForm](const CommandNodeBuilder& form) { parseForm(form, Destination::Caller); });
    root.literal("bcparse", [parseForm](const CommandNodeBuilder& form) { parseForm(form, Destination::Everyone); });
    root.literal("cmdparse", [parseForm](const CommandNodeBuilder& form) { parseForm(form, Destination::Dispatcher); });

    root.literal("parserel", [](const CommandNodeBuilder& form) {
        form.argument("first", wordArgument(), [](const CommandNodeBuilder& first) {
            first.suggestsFromServer(
                [](const SuggestionContext& context) { return context.matching(subjects(context)); });
            first.argument("second", wordArgument(), [](const CommandNodeBuilder& second) {
                second.suggestsFromServer(
                    [](const SuggestionContext& context) { return context.matching(subjects(context)); });
                second.argument("text", greedyStringArgument(), [](const CommandNodeBuilder& text) {
                    text.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                        const auto first = onlinePlayer(context, textOf(context, "first"));
                        if (!first.has_value()) {
                            return std::unexpected{first.error()};
                        }
                        const auto second = onlinePlayer(context, textOf(context, "second"));
                        if (!second.has_value()) {
                            return std::unexpected{second.error()};
                        }
                        const PlaceholderContext placeholderContext =
                            PlaceholderContext{}
                                .with(placeholder::playerSlot(), *first)
                                .with(placeholder::otherPlayerSlot(), *second);
                        context.source().sendSystemMessage(wire::text::miniMessage::parse(
                            substitute(context, placeholderContext, textOf(context, "text"))));
                        // No placeholder Basalt itself registers looks at the second player yet.
                        context.source().sendSystemMessage(
                            hint("No built-in placeholder is relational yet; this is for plugins to use."));
                        return 1;
                    });
                });
            });
        });
    });

    root.literal("info", [](const CommandNodeBuilder& form) {
        form.argument("name", greedyStringArgument(), [](const CommandNodeBuilder& named) {
            named.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                std::string name = textOf(context, "name");
                while (!name.empty() && name.front() == '%') {
                    name.erase(name.begin());
                }
                while (!name.empty() && name.back() == '%') {
                    name.pop_back();
                }
                const placeholders::PlaceholderRegistry& registry = context.environment().placeholders();
                const std::optional<std::string> identifier = registry.identifierFor(name);
                if (!identifier.has_value()) {
                    std::vector<std::string> known = registry.identifiers();
                    std::ranges::sort(known);
                    std::string listed;
                    for (const std::string& one : known) {
                        listed += listed.empty() ? one : ", " + one;
                    }
                    context.source().sendSystemMessage(
                        problem(name, PlaceholderValue::unknown())
                            .append(Component::text(" Registered: " + listed + ".", kGray)));
                    return 0;
                }
                context.source().sendSystemMessage(hint("%" + name + "% is served by '" + *identifier + "'."));
                const PlaceholderContext here =
                    PlaceholderContext{}.with(placeholder::playerSlot(), context.source().player());
                const PlaceholderValue value = registry.resolve(name, here);
                if (value.isText()) {
                    context.source().sendSystemMessage(
                        Component::text("Here it says: ", kGray).append(Component::text(value.text)));
                } else {
                    context.source().sendSystemMessage(problem(name, value));
                }
                return 1;
            });
        });
    });

    root.literal("list", [](const CommandNodeBuilder& form) {
        form.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const placeholders::PlaceholderRegistry& registry = context.environment().placeholders();
            std::vector<std::string> identifiers = registry.identifiers();
            std::ranges::sort(identifiers);
            if (identifiers.empty()) {
                context.source().sendSystemMessage(hint("Nothing has registered a placeholder."));
                return 0;
            }
            context.source().sendSystemMessage(
                hint(std::to_string(identifiers.size()) + " placeholder identifier(s):"));
            for (const std::string& identifier : identifiers) {
                const std::size_t count = registry.namesOf(identifier).size();
                const std::string tail = count == 0 ? "does not list its names" : std::to_string(count) + " name(s)";
                context.source().sendSystemMessage(
                    Component::text("  %" + identifier, kWhite).append(Component::text("_...%  " + tail, kGray)));
            }
            return static_cast<std::int32_t>(identifiers.size());
        });
        form.argument("identifier", wordArgument(), [](const CommandNodeBuilder& identifier) {
            identifier.suggestsFromServer([](const SuggestionContext& context) {
                return context.matching(context.environment().placeholders().identifiers());
            });
            identifier.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const std::string wanted = textOf(context, "identifier");
                const std::vector<std::string> names = context.environment().placeholders().namesOf(wanted);
                if (names.empty()) {
                    context.source().sendSystemMessage(hint(
                        "'" + wanted + "' has registered no names, or does not list them. Try /placeholder list."));
                    return 0;
                }
                context.source().sendSystemMessage(
                    hint(std::to_string(names.size()) + " placeholder(s) under '" + wanted + "':"));
                std::string listed;
                for (const std::string& name : names) {
                    const std::string one = "%" + wanted + "_" + name + "%";
                    listed += listed.empty() ? one : ", " + one;
                }
                context.source().sendSystemMessage(Component::text(listed, kGray));
                return static_cast<std::int32_t>(names.size());
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
