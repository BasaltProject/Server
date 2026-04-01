#pragma once

#include <basalt/server/runtime/chat/ChatMessage.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/StringReader.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::runtime::command {

class CommandContext;

/// The `minecraft:message` argument beyond its text: the selectors inside it and the chat message
/// it becomes. parity: `MessageArgument`.
namespace messageArguments {

/// parity: `MessageArgument.Message.parseText`'s ceiling, in UTF-16 units as Java counts them.
inline constexpr std::int32_t kMaxLength = 256;

/// parity: `EntitySelectorParser.allowSelectors`, the gamemaster level.
[[nodiscard]] bool allowsSelectors(const CommandSource& source);

/// Reads the rest of the line as a message, refusing one too long and one whose selectors do not
/// parse. The reader ends at the end of the line.
[[nodiscard]] Parsed<std::string> parse(StringReader& reader, const CommandSource& source,
                                        const CommandEnvironment& environment);

/// parity: `Message.toComponent`: the text with every selector replaced by the names it matches.
[[nodiscard]] wire::text::Component resolveComponent(const std::string& text, const CommandSource& source,
                                                     const CommandEnvironment& environment);

/// parity: `MessageArgument.resolveChatMessage`: the signed argument when the player signed one,
/// otherwise a system message; either way with the resolved text as its unsigned content.
[[nodiscard]] chat::PlayerChatMessage resolveChatMessage(const CommandContext& context, std::string_view name);

/// parity: `CommandSourceStack.sendChatMessage`: a player receives it as chat, anything else as the
/// decorated line.
void sendChatMessage(const CommandSource& source, const chat::PlayerChatMessage& message,
                     const chat::BoundChatType& chatType);

/// parity: `ChatType.bind(key, source)`, bound to the source's display name.
[[nodiscard]] chat::BoundChatType bind(std::int32_t chatType, const CommandSource& source,
                                       const CommandEnvironment& environment);

} // namespace messageArguments

} // namespace basalt::server::runtime::command
