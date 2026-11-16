#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Vaults.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::world::vaults {
namespace {

using wire::common::Uuid;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::array<std::string_view, 4> kNames{"inactive", "active", "unlocking", "ejecting"};
/// parity: `VaultServerData.MAX_REWARD_PLAYERS`.
constexpr std::size_t kMaxRewardedPlayers = 128;

[[nodiscard]] std::optional<Uuid> uuidOf(const NbtTag& tag) {
    const auto* ints = tag.get<std::vector<std::int32_t>>();
    if (ints == nullptr || ints->size() != 4) {
        return std::nullopt;
    }
    const auto part = [](std::int32_t value) { return static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)); };
    return Uuid{(part((*ints)[0]) << 32) | part((*ints)[1]), (part((*ints)[2]) << 32) | part((*ints)[3])};
}

[[nodiscard]] std::vector<Uuid> readUuids(const NbtCompound& data, std::string_view key) {
    std::vector<Uuid> out;
    if (const NbtList* list = data.getList(key); list != nullptr) {
        for (const NbtTag& tag : *list) {
            if (const std::optional<Uuid> id = uuidOf(tag);
                id.has_value() && std::ranges::find(out, *id) == out.end()) {
                out.push_back(*id);
            }
        }
    }
    return out;
}

void writeUuids(NbtCompound& data, std::string_view key, const std::vector<Uuid>& ids) {
    if (ids.empty()) {
        static_cast<void>(data.remove(key));
        return;
    }
    std::vector<NbtTag> tags;
    for (const Uuid& id : ids) {
        tags.push_back(
            NbtTag::ofIntArray({static_cast<std::int32_t>(id.high() >> 32), static_cast<std::int32_t>(id.high()),
                                static_cast<std::int32_t>(id.low() >> 32), static_cast<std::int32_t>(id.low())}));
    }
    data.set(key, NbtTag::ofList(NbtList{NbtType::IntArray, std::move(tags)}));
}

[[nodiscard]] std::optional<server::world::loot::ItemStack> readOptionalStack(const NbtCompound& data,
                                                                              std::string_view key) {
    const NbtCompound* stack = data.getCompound(key);
    if (stack == nullptr) {
        return std::nullopt;
    }
    std::optional<server::world::loot::ItemStack> read = blockInventory::readStack(*stack);
    return read.has_value() && read->count > 0 ? read : std::nullopt;
}

} // namespace

std::string_view nameOf(const State state) { return kNames[static_cast<std::size_t>(state)]; }

std::optional<State> stateByName(const std::string_view name) {
    for (std::size_t index = 0; index < kNames.size(); ++index) {
        if (kNames[index] == name) {
            return static_cast<State>(index);
        }
    }
    return std::nullopt;
}

Config Config::standard() {
    Config config;
    config.keyItem = server::world::loot::ItemStack{.item = wire::common::Identifier::vanilla("trial_key"), .count = 1};
    return config;
}

Config Config::read(const NbtCompound& entity) {
    const NbtCompound* stored = entity.getCompound("config");
    if (stored == nullptr) {
        return standard();
    }
    Config config = standard();
    if (stored->contains("loot_table")) {
        config.lootTable = std::string{stored->getString("loot_table")};
    }
    config.activationRange = stored->getDouble("activation_range", config.activationRange);
    config.deactivationRange = stored->getDouble("deactivation_range", config.deactivationRange);
    config.keyItem = readOptionalStack(*stored, "key_item");
    if (stored->contains("override_loot_table_to_display")) {
        config.displayTable = std::string{stored->getString("override_loot_table_to_display")};
    }
    // parity: `VaultConfig.validate` falls back to the default for an inverted pair.
    return config.activationRange > config.deactivationRange ? standard() : config;
}

