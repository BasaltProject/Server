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

/// parity: `ComponentUtils.updateForEntity`, a `score` or `selector` content turned into the text it names.
/// `holder` is who a score name of `*` stands for.
[[nodiscard]] wire::text::Component resolveContents(const wire::text::Component& component, const CommandSource& source,
                                                    const CommandEnvironment& environment,
                                                    const CommandTarget* holder = nullptr);

/// parity: `ComponentArgument.getResolvedComponent`; null when the argument was not given. The pointer stays valid
/// until the next call on this thread.
[[nodiscard]] const wire::text::Component* resolvedComponent(const CommandContext& context, std::string_view name);

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
