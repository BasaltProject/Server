#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::player {

struct ClientOptions {
    std::string locale = "en_us";
    std::int32_t viewDistance = 10;
    std::int32_t particleStatus = 0;
    bool chatColors = true;
    bool textFiltering = false;
    bool serverListings = true;
};

struct PlayerExtras {
    static constexpr float kDefaultFlySpeed = 0.05F;
    static constexpr float kDefaultWalkSpeed = 0.1F;

    std::optional<bool> allowFlight;
    float flySpeed = kDefaultFlySpeed;
    float walkSpeed = kDefaultWalkSpeed;
    std::optional<float> healthScale;

    std::optional<std::int64_t> timeOverride;
    bool timeRelative = false;
    std::optional<float> rainOverride;
    float thunderOverride = 0.0F;

    std::set<std::int32_t> hiddenEntities;
    bool hiddenByDefault = false;
    std::set<std::int32_t> shownEntities;
    std::optional<std::int32_t> spectatorTarget;
    std::optional<std::array<std::int32_t, 3>> compassTarget;

    std::optional<std::string> tabHeader;
    std::optional<std::string> tabFooter;
    std::optional<std::string> listName;
    bool unlisted = false;
    std::int32_t listOrder = 0;
    bool listDirty = false;

    ClientOptions client;
    std::string brand;
    std::int32_t pingMillis = 0;
    std::int32_t packStatus = -1;
    std::int64_t lastActionMillis = 0;
    std::int32_t inputFlags = 0;
    std::optional<std::int32_t> viewDistance;
    std::optional<std::int32_t> simulationDistance;
    std::int32_t previousGamemode = -1;
    std::set<std::int32_t> unlistedPlayers;

    bool seenWinScreen = false;
    bool sleepingIgnored = false;
    bool collidable = true;
    bool affectsSpawning = true;
    bool flyingFallDamage = false;
    std::int32_t expCooldown = 0;
    std::int32_t arrowCooldown = 0;
    std::int32_t noActionTicks = 0;
    std::int32_t pendingDeathScore = 0;

    std::optional<std::array<double, 4>> respawnAt;
    std::string respawnWorld;
    bool respawnForced = false;

    std::map<std::string, std::string, std::less<>> state;
};

} // namespace basalt::server::runtime::player
