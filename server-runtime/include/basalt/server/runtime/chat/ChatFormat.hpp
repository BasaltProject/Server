#pragma once

#include <basalt/wire/text/Component.hpp>

#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::chat {

/// A plugin's chat line with {player} and {message} filled in as plain text, so their `<tags>` do nothing.
[[nodiscard]] wire::text::Component formatLine(std::string format, const std::string& player,
                                               const std::string& message);

/// What a plugin decided about a chat line beyond its text: an empty format keeps vanilla's.
struct ChatRouting {
    std::string format;
    std::optional<std::vector<std::string>> recipients;
};

/// Reads the `player.chat` recipients field: `*` is everybody, otherwise UUIDs separated by commas.
[[nodiscard]] std::optional<std::vector<std::string>> parseRecipients(const std::string& field);

} // namespace basalt::server::runtime::chat
