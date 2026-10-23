#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerStats {
public:
    /// Asked before a counter moves; false leaves it where it is.
    using Gate = std::function<bool(std::string_view key, std::int64_t amount)>;

    void setGate(Gate gate) { gate_ = std::move(gate); }

    void increment(std::string_view key, std::int64_t amount = 1);

    [[nodiscard]] std::int64_t get(std::string_view key) const;

    void reset(std::string_view key);

    void noteOnly(std::string_view key, std::int64_t amount);

    void addFallen(double blocks);

    void addDamageTaken(float amount);

    void addDamageDealt(float amount);

    [[nodiscard]] storage::StoredStats snapshot(std::int64_t playTimeTicks) const;

    void restore(const storage::StoredStats& stored);

    struct Change {
        bool reset = false;
        std::int64_t delta = 0;
    };

    [[nodiscard]] std::map<std::string, Change, std::less<>> takeChanges();

private:
    void noteChange(std::string_view key, std::int64_t delta, bool reset);

    std::map<std::string, Change, std::less<>> changes_;

    Gate gate_;
    mutable std::mutex lock_;
    std::map<std::string, std::int64_t, std::less<>> counters_;

    /// Fractions of a block walked, kept until they add up to a whole centimetre.
};

} // namespace basalt::server::runtime::player
