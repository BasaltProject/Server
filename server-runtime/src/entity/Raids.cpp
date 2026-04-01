#include <basalt/server/runtime/entity/Raids.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <memory>

namespace basalt::server::runtime::entity {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

using Counts = std::array<std::int32_t, 8>;

/// parity: `RaiderType`'s `spawnsPerWaveBeforeBonus`, in declaration order.
constexpr std::array<Counts, 5> kCounts{Counts{0, 0, 2, 0, 1, 4, 2, 5}, Counts{0, 0, 0, 0, 0, 1, 1, 2},
                                        Counts{0, 4, 3, 3, 4, 4, 4, 2}, Counts{0, 0, 0, 0, 3, 0, 0, 1},
                                        Counts{0, 0, 0, 1, 0, 1, 0, 2}};

/// parity: `SavedData`'s wrapper version, the target's world version.
constexpr std::int32_t kDataVersion = 5023;

constexpr std::array<std::string_view, 4> kStatusNames{"ongoing", "victory", "loss", "stopped"};

[[nodiscard]] std::vector<std::int32_t> intsOf(const wire::common::Uuid& uuid) {
    return {static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high() >> 32U)),
            static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high())),
            static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low() >> 32U)),
            static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low()))};
}

[[nodiscard]] std::uint64_t halfOf(std::int32_t high, std::int32_t low) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(high)) << 32U) | static_cast<std::uint32_t>(low);
}

[[nodiscard]] std::string pathFor(std::string_view worldRoot) { return std::string{worldRoot} + "/raids.nbt"; }

} // namespace

Raid::Raid(const RaidPos& center, std::int32_t numGroups, std::int64_t seed)
    : numGroups(numGroups), center(center), random(random::RandomSource::legacy(seed)),
      barId(wire::common::Uuid::random()), barName(wire::text::Component::translatable("event.minecraft.raid")) {}

std::int32_t Raid::totalRaidersAlive() const {
    std::int32_t total = 0;
    for (const auto& [wave, raiders] : groups) {
        total += static_cast<std::int32_t>(raiders.size());
    }
    return total;
}

void Raid::absorbOmen(std::int32_t amplifier) {
    raidOmenLevel = std::clamp(raidOmenLevel + amplifier + 1, 0, kMaxOmenLevel);
}

void Raid::stop() {
    if (isOver()) {
        endedAs = status;
    }
    active = false;
    status = RaidStatus::Stopped;
}

float Raid::enchantOdds() const noexcept {
    switch (raidOmenLevel) {
    case 2:
        return 0.1F;
    case 3:
        return 0.25F;
    case 4:
        return 0.5F;
    case 5:
        return 0.75F;
    default:
        return 0.0F;
    }
}

std::int32_t Raid::spawnsFor(RaiderType type, std::int32_t wave, std::string_view difficulty, bool bonusWave) {
    const std::int32_t base = raidWaves::countFor(type, bonusWave ? numGroups : wave);
    const bool easy = difficulty == "easy";
    const bool normal = difficulty == "normal";
    std::int32_t bonus = 0;
    switch (type) {
    case RaiderType::Vindicator:
    case RaiderType::Pillager:
        bonus = easy ? random.nextInt(2) : normal ? 1 : 2;
        break;
    case RaiderType::Evoker:
        return base;
    case RaiderType::Witch:
        if (easy || wave <= 2 || wave == 4) {
            return base;
        }
        bonus = 1;
        break;
    case RaiderType::Ravager:
        bonus = !easy && bonusWave ? 1 : 0;
        break;
    }
    return base + (bonus > 0 ? random.nextInt(bonus + 1) : 0);
}

bool Raid::has(std::int32_t entityId) const {
    return std::ranges::any_of(groups,
                               [entityId](const auto& group) { return std::ranges::contains(group.second, entityId); });
}

