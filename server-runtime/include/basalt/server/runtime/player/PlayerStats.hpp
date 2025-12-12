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

    /// Adds a distance in blocks, counted in centimetres as vanilla does.
    ///
    /// The remainder is kept rather than rounded away: a player moves a few centimetres per tick,
    /// so rounding each one to the nearest centimetre would lose most of the distance at a walk and
    /// all of it when standing almost still.
    void addWalked(double blocks);

    /// Fallen distance, in centimetres like the walked one.
    void addFallen(double blocks);

    void addDamageTaken(float amount);

    void addDamageDealt(float amount);

    /// A snapshot for saving. `playTimeTicks` is kept in its own column and folded in here too.
    [[nodiscard]] storage::StoredStats snapshot(std::int64_t playTimeTicks) const;

    void restore(const storage::StoredStats& stored);

private:
    mutable std::mutex lock_;
    std::map<std::string, std::int64_t, std::less<>> counters_;

    /// Fractions of a block walked, kept until they add up to a whole centimetre.
    double walkRemainder_ = 0.0;
};

} // namespace basalt::server::runtime::player
