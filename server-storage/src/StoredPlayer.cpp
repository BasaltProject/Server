#include <basalt/server/storage/StoredPlayer.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::storage {

StoredInventory StoredInventory::empty(std::int32_t size) {
    return StoredInventory{.slots = std::vector<std::optional<StoredItemStack>>(static_cast<std::size_t>(size))};
}

bool StoredInventory::isEmpty() const {
    return std::none_of(slots.begin(), slots.end(),
                        [](const std::optional<StoredItemStack>& slot) { return slot.has_value(); });
}

StoredGamemode gamemodeById(std::int32_t id) {
    if (id < 0 || id > 3) {
        return StoredGamemode::Survival;
    }
    return static_cast<StoredGamemode>(id);
}

std::int64_t StoredStats::get(std::string_view key) const {
    const auto found = counters.find(key);
    return found == counters.end() ? 0 : found->second;
}

StoredStats StoredStats::with(std::string_view key, std::int64_t amount) const {
    StoredStats built = *this;
    built.counters[std::string{key}] = get(key) + amount;
    return built;
}

std::string stats::custom(std::string_view name) { return std::string{kCustom} + "/minecraft:" + std::string{name}; }

std::string stats::mined(std::string_view block) { return std::string{kMined} + "/" + std::string{block}; }

namespace {

const std::string& named(std::string_view name) {
    // One string per counter, built once: these are compared and looked up, never rebuilt per tick.
    static std::map<std::string, std::string, std::less<>> built;
    const auto found = built.find(name);
    if (found != built.end()) {
        return found->second;
    }
    return built.emplace(std::string{name}, stats::custom(name)).first->second;
}

} // namespace

const std::string& stats::deaths() { return named("deaths"); }

const std::string& stats::playTime() { return named("play_time"); }

const std::string& stats::timeSinceRest() { return named("time_since_rest"); }

const std::string& stats::walkOneCm() { return named("walk_one_cm"); }

const std::string& stats::damageTaken() { return named("damage_taken"); }

const std::string& stats::damageDealt() { return named("damage_dealt"); }

const std::string& stats::mobKills() { return named("mob_kills"); }

const std::string& stats::jump() { return named("jump"); }

const std::string& stats::fallOneCm() { return named("fall_one_cm"); }

const std::string& stats::enchantItem() { return named("enchant_item"); }

const std::string& stats::animalsBred() { return named("animals_bred"); }

} // namespace basalt::server::storage