bool Raid::addWaveMob(std::int32_t wave, std::int32_t entityId) {
    std::vector<std::int32_t>& raiders = groups[wave];
    if (std::ranges::contains(raiders, entityId)) {
        return false;
    }
    raiders.push_back(entityId);
    return true;
}

bool Raid::removeRaider(std::int32_t wave, std::int32_t entityId) {
    const auto found = groups.find(wave);
    if (found == groups.end()) {
        return false;
    }
    return std::erase(found->second, entityId) > 0;
}

void Raid::encode(NbtCompound& tag) const {
    tag.putBoolean("started", started);
    tag.putBoolean("active", active);
    tag.putLong("ticks_active", ticksActive);
    tag.putInt("raid_omen_level", raidOmenLevel);
    tag.putInt("groups_spawned", groupsSpawned);
    tag.putInt("cooldown_ticks", cooldownTicks);
    tag.putInt("post_raid_ticks", postRaidTicks);
    tag.putFloat("total_health", totalHealth);
    tag.putInt("group_count", numGroups);
    tag.putString("status", std::string{kStatusNames[static_cast<std::size_t>(status)]});
    tag.putIntArray("center", {center[0], center[1], center[2]});
    std::vector<NbtTag> heroTags;
    for (const wire::common::Uuid& hero : heroes) {
        heroTags.push_back(NbtTag::ofIntArray(intsOf(hero)));
    }
    tag.set("heroes_of_the_village", NbtTag::ofList(NbtList{NbtType::IntArray, std::move(heroTags)}));
}

std::unique_ptr<Raid> Raid::decode(const NbtCompound& tag, std::int64_t seed) {
    const std::span<const std::int32_t> center = tag.getIntArray("center");
    if (center.size() != 3) {
        return nullptr;
    }
    auto raid = std::make_unique<Raid>(RaidPos{center[0], center[1], center[2]}, tag.getInt("group_count"), seed);
    raid->started = tag.getBoolean("started");
    raid->active = tag.getBoolean("active");
    raid->ticksActive = tag.getLong("ticks_active");
    raid->raidOmenLevel = tag.getInt("raid_omen_level");
    raid->groupsSpawned = tag.getInt("groups_spawned");
    raid->cooldownTicks = tag.getInt("cooldown_ticks");
    raid->postRaidTicks = tag.getInt("post_raid_ticks");
    raid->totalHealth = tag.getFloat("total_health");
    const auto status = std::ranges::find(kStatusNames, tag.getString("status"));
    raid->status = status == kStatusNames.end() ? RaidStatus::Ongoing
                                                : static_cast<RaidStatus>(std::distance(kStatusNames.begin(), status));
    if (const NbtList* heroes = tag.getList("heroes_of_the_village"); heroes != nullptr) {
        for (const NbtTag& hero : *heroes) {
            if (const auto* ints = hero.get<std::vector<std::int32_t>>(); ints != nullptr && ints->size() == 4) {
                raid->heroes.emplace_back(halfOf((*ints)[0], (*ints)[1]), halfOf((*ints)[2], (*ints)[3]));
            }
        }
    }
    return raid;
}

namespace raidWaves {

std::string_view typeName(RaiderType type) {
    static constexpr std::array<std::string_view, 5> kNames{"vindicator", "evoker", "pillager", "witch", "ravager"};
    return kNames[static_cast<std::size_t>(type)];
}

std::int32_t countFor(RaiderType type, std::int32_t wave) {
    const Counts& counts = kCounts[static_cast<std::size_t>(type)];
    return counts[static_cast<std::size_t>(std::clamp(wave, 0, static_cast<std::int32_t>(counts.size()) - 1))];
}

std::int32_t groupsFor(std::string_view difficulty) {
    if (difficulty == "peaceful") {
        return 0;
    }
    if (difficulty == "easy") {
        return 3;
    }
    if (difficulty == "hard") {
        return 7;
    }
    return 5;
}

} // namespace raidWaves

