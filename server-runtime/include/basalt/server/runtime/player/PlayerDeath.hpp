#pragma once

#include <basalt/server/runtime/Announcement.hpp>
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

/// What a death message names of an entity: its display name and, when it holds a renamed item, that item.
struct DeathLabel {
    wire::text::Component name;
    std::optional<wire::text::Component> heldItem;
};

struct DeathContext {
    PlayerRegistry* registry = nullptr;
    wire::common::Logger logger{"Server"};
    wire::text::LangTable lang;
    std::function<Announcement(const PlayerSession& player, const std::string& deathMessage)> died;
    std::function<std::optional<DeathLabel>(std::int32_t entityId)> labelOf;
};

void killPlayer(PlayerSession& session, std::string_view deathMessageId,
                const std::optional<wire::text::Component>& killer, const DeathContext& context,
                const std::optional<wire::text::Component>& message = std::nullopt);

} // namespace basalt::server::runtime::player
