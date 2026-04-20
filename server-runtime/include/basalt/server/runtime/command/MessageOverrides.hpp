#pragma once

#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <map>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {

class MessageOverrides {
public:
    MessageOverrides(std::map<std::string, std::string, std::less<>> messages,
                     const wire::common::placeholder::PlaceholderRegistry& placeholders, wire::text::LangTable lang);

    [[nodiscard]] bool isEmpty() const noexcept { return messages_.empty(); }

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
