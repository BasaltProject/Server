#pragma once

#include <basalt/server/runtime/player/FoodData.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

struct VitalsContext {
    Difficulty difficulty = Difficulty::Easy;
    bool naturalRegeneration = true;

    std::function<std::optional<std::int32_t>(const PlayerSession& player, std::int32_t now, std::int32_t before)>
        foodChanged;
};

void tickVitals(PlayerSession& player, const VitalsContext& context);

[[nodiscard]] Difficulty difficultyOf(std::string_view name);

} // namespace basalt::server::runtime::player
