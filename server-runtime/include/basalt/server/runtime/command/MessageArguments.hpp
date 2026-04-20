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

/// parity: `MessageArgument`.
namespace messageArguments {

/// parity: `MessageArgument.Message.parseText`'s ceiling.
inline constexpr std::int32_t kMaxLength = 256;

/// parity: `EntitySelectorParser.allowSelectors`, the gamemaster level.
[[nodiscard]] bool allowsSelectors(const CommandSource& source);

[[nodiscard]] Parsed<std::string> parse(StringReader& reader, const CommandSource& source,
                                        const CommandEnvironment& environment);

/// parity: `Message.toComponent`.
[[nodiscard]] wire::text::Component resolveComponent(const std::string& text, const CommandSource& source,
                                                     const CommandEnvironment& environment);

/// parity: `MessageArgument.resolveChatMessage`.
[[nodiscard]] chat::PlayerChatMessage resolveChatMessage(const CommandContext& context, std::string_view name);

/// parity: `CommandSourceStack.sendChatMessage`.
void sendChatMessage(const CommandSource& source, const chat::PlayerChatMessage& message,
                     const chat::BoundChatType& chatType);

/// parity: `ChatType.bind(key, source)`, bound to the source's display name.
[[nodiscard]] chat::BoundChatType bind(std::int32_t chatType, const CommandSource& source,
                                       const CommandEnvironment& environment);

} // namespace messageArguments

} // namespace basalt::server::runtime::command
