#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace basalt::server::runtime::net {

class ConnectionGate {
public:
    class Slot {
    public:
        Slot() = default;

        Slot(ConnectionGate* gate, std::string address) : gate_(gate), address_(std::move(address)) {}

        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;

        Slot(Slot&& other) noexcept : gate_(std::exchange(other.gate_, nullptr)), address_(std::move(other.address_)) {}

        Slot& operator=(Slot&& other) noexcept;
        ~Slot();

    private:
        ConnectionGate* gate_ = nullptr;
        std::string address_;
    };

    /// Counts one more open connection from `address`; the slot frees it. Refused when `maxPerAddress` is reached.
    [[nodiscard]] std::optional<Slot> open(const std::string& address, std::int32_t maxPerAddress);

    /// True when the address logged in less than `throttleMs` ago; loopback is never throttled.
    [[nodiscard]] bool throttleLogin(const std::string& address, std::int32_t throttleMs,
                                     std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

private:
    void release(const std::string& address);

    std::mutex mutex_;
    std::map<std::string, std::int32_t, std::less<>> open_;
    std::map<std::string, std::chrono::steady_clock::time_point, std::less<>> lastLogin_;
};

} // namespace basalt::server::runtime::net
