#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/Vaults.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster;

/// parity: `VaultBlockEntity.Server` with `VaultState`, `VaultServerData` and `VaultSharedData`.
class VaultSystem {
public:
    using Pos = std::array<std::int32_t, 3>;
    using Loot = std::vector<server::world::loot::ItemStack>;

    struct Hooks {
        /// parity: `BlockDispenseLootEvent`; false when a listener cancelled the dispensing.
        std::function<bool(const player::PlayerSession& who, world::LoadedWorld& world, const Pos& pos,
                           const std::string& table, Loot& loot)>
            dispenseLoot;
        /// parity: `VaultDisplayItemEvent`; false when a listener cancelled the new display item.
        std::function<bool(world::LoadedWorld& world, const Pos& pos,
                           std::optional<server::world::loot::ItemStack>& item)>
            displayItem;
        /// parity: `VaultChangeStateEvent`; false when a listener cancelled the change.
        std::function<bool(world::LoadedWorld& world, const Pos& pos, const player::PlayerSession* who,
                           world::vaults::State from, world::vaults::State to)>
            changeState;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        world::RealLootTables* lootTables = nullptr;
    };

    VaultSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, Hooks hooks, Options options);

    /// parity: `VaultBlockEntity.Server.tick`; true when the block entity changed.
    bool tick(world::LoadedWorld& world, const Pos& pos);

    /// parity: `VaultBlockEntity.Server.tryInsertKey`; how many of the held key were spent.
    [[nodiscard]] std::int32_t tryInsertKey(world::LoadedWorld& world, player::PlayerSession& player, const Pos& pos,
                                            const server::world::loot::ItemStack& held);

    /// parity: `VaultBlock.useItemOn`, which a held item answers only while the vault is active.
    [[nodiscard]] bool isActive(world::LoadedWorld& world, const Pos& pos) const;

private:
    struct Turn;

    [[nodiscard]] world::vaults::State step(Turn& turn, world::vaults::State current);

    [[nodiscard]] world::vaults::State settle(Turn& turn, double range);

    void updateConnected(Turn& turn, double range);

    void cycleDisplayItem(Turn& turn, world::vaults::State state);

    void changeState(Turn& turn, world::vaults::State to, const player::PlayerSession* who);

    void onEnter(Turn& turn, world::vaults::State state);

    void eject(Turn& turn, const server::world::loot::ItemStack& stack, float progress);

    [[nodiscard]] Loot roll(Turn& turn, const std::string& table, const player::PlayerSession* who,
                            const server::world::loot::ItemStack* key);

    void failSound(Turn& turn, const char* name);

    void soundAt(Turn& turn, const char* name, float volume, float pitch) const;

    void levelEvent(Turn& turn, std::int32_t type, std::int32_t data) const;

    void sendUpdate(Turn& turn) const;

    bool commit(Turn& turn, const world::vaults::ServerData& serverBefore,
                const world::vaults::SharedData& sharedBefore);

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    Hooks hooks_;
    Options options_;
    std::map<std::pair<std::string, Pos>, std::int64_t> lastInsertFail_;
};

} // namespace basalt::server::runtime
