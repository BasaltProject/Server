#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace basalt::server::runtime::player {

/// The counters of one logged-in player, behind a lock because this connection's loop and whatever
/// thread ran a command both touch them: a statistic that loses increments to a race is worse than
/// useless, and the lock is uncontended anyway.
///
/// Only what this build can observe is counted.
class PlayerStats {
public:
    void increment(std::string_view key, std::int64_t amount = 1);

    [[nodiscard]] std::int64_t get(std::string_view key) const;

    /// parity: `ServerStatsCounter.setValue`, used when sleep or death resets a timer stat.
    void reset(std::string_view key);

    /// Fallen distance, in centimetres like the walked one.
    void addFallen(double blocks);

    void addDamageTaken(float amount);

    void addDamageDealt(float amount);

    /// A snapshot for saving. `playTimeTicks` is kept in its own column and folded in here too.
    [[nodiscard]] storage::StoredStats snapshot(std::int64_t playTimeTicks) const;

    void restore(const storage::StoredStats& stored);

    /// What changed since the last call, for the objectives that count a statistic. `reset` means
    /// the counter went back to zero before `delta` was added.
    struct Change {
        bool reset = false;
        std::int64_t delta = 0;
    };

    [[nodiscard]] std::map<std::string, Change, std::less<>> takeChanges();

private:
    void noteChange(std::string_view key, std::int64_t delta, bool reset);

    std::map<std::string, Change, std::less<>> changes_;

    mutable std::mutex lock_;
    std::map<std::string, std::int64_t, std::less<>> counters_;

    /// Fractions of a block walked, kept until they add up to a whole centimetre.
};

} // namespace basalt::server::runtime::player
