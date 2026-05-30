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

class CommandTarget {
public:
    /// parity: `EntityTypes.ITEM` and its own quarter-block box.
    static const wire::common::Identifier& itemType();
    static constexpr double kItemWidth = 0.25;
    static constexpr double kItemHeight = 0.25;

    /// parity: `EntityTypes.PLAYER`'s dimensions.
    static constexpr double kPlayerWidth = 0.6;
    static constexpr double kPlayerHeight = 1.8;

    explicit CommandTarget(player::PlayerSession& session);

    CommandTarget(entity::ServerEntity& entity, std::string worldName = {});

    CommandTarget(const world::DroppedItem& item, std::string worldName = {});

    [[nodiscard]] player::PlayerSession* player() const;

    [[nodiscard]] entity::ServerEntity* entity() const;

    [[nodiscard]] const world::DroppedItem* item() const;

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

    /// parity: `Entity.getScoreboardName`.
    [[nodiscard]] std::string scoreboardName() const;

    [[nodiscard]] double width() const;

    [[nodiscard]] double height() const;

private:
    std::variant<player::PlayerSession*, entity::ServerEntity*, const world::DroppedItem*> what_;
    std::string worldName_;
};

} // namespace basalt::server::runtime::command
