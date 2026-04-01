#include <basalt/server/runtime/chat/ChatBroadcast.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/EntitySelector.hpp>
#include <basalt/server/runtime/command/MessageArguments.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/wire/text/TextColor.hpp>

#include <optional>
#include <vector>

namespace basalt::server::runtime::command::messageArguments {
namespace {

using wire::text::Component;

/// One selector inside a message, by where it sits in the text.
struct Part {
    std::int32_t start = 0;
    std::int32_t end = 0;
    EntitySelector selector;
};

/// Java's `String.length`: UTF-16 units, so a character past the BMP counts twice.
[[nodiscard]] std::int32_t utf16Length(std::string_view text) {
    std::int32_t length = 0;
    for (const char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        if ((byte & 0xC0) != 0x80) {
            length += byte >= 0xF0 ? 2 : 1;
        }
    }
    return length;
}

[[nodiscard]] bool skippable(const CommandSyntaxError& error) {
    const auto* translatable = error.component.as<Component::Translatable>();
    return translatable != nullptr && (translatable->key == "argument.entity.selector.missing" ||
                                       translatable->key == "argument.entity.selector.unknown");
}

/// parity: `Message.parseText`'s loop: a malformed selector past its type is an error, a bad type
/// only moves on one character.
[[nodiscard]] Parsed<std::vector<Part>> scan(StringReader& reader, const CommandSource& source,
                                             const CommandEnvironment& environment) {
    const std::int32_t offset = reader.cursor();
    std::vector<Part> parts;
    while (reader.canRead()) {
        if (reader.peek() != '@') {
            reader.skip();
            continue;
        }
        const std::int32_t start = reader.cursor();
        Parsed<EntitySelector> selector = EntitySelector::parse(reader, source, environment);
        if (!selector.has_value()) {
            if (!skippable(selector.error())) {
                return std::unexpected{selector.error()};
            }
            reader.setCursor(start + 1);
            continue;
        }
        parts.push_back(Part{.start = start - offset, .end = reader.cursor() - offset, .selector = *selector});
    }
    return parts;
}

/// parity: `EntitySelector.joinNames`, display names joined with a grey comma.
[[nodiscard]] Component joinNames(const std::vector<CommandTarget>& targets, const CommandEnvironment& environment) {
    std::vector<Component> names;
    for (const CommandTarget& target : targets) {
        if (const player::PlayerSession* player = target.player(); player != nullptr) {
            names.push_back(chat::displayName(*player, environment.options().teams));
        } else if (const entity::ServerEntity* entity = target.entity(); entity != nullptr) {
            names.push_back(entity::entityCombat::displayName(*entity));
        } else {
            names.push_back(Component::text(target.name()));
        }
    }
    if (names.empty()) {
        return Component::empty();
    }
    if (names.size() == 1) {
        return names.front();
    }
    return Component::join(Component::text(", ").color(wire::text::kGray), names);
}

} // namespace

bool allowsSelectors(const CommandSource& source) { return source.permissionLevel() >= permissionLevel::kGamemasters; }

Parsed<std::string> parse(StringReader& reader, const CommandSource& source, const CommandEnvironment& environment) {
    const std::string text = reader.remaining();
    const std::int32_t length = utf16Length(text);
    if (length > kMaxLength) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("argument.message.too_long",
                                                                    {Component::text(std::to_string(length)),
                                                                     Component::text(std::to_string(kMaxLength))})}};
    }
    if (allowsSelectors(source)) {
        if (Parsed<std::vector<Part>> parts = scan(reader, source, environment); !parts.has_value()) {
            return std::unexpected{parts.error()};
        }
    }
    reader.setCursor(static_cast<std::int32_t>(reader.string().size()));
    return text;
}

Component resolveComponent(const std::string& text, const CommandSource& source,
                           const CommandEnvironment& environment) {
    if (!allowsSelectors(source)) {
        return Component::text(text);
    }
    StringReader reader{text};
    const Parsed<std::vector<Part>> parts = scan(reader, source, environment);
    if (!parts.has_value() || parts->empty()) {
        return Component::text(text);
    }
    Component result = Component::text(text.substr(0, static_cast<std::size_t>(parts->front().start)));
    std::int32_t readTo = parts->front().start;
    for (const Part& part : *parts) {
        const Component names = joinNames(part.selector.select(source, environment, false), environment);
        if (readTo < part.start) {
            result = result.append(Component::text(
                text.substr(static_cast<std::size_t>(readTo), static_cast<std::size_t>(part.start - readTo))));
        }
        result = result.append(names);
        readTo = part.end;
    }
    if (static_cast<std::size_t>(readTo) < text.size()) {
        result = result.append(Component::text(text.substr(static_cast<std::size_t>(readTo))));
    }
    return result;
}

chat::PlayerChatMessage resolveChatMessage(const CommandContext& context, std::string_view name) {
    const std::string* text = context.get<std::string>(name);
    const std::string content = text == nullptr ? std::string{} : *text;
    const Component formatted = resolveComponent(content, context.source(), context.environment());
    const chat::SignedArguments* signedArguments = context.source().signedArguments();
    if (signedArguments != nullptr) {
        if (const auto found = signedArguments->find(name); found != signedArguments->end()) {
            return found->second.withUnsignedContent(formatted);
        }
    }
    return chat::PlayerChatMessage::system(content, chat::nowMillis()).withUnsignedContent(formatted);
}

void sendChatMessage(const CommandSource& source, const chat::PlayerChatMessage& message,
                     const chat::BoundChatType& chatType) {
    if (const player::PlayerSession* player = source.player(); player != nullptr) {
        player->sendChatMessage(message, chatType);
        return;
    }
    source.sendSystemMessage(chatType.decorate(message.decoratedContent()));
}

chat::BoundChatType bind(std::int32_t chatType, const CommandSource& source, const CommandEnvironment& environment) {
    const player::PlayerSession* player = source.player();
    return chat::BoundChatType{.type = chatType,
                               .name = player != nullptr ? chat::displayName(*player, environment.options().teams)
                                                         : source.displayName(),
                               .target = std::nullopt};
}

} // namespace basalt::server::runtime::command::messageArguments
