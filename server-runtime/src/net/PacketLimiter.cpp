#include <basalt/server/runtime/net/PacketLimiter.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::net {

PacketLimiter::PacketLimiter(std::shared_ptr<const config::LimitsConfig> limits) : limits_(std::move(limits)) {
    if (limits_ != nullptr) {
        ruleCounts_.assign(limits_->packetRules.size(), 0);
    }
}

bool PacketLimiter::active() const noexcept {
    return limits_ != nullptr && (limits_->packetRateLimit > 0 || !limits_->packetRules.empty());
}

// parity: `Connection.tickSecond`, folded in lazily so an idle connection costs nothing.
void PacketLimiter::roll(Clock::time_point now) {
    if (!started_) {
        started_ = true;
        windowStart_ = now;
        return;
    }
    while (now - windowStart_ >= std::chrono::seconds{1}) {
        average_ = (0.25F * static_cast<float>(received_)) + (0.75F * average_);
        received_ = 0;
        std::ranges::fill(ruleCounts_, 0);
        windowStart_ += std::chrono::seconds{1};
        if (limits_->packetRateLimit > 0 && average_ > static_cast<float>(limits_->packetRateLimit)) {
            return;
        }
    }
}

PacketLimiter::Verdict PacketLimiter::receive(server::plugins::PacketPhase phase, std::int32_t id,
                                              Clock::time_point now) {
    roll(now);
    // parity: `RateKickingConnection.tickSecond`.
    if (limits_->packetRateLimit > 0 && average_ > static_cast<float>(limits_->packetRateLimit)) {
        return Verdict::Kick;
    }
    ++received_;
    for (std::size_t i = 0; i < limits_->packetRules.size(); ++i) {
        const config::PacketRule& rule = limits_->packetRules[i];
        if (rule.address.phase != phase || rule.address.id != id) {
            continue;
        }
        if (++ruleCounts_[i] > rule.maxPerSecond) {
            return rule.action == config::PacketRule::Action::Kick ? Verdict::Kick : Verdict::Drop;
        }
    }
    return Verdict::Allow;
}

} // namespace basalt::server::runtime::net
