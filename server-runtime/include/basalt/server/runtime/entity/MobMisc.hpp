#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobMisc {

/// What a plugin may hold on a single mob beyond the typed state vanilla keeps for it.
struct State {
    bool creeperIgnited = false;
    std::optional<std::int32_t> igniter;
    std::optional<std::int32_t> wololoTarget;
    bool polarStanding = false;
    std::int32_t polarWarning = 0;
    bool striderCold = false;
    bool turtleLaying = false;
    bool turtleGoingHome = false;
    bool partyParrot = false;
    std::optional<std::array<std::int32_t, 3>> parrotJukebox;
    bool canUsePortals = false;
    bool cubeWanders = true;
    std::int32_t boostTotal = 0;
    std::int32_t boostTime = 0;
    bool boosting = false;
    bool witchDrinking = false;
    std::int32_t witchDrinkTicks = 0;
    std::optional<std::int32_t> schoolLeader;
    std::int32_t schoolSize = 1;
    bool sulfurExplosive = false;
    bool sulfurBody = false;
    bool sulfurFuseSounded = false;
    bool turtleHasEgg = false;
    std::optional<std::array<std::int32_t, 3>> turtleHome;
    std::optional<std::array<std::int32_t, 3>> turtleTravel;
    std::int32_t turtleLayCounter = 0;
    bool traderNoPotion = false;
    bool traderNoMilk = false;
};

using Lookup = std::function<ServerEntity*(std::int32_t entityId)>;

/// parity: `AbstractSchoolingFish.getMaxSchoolSize`.
[[nodiscard]] std::int32_t maxSchoolSize(const ServerEntity& fish);

[[nodiscard]] bool isSchooling(const ServerEntity& fish);

/// parity: `AbstractSchoolingFish.isFollower`, the leader still alive.
[[nodiscard]] bool isFollower(const ServerEntity& fish, const Lookup& lookup);

[[nodiscard]] bool hasFollowers(const ServerEntity& fish);

[[nodiscard]] bool canBeFollowed(const ServerEntity& fish);

void startFollowing(ServerEntity& fish, ServerEntity& leader);

void stopFollowing(ServerEntity& fish, const Lookup& lookup);

[[nodiscard]] State& of(ServerEntity& mob);

[[nodiscard]] const State* peek(const ServerEntity& mob);

} // namespace mobMisc

} // namespace basalt::server::runtime::entity
