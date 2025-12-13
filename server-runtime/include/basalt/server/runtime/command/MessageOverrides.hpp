#pragma once

#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <map>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {

/// Replaces what a command answers, by the translation key it would have sent.
///
/// Keyed by translation key rather than by command, because that is what a message *is* here: no
/// command holds its text, it sends `commands.gamemode.success.self` and the client looks it up.
/// Which also explains the cost, and why the file says so plainly: an overridden message stops
/// being a key and becomes literal text, so every client sees the operator's language instead of
/// its own.
///
/// Applied at the one place command feedback leaves the server, not inside each command, so no
/// command has to know this exists and none can forget to.
class MessageOverrides {
public:
    MessageOverrides(std::map<std::string, std::string, std::less<>> messages,
                     const wire::common::placeholder::PlaceholderRegistry& placeholders, wire::text::LangTable lang);

    [[nodiscard]] bool isEmpty() const noexcept { return messages_.empty(); }

    /// The message with every overridden translatable swapped out, or the message itself when
    /// nothing matches, which is the normal case and costs one map lookup per node.
    ///
    /// The player is who the placeholders resolve against; a console message has none.
    [[nodiscard]] wire::text::Component apply(const wire::text::Component& message,
                                              const player::PlayerSession* player) const;

private:
    [[nodiscard]] wire::text::Component transform(const wire::text::Component& component,
                                                  const player::PlayerSession* player) const;

    [[nodiscard]] wire::text::Component render(const std::string& text, const std::vector<wire::text::Component>& args,
                                               const player::PlayerSession* player) const;

    std::map<std::string, std::string, std::less<>> messages_;
    const wire::common::placeholder::PlaceholderRegistry* placeholders_;
    wire::text::LangTable lang_;
};

} // namespace basalt::server::runtime::command
