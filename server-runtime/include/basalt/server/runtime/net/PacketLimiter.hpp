#pragma once

#include <basalt/server/runtime/config/LimitsConfig.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace basalt::server::runtime::net {

class PacketLimiter {
public:
    enum class Verdict : std::uint8_t { Allow, Drop, Kick };

    using Clock = std::chrono::steady_clock;

    PacketLimiter() = default;
    explicit PacketLimiter(std::shared_ptr<const config::LimitsConfig> limits);

    [[nodiscard]] bool active() const noexcept;

    [[nodiscard]] Verdict receive(server::plugins::PacketPhase phase, std::int32_t id, Clock::time_point now);

    /// parity: `Connection.getAverageReceivedPackets`.
    [[nodiscard]] float average() const noexcept { return average_; }

private:
    void roll(Clock::time_point now);

    std::shared_ptr<const config::LimitsConfig> limits_;
    std::vector<std::int32_t> ruleCounts_;
    Clock::time_point windowStart_{};
    bool started_ = false;
    std::int32_t received_ = 0;
    float average_ = 0.0F;
};

} // namespace basalt::server::runtime::net
