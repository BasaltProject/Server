#pragma once

#include <basalt/server/remote/ManagementBackend.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::remote {

/// Turns what happens on the server into management notifications; callable from any thread.
class ManagementNotifier {
public:
    using Sink = std::function<void(std::string frame)>;

    void attach(Sink sink);
    void detach();

    [[nodiscard]] bool listening() const noexcept { return active_.load(std::memory_order_relaxed); }

    void serverStarted() const;
    void serverStopping() const;
    void saveStarted() const;
    void saveCompleted() const;
    void activity();
    void playerJoined(const ManagedPlayer& player) const;
    void playerLeft(const ManagedPlayer& player) const;
    void operatorAdded(const ManagedOperator& entry) const;
    void operatorRemoved(const ManagedOperator& entry) const;
    void allowlistAdded(const ManagedPlayer& player) const;
    void allowlistRemoved(const ManagedPlayer& player) const;
    void ipBanned(const ManagedIpBan& ban) const;
    void ipUnbanned(std::string_view ip) const;
    void banned(const ManagedUserBan& ban) const;
    void unbanned(const ManagedPlayer& player) const;
    void gameRuleChanged(const ManagedGameRule& rule) const;
    void heartbeat(bool started, const std::vector<ManagedPlayer>& players, std::string_view versionName,
                   std::int32_t protocol) const;

private:
    void send(std::string_view method, const wire::common::JsonValue* param) const;

    mutable std::mutex lock_;
    Sink sink_;
    std::atomic<bool> active_{false};
    std::chrono::steady_clock::time_point lastActivity_{};
};

} // namespace basalt::server::remote
