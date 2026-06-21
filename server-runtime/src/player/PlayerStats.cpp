#include <basalt/server/runtime/player/PlayerStats.hpp>

#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::player {
namespace {

/// parity: `Float.roundToLong`.
[[nodiscard]] std::int64_t rounded(float value) {
    return static_cast<std::int64_t>(std::floor(static_cast<double>(value) + 0.5));
}

} // namespace

void PlayerStats::increment(std::string_view key, std::int64_t amount) {
    if (amount == 0 || (gate_ && !gate_(key, amount))) {
        return;
    }
    const std::lock_guard<std::mutex> guard{lock_};
    noteChange(key, amount, false);
    const auto found = counters_.find(key);
    if (found == counters_.end()) {
        counters_.emplace(std::string{key}, amount);
        return;
    }
    found->second += amount;
}

void PlayerStats::noteOnly(std::string_view key, std::int64_t amount) {
    const std::lock_guard<std::mutex> guard{lock_};
    noteChange(key, amount, false);
}

void PlayerStats::noteChange(std::string_view key, std::int64_t delta, bool reset) {
    auto found = changes_.find(key);
    if (found == changes_.end()) {
        found = changes_.emplace(std::string{key}, Change{}).first;
    }
    if (reset) {
        found->second = Change{.reset = true};
    }
    found->second.delta += delta;
}

std::map<std::string, PlayerStats::Change, std::less<>> PlayerStats::takeChanges() {
    const std::lock_guard<std::mutex> guard{lock_};
    return std::exchange(changes_, {});
}

std::int64_t PlayerStats::get(std::string_view key) const {
    const std::lock_guard<std::mutex> guard{lock_};
    const auto found = counters_.find(key);
    return found == counters_.end() ? 0 : found->second;
}

void PlayerStats::reset(std::string_view key) {
    const std::lock_guard<std::mutex> guard{lock_};
    noteChange(key, 0, true);
    const auto found = counters_.find(key);
    if (found != counters_.end()) {
        counters_.erase(found);
    }
}

void PlayerStats::addFallen(double blocks) {
    if (blocks <= 0.0) {
        return;
    }
    increment(storage::stats::fallOneCm(), static_cast<std::int64_t>(blocks * 100.0));
}

void PlayerStats::addDamageTaken(float amount) {
    // Tenths of a heart, matching vanilla's own scale for this counter.
    increment(storage::stats::damageTaken(), rounded(amount * 10.0F));
}

void PlayerStats::addDamageDealt(float amount) { increment(storage::stats::damageDealt(), rounded(amount * 10.0F)); }

storage::StoredStats PlayerStats::snapshot(std::int64_t playTimeTicks) const {
    const std::lock_guard<std::mutex> guard{lock_};
    storage::StoredStats stored{.counters = counters_};
    stored.counters.insert_or_assign(storage::stats::playTime(), playTimeTicks);
    return stored;
}

void PlayerStats::restore(const storage::StoredStats& stored) {
    const std::lock_guard<std::mutex> guard{lock_};
    counters_ = stored.counters;
    // Play time has its own authoritative column; a second copy here could disagree.
    const auto found = counters_.find(storage::stats::playTime());
    if (found != counters_.end()) {
        counters_.erase(found);
    }
}

} // namespace basalt::server::runtime::player
