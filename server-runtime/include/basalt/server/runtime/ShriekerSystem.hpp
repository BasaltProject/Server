#pragma once

#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;

namespace vibrations {
struct ShriekerArrival;
}
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class MobActionSystem;
class NaturalSpawningSystem;

/// The sculk shrieker's turn in the world: the shriek a player sets off, the darkness and the
/// warning it answers with, and the warden at the fourth warning.
///
/// parity: `SculkShriekerBlockEntity.tryShriek`, `shriek` and `tryRespond`, with `SculkShriekerBlock`.
class ShriekerSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    ShriekerSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                   NaturalSpawningSystem& spawning, MobActionSystem& mobs, Options options);

    /// parity: `SculkShriekerBlock.stepOn`, a player walking over one.
    void steppedOn(world::LoadedWorld& world, player::PlayerSession& player, std::int32_t x, std::int32_t y,
                   std::int32_t z);

    /// The player behind a noise's source, if any.
    ///
    /// parity: `SculkShriekerBlockEntity.tryGetPlayer`: the player, whoever steers the entity, or
    /// a projectile's owner. A thrown item remembers no thrower here, so it never counts.
    [[nodiscard]] static player::PlayerSession* playerBehind(const world::LoadedWorld& world,
                                                             const player::PlayerRegistry& players,
                                                             const wire::common::Uuid& source);

    /// A noise that travelled to a shrieker. parity: `VibrationUser.onReceiveVibration`.
    void heard(world::LoadedWorld& world, const world::vibrations::ShriekerArrival& arrival);

    /// parity: `SculkShriekerBlockEntity.tryShriek`.
    void tryShriek(world::LoadedWorld& world, player::PlayerSession& player, std::int32_t x, std::int32_t y,
                   std::int32_t z);

    /// The shriek's booked turn. parity: `SculkShriekerBlock.tick`.
    void scheduledTick(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                       std::int32_t y, std::int32_t z);

    /// Every player's warning counts down. parity: `ServerPlayer.tick`'s `wardenSpawnTracker.tick`.
    void tickTrackers(const world::LoadedWorld& world);

private:
    [[nodiscard]] bool canRespond(const world::LoadedWorld& world, std::int32_t stateId) const;

    [[nodiscard]] bool tryToWarn(world::LoadedWorld& world, player::PlayerSession& player, std::int32_t x,
                                 std::int32_t y, std::int32_t z);

    void shriek(world::LoadedWorld& world, const player::PlayerSession& player, std::int32_t x, std::int32_t y,
                std::int32_t z);

    void tryRespond(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] bool trySummonWarden(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    NaturalSpawningSystem* spawning_;
    MobActionSystem* mobs_;
    Options options_;
};

} // namespace basalt::server::runtime
