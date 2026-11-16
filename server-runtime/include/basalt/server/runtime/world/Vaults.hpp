#pragma once

#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::vaults {

/// parity: `VaultState`.
enum class State : std::uint8_t { Inactive, Active, Unlocking, Ejecting };

[[nodiscard]] std::string_view nameOf(State state);

[[nodiscard]] std::optional<State> stateByName(std::string_view name);

/// parity: `VaultConfig`, with an absent `key_item` meaning no key at all.
struct Config {
    std::string lootTable = "minecraft:chests/trial_chambers/reward";
    double activationRange = 4.0;
    double deactivationRange = 4.5;
    std::optional<server::world::loot::ItemStack> keyItem;
    std::optional<std::string> displayTable;

    [[nodiscard]] static Config standard();

    [[nodiscard]] static Config read(const wire::nbt::NbtCompound& entity);
};

/// parity: `VaultServerData`.
struct ServerData {
    std::vector<wire::common::Uuid> rewardedPlayers;
    std::int64_t stateUpdatingResumesAt = 0;
    std::vector<server::world::loot::ItemStack> itemsToEject;
    std::int32_t totalEjectionsNeeded = 0;

    [[nodiscard]] static ServerData read(const wire::nbt::NbtCompound& entity);

    void write(wire::nbt::NbtCompound& entity) const;

    [[nodiscard]] bool hasRewarded(const wire::common::Uuid& player) const;

    void addRewarded(const wire::common::Uuid& player);

    /// parity: `VaultServerData.ejectionProgress`.
    [[nodiscard]] float ejectionProgress() const;

    friend bool operator==(const ServerData&, const ServerData&) = default;
};

/// parity: `VaultSharedData`, the part the client is sent.
struct SharedData {
    std::optional<server::world::loot::ItemStack> displayItem;
    std::vector<wire::common::Uuid> connectedPlayers;
    double connectedParticlesRange = 4.5;

    [[nodiscard]] static SharedData read(const wire::nbt::NbtCompound& entity);

    void write(wire::nbt::NbtCompound& entity) const;

    /// parity: `VaultBlockEntity.getUpdateTag`.
    [[nodiscard]] wire::nbt::NbtCompound updateTag() const;

    /// parity: `updateConnectedPlayersWithinRange`'s set comparison.
    [[nodiscard]] bool sameConnected(const std::vector<wire::common::Uuid>& other) const;

    friend bool operator==(const SharedData&, const SharedData&) = default;
};

} // namespace basalt::server::runtime::world::vaults
