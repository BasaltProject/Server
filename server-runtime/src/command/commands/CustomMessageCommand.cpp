#include <basalt/server/runtime/command/commands/CustomMessageCommand.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace basalt::server::runtime::command::commands {
namespace {

namespace placeholders = wire::common::placeholder;

[[nodiscard]] std::optional<std::string> permissionOf(const config::CustomCommand& declared) {
    return declared.permission.empty() ? std::nullopt : std::optional<std::string>{declared.permission};
}
} // namespace

CustomMessageCommand::CustomMessageCommand(const config::CustomCommand& declared)
    : Command(declared.name, permissionLevel::kAll, declared.aliases, permissionOf(declared)),
      lines_(declared.message) {}

void CustomMessageCommand::build(const CommandNodeBuilder& root) const {
    root.executes([lines = lines_](const CommandContext& context) -> Parsed<std::int32_t> {
        placeholders::PlaceholderContext subject;
        if (player::PlayerSession* caller = context.source().player(); caller != nullptr) {
            subject = subject.with(placeholder::playerSlot(), caller);
        }
        for (const std::string& line : lines) {
            const std::string text = placeholders::substitute(
                line, context.environment().placeholders(), subject,
                [](std::string_view, const placeholders::PlaceholderValue&) -> std::optional<std::string> {
                    return std::string{};
                });
            context.source().sendSystemMessage(wire::text::miniMessage::parse(text));
        }
        return static_cast<std::int32_t>(lines.size());
    });
}

} // namespace basalt::server::runtime::command::commands
