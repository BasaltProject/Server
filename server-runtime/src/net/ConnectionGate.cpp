#include <basalt/server/runtime/net/ConnectionGate.hpp>

#include <optional>
#include <thread>
#include <utility>

namespace basalt::server::runtime::net {

ConnectionGate::Slot& ConnectionGate::Slot::operator=(Slot&& other) noexcept {
    if (this != &other) {
        if (gate_ != nullptr) {
            gate_->release(address_);
        }
        gate_ = std::exchange(other.gate_, nullptr);
        address_ = std::move(other.address_);
    }
    return *this;
}

ConnectionGate::Slot::~Slot() {
    if (gate_ != nullptr) {
        gate_->release(address_);
    }
}

std::optional<ConnectionGate::Slot> ConnectionGate::open(const std::string& address, std::int32_t maxPerAddress) {
    const std::lock_guard<std::mutex> lock{mutex_};
    std::int32_t& count = open_[address];
    if (maxPerAddress > 0 && count >= maxPerAddress) {
        if (count == 0) {
            open_.erase(address);
        }
        return std::nullopt;
    }
    ++count;
    return Slot{this, address};
}

void ConnectionGate::release(const std::string& address) {
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto found = open_.find(address);
    if (found != open_.end() && --found->second <= 0) {
        open_.erase(found);
    }
}

// parity: Paper `max-joins-per-tick`, a tick being fifty milliseconds here.
void ConnectionGate::awaitJoinSlot(std::int32_t perTick) {
    constexpr std::chrono::milliseconds kWindow{50};
    while (perTick > 0) {
        std::chrono::steady_clock::time_point wakeAt;
        {
            const std::lock_guard<std::mutex> lock{mutex_};
            const auto now = std::chrono::steady_clock::now();
            if (now - joinWindow_ >= kWindow) {
                joinWindow_ = now;
                joinsInWindow_ = 0;
            }
            if (joinsInWindow_ < perTick) {
                ++joinsInWindow_;
                return;
            }
            wakeAt = joinWindow_ + kWindow;
        }
        std::this_thread::sleep_until(wakeAt);
    }
}

// parity: CraftBukkit `connection-throttle`, which refreshes the stamp even when it refuses.
bool ConnectionGate::throttleLogin(const std::string& address, std::int32_t throttleMs,
                                   std::chrono::steady_clock::time_point now) {
    if (throttleMs <= 0 || address == "127.0.0.1" || address == "::1" || address == "0:0:0:0:0:0:0:1") {
        return false;
    }
    const std::lock_guard<std::mutex> lock{mutex_};
    if (now - lastPrune_ >= std::chrono::minutes(1)) {
        lastPrune_ = now;
        for (auto it = lastLogin_.begin(); it != lastLogin_.end();) {
            it = now - it->second > std::chrono::minutes(10) ? lastLogin_.erase(it) : std::next(it);
        }
    }
    const auto found = lastLogin_.find(address);
    const bool throttled = found != lastLogin_.end() && now - found->second < std::chrono::milliseconds(throttleMs);
    lastLogin_[address] = now;
    return throttled;
}

} // namespace basalt::server::runtime::net