ServerData ServerData::read(const NbtCompound& entity) {
    ServerData data;
    const NbtCompound* stored = entity.getCompound("server_data");
    if (stored == nullptr) {
        return data;
    }
    data.rewardedPlayers = readUuids(*stored, "rewarded_players");
    data.stateUpdatingResumesAt = stored->getLong("state_updating_resumes_at", 0);
    if (const NbtList* items = stored->getList("items_to_eject"); items != nullptr) {
        for (const NbtTag& tag : *items) {
            const NbtCompound* compound = tag.get<NbtCompound>();
            const std::optional<server::world::loot::ItemStack> stack =
                compound == nullptr ? std::nullopt : blockInventory::readStack(*compound);
            if (stack.has_value()) {
                data.itemsToEject.push_back(*stack);
            }
        }
    }
    data.totalEjectionsNeeded = stored->getInt("total_ejections_needed", 0);
    return data;
}

void ServerData::write(NbtCompound& entity) const {
    NbtCompound stored;
    writeUuids(stored, "rewarded_players", rewardedPlayers);
    if (stateUpdatingResumesAt != 0) {
        stored.putLong("state_updating_resumes_at", stateUpdatingResumesAt);
    }
    if (!itemsToEject.empty()) {
        NbtList items{NbtType::Compound};
        for (const server::world::loot::ItemStack& stack : itemsToEject) {
            items.push(NbtTag::ofCompound(blockInventory::writeStack(stack)));
        }
        stored.set("items_to_eject", NbtTag::ofList(std::move(items)));
    }
    if (totalEjectionsNeeded != 0) {
        stored.putInt("total_ejections_needed", totalEjectionsNeeded);
    }
    entity.set("server_data", NbtTag::ofCompound(std::move(stored)));
}

bool ServerData::hasRewarded(const Uuid& player) const {
    return std::ranges::find(rewardedPlayers, player) != rewardedPlayers.end();
}

void ServerData::addRewarded(const Uuid& player) {
    if (!hasRewarded(player)) {
        rewardedPlayers.push_back(player);
    }
    if (rewardedPlayers.size() > kMaxRewardedPlayers) {
        rewardedPlayers.erase(rewardedPlayers.begin());
    }
}

float ServerData::ejectionProgress() const {
    if (totalEjectionsNeeded == 1) {
        return 1.0F;
    }
    // parity: `Mth.inverseLerp(size, 1, total)`.
    const float size = static_cast<float>(itemsToEject.size());
    const float total = static_cast<float>(totalEjectionsNeeded);
    return 1.0F - ((size - 1.0F) / (total - 1.0F));
}

SharedData SharedData::read(const NbtCompound& entity) {
    SharedData data;
    const NbtCompound* stored = entity.getCompound("shared_data");
    if (stored == nullptr) {
        return data;
    }
    data.displayItem = readOptionalStack(*stored, "display_item");
    data.connectedPlayers = readUuids(*stored, "connected_players");
    data.connectedParticlesRange = stored->getDouble("connected_particles_range", data.connectedParticlesRange);
    return data;
}

namespace {

[[nodiscard]] NbtCompound sharedBody(const SharedData& data) {
    NbtCompound stored;
    if (data.displayItem.has_value()) {
        stored.set("display_item", NbtTag::ofCompound(blockInventory::writeStack(*data.displayItem)));
    }
    writeUuids(stored, "connected_players", data.connectedPlayers);
    if (data.connectedParticlesRange != Config::standard().deactivationRange) {
        stored.putDouble("connected_particles_range", data.connectedParticlesRange);
    }
    return stored;
}

} // namespace

void SharedData::write(NbtCompound& entity) const { entity.set("shared_data", NbtTag::ofCompound(sharedBody(*this))); }

NbtCompound SharedData::updateTag() const {
    NbtCompound tag;
    tag.set("shared_data", NbtTag::ofCompound(sharedBody(*this)));
    return tag;
}

bool SharedData::sameConnected(const std::vector<Uuid>& other) const {
    return other.size() == connectedPlayers.size() && std::ranges::all_of(other, [this](const Uuid& id) {
               return std::ranges::find(connectedPlayers, id) != connectedPlayers.end();
           });
}

} // namespace basalt::server::runtime::world::vaults
