#pragma once

#include <basalt/wire/text/Component.hpp>

#include <string>

namespace basalt::server::runtime::chat {

/// A plugin's chat line with {player} and {message} filled in as plain text, so their `<tags>` do nothing.
[[nodiscard]] wire::text::Component formatLine(std::string format, const std::string& player,
                                               const std::string& message);

} // namespace basalt::server::runtime::chat
