#pragma once

#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <string_view>

namespace basalt::server::runtime::player {

[[nodiscard]] constexpr std::string_view faceName(const wire::protocol::play::Direction face) {
    switch (face) {
    case wire::protocol::play::Direction::Down:
        return "down";
    case wire::protocol::play::Direction::Up:
        return "up";
    case wire::protocol::play::Direction::South:
        return "south";
    case wire::protocol::play::Direction::West:
        return "west";
    case wire::protocol::play::Direction::East:
        return "east";
    case wire::protocol::play::Direction::North:
        break;
    }
    return "north";
}

} // namespace basalt::server::runtime::player
