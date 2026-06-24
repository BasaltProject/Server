#pragma once

#include <basalt/server/runtime/player/PlayerCrossbow.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime {

class ClientSession;

class ItemUseTimers {
public:
    struct Hooks {
        std::function<bool(const player::PlayerSession& player, const wire::common::Identifier& item)> consumeAllowed;
        std::function<bool(const player::PlayerSession& player, const wire::common::Identifier& projectile,
                           float power)>
            shootAllowed;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        std::function<float()> nextFloat;
    };

    explicit ItemUseTimers(ClientSession& owner, Hooks hooks = {}, Options options = {});

    bool startEating(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    void stopEating();

    void tickEating(player::PlayerSession& session);

    bool startDrawing(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    void stopDrawing();

    void tickDrawing(player::PlayerSession& session);

    void releaseBow();

    bool startBlocking(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    void stopBlocking();

    void tickBlocking(player::PlayerSession& session);

    bool throwEnderPearl(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    bool startSpyglass(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);
    void stopSpyglass();
    void tickSpyglass(player::PlayerSession& session);

    void tickUsingTrigger(player::PlayerSession& session);

    [[nodiscard]] bool onCooldown(const wire::common::Identifier& item);

    void startCooldown(const wire::common::Identifier& item);

    void setUsingItem(bool usingItem, std::optional<wire::common::Identifier> item = std::nullopt,
                      bool offhand = false);

    [[nodiscard]] static float bowPower(std::int32_t ticks);

    struct UseState {
        std::int32_t used = 0;
        std::int32_t duration = 0;
    };

    [[nodiscard]] std::optional<UseState> useState() const;
    bool setRemaining(std::int32_t ticks);
    void completeUse(player::PlayerSession& session);
    void clearUse();
    bool startUse(player::PlayerSession& session, std::int32_t slot);

private:
    ClientSession* owner_;
    Hooks hooks_;
    Options options_;
    void raiseUseVibration(std::string_view event, const wire::common::Identifier& item);

    void emitAt(std::string_view event);

    void playAroundExceptSelf(const player::PlayerSession& session, std::string_view sound);

    bool usingItem_ = false;
    std::optional<wire::common::Identifier> usedItem_;
    std::int32_t eatingSlot_ = -1;
    std::int32_t spyglassSlot_ = -1;
    std::int32_t spyglassTicks_ = 0;
    std::int32_t eatingTicks_ = 0;
    std::int32_t eatingDuration_ = 0;
    std::int32_t drawingSlot_ = -1;
    std::int32_t drawingTicks_ = 0;
    player::crossbow::Charging crossbowCharging_;
};

} // namespace basalt::server::runtime
