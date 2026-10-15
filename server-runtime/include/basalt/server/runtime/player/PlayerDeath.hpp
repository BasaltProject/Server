#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;
class PlayerRegistry;

struct DeathContext {
    PlayerRegistry* registry = nullptr;
    wire::common::Logger logger{"Server"};
    wire::text::LangTable lang;
    std::function<void(const PlayerSession& player, const std::string& deathMessage)> died;
};

void killPlayer(PlayerSession& session, std::string_view deathMessageId,
                const std::optional<wire::text::Component>& killer, const DeathContext& context);

} // namespace basalt::server::runtime::player