Raid* RaidManager::get(std::int32_t id) {
    const auto found = raids_.find(id);
    return found == raids_.end() ? nullptr : found->second.get();
}

std::optional<std::int32_t> RaidManager::idOf(const Raid* raid) const {
    for (const auto& [id, held] : raids_) {
        if (held.get() == raid) {
            return id;
        }
    }
    return std::nullopt;
}

Raid* RaidManager::nearby(const RaidPos& pos, std::int32_t maxDistanceSquared) {
    Raid* closest = nullptr;
    auto closestDistance = static_cast<double>(maxDistanceSquared);
    for (const std::int32_t id : tickOrder()) {
        Raid* raid = raids_.at(id).get();
        const auto dx = static_cast<double>(raid->center[0] - pos[0]);
        const auto dy = static_cast<double>(raid->center[1] - pos[1]);
        const auto dz = static_cast<double>(raid->center[2] - pos[2]);
        const double distance = (dx * dx) + (dy * dy) + (dz * dz);
        if (raid->active && distance < closestDistance) {
            closest = raid;
            closestDistance = distance;
        }
    }
    return closest;
}

std::int32_t RaidManager::add(std::unique_ptr<Raid> raid) {
    const std::int32_t id = ++nextId_;
    raids_.insert_or_assign(id, std::move(raid));
    order_.insert(id);
    dirty = true;
    return id;
}

void RaidManager::remove(std::int32_t id) {
    raids_.erase(id);
    order_.remove(id);
    dirty = true;
}

void RaidManager::forEachInTickOrder(const std::function<bool(std::int32_t id, Raid& raid)>& visit) {
    order_.iterate([&](std::int32_t id) {
        if (!visit(id, *raids_.at(id))) {
            return false;
        }
        raids_.erase(id);
        dirty = true;
        return true;
    });
}

void RaidManager::encode(NbtCompound& tag) const {
    std::vector<NbtTag> raids;
    for (const std::int32_t id : tickOrder()) {
        NbtCompound entry;
        entry.putInt("id", id);
        raids_.at(id)->encode(entry);
        raids.push_back(NbtTag::ofCompound(std::move(entry)));
    }
    tag.set("raids", NbtTag::ofList(NbtList{NbtType::Compound, std::move(raids)}));
    tag.putInt("next_id", nextId_);
    tag.putInt("tick", tick);
}

void RaidManager::decode(const NbtCompound& tag) {
    raids_.clear();
    order_ = IntHashOrder{};
    nextId_ = tag.getInt("next_id", 1);
    tick = tag.getInt("tick");
    if (const NbtList* raids = tag.getList("raids"); raids != nullptr) {
        for (const NbtTag& entry : *raids) {
            const auto* compound = entry.get<NbtCompound>();
            if (compound == nullptr) {
                continue;
            }
            if (std::unique_ptr<Raid> raid = Raid::decode(*compound, ++seeds_); raid != nullptr) {
                raids_.insert_or_assign(compound->getInt("id"), std::move(raid));
                order_.insert(compound->getInt("id"));
            }
        }
    }
}

void RaidManager::load(std::string_view worldRoot) {
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(pathFor(worldRoot));
    if (!bytes.has_value()) {
        return;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*bytes);
    const NbtCompound* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    const NbtCompound* data = root == nullptr ? nullptr : root->getCompound("data");
    if (data != nullptr) {
        decode(*data);
    }
    dirty = false;
}

void RaidManager::save(std::string_view worldRoot) {
    if (!dirty) {
        return;
    }
    NbtCompound data;
    encode(data);
    NbtCompound root;
    root.set("data", NbtTag::ofCompound(std::move(data)));
    root.putInt("DataVersion", kDataVersion);
    static_cast<void>(
        wire::io::fileIO::writeBytes(pathFor(worldRoot), wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(root)))));
    dirty = false;
}

} // namespace basalt::server::runtime::entity
