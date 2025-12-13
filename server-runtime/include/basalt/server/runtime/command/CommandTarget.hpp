#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <variant>

namespace basalt::server::runtime::command {

/// Whatever a selector can point at: a player, a summoned entity, or an item on the floor. Commands
/// that work on anything (`/kill`, `/damage`, `/tag`) take this, and every filter question is
/// answered here so the filters need not know which of the three they hold.
///
/// A value holding one of three pointers rather than a hierarchy: without RTTI a caller cannot ask
/// what a base pointer really is.
class CommandTarget {
public:
    /// parity: `EntityTypes.ITEM` and its own quarter-block box.
    static const wire::common::Identifier& itemType();
    static constexpr double kItemWidth = 0.25;
    static constexpr double kItemHeight = 0.25;

    /// parity: `EntityTypes.PLAYER`'s dimensions, which is what a `dx=` box is tested against.
    static constexpr double kPlayerWidth = 0.6;
    static constexpr double kPlayerHeight = 1.8;

    explicit CommandTarget(player::PlayerSession& session);

    CommandTarget(entity::ServerEntity& entity, std::string worldName = {});

    CommandTarget(const world::DroppedItem& item, std::string worldName = {});

    /// The player this points at, or null when it points at something else.
    [[nodiscard]] player::PlayerSession* player() const;

    /// The entity this points at, or null for a player or an item.
    [[nodiscard]] entity::ServerEntity* entity() const;

    [[nodiscard]] std::string name() const;

    [[nodiscard]] std::int32_t entityId() const;

    [[nodiscard]] std::string uuid() const;

    [[nodiscard]] wire::common::Identifier type() const;

    [[nodiscard]] const std::string& worldName() const;

    [[nodiscard]] double x() const;

    [[nodiscard]] double y() const;

    [[nodiscard]] double z() const;

    [[nodiscard]] float yaw() const;

    [[nodiscard]] float pitch() const;

    [[nodiscard]] std::set<std::string> tags() const;

    [[nodiscard]] bool alive() const;

    /// How the scoreboard knows it. parity: `Entity.getScoreboardName`, a player's name and
    /// anything else's UUID.
    [[nodiscard]] std::string scoreboardName() const;

    /// Its collision box, for `dx/dy/dz`.
    [[nodiscard]] double width() const;

    [[nodiscard]] double height() const;

private:
    std::variant<player::PlayerSession*, entity::ServerEntity*, const world::DroppedItem*> what_;
    std::string worldName_;
};

} // namespace basalt::server::runtime::command
