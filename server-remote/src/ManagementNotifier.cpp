#include "ManagementDto.hpp"

#include <basalt/server/remote/ManagementNotifier.hpp>
#include <basalt/server/remote/ManagementRpc.hpp>

namespace basalt::server::remote {
namespace {

using wire::common::JsonValue;

constexpr std::chrono::seconds kActivityInterval{30};

} // namespace

void ManagementNotifier::attach(Sink sink) {
    const std::lock_guard guard{lock_};
    sink_ = std::move(sink);
    active_.store(static_cast<bool>(sink_), std::memory_order_relaxed);
}

void ManagementNotifier::detach() {
    const std::lock_guard guard{lock_};
    sink_ = nullptr;
    active_.store(false, std::memory_order_relaxed);
}

void ManagementNotifier::send(const std::string_view method, const JsonValue* param) const {
    if (!listening()) {
        return;
    }
    Sink sink;
    {
        const std::lock_guard guard{lock_};
        sink = sink_;
    }
    if (sink) {
        sink(managementRpc::notificationFrame(method, param));
    }
}

void ManagementNotifier::serverStarted() const { send("server/started", nullptr); }

void ManagementNotifier::serverStopping() const { send("server/stopping", nullptr); }

void ManagementNotifier::saveStarted() const { send("server/saving", nullptr); }

void ManagementNotifier::saveCompleted() const { send("server/saved", nullptr); }

void ManagementNotifier::activity() {
    if (!listening()) {
        return;
    }
    {
        const std::lock_guard guard{lock_};
        const auto now = std::chrono::steady_clock::now();
        if (lastActivity_ != std::chrono::steady_clock::time_point{} && now - lastActivity_ < kActivityInterval) {
            return;
        }
        lastActivity_ = now;
    }
    send("server/activity", nullptr);
}

void ManagementNotifier::playerJoined(const ManagedPlayer& player) const {
    const JsonValue param = managementDto::encodePlayer(player);
    send("players/joined", &param);
}

void ManagementNotifier::playerLeft(const ManagedPlayer& player) const {
    const JsonValue param = managementDto::encodePlayer(player);
    send("players/left", &param);
}

void ManagementNotifier::operatorAdded(const ManagedOperator& entry) const {
    const JsonValue param = managementDto::encodeOperator(entry);
    send("operators/added", &param);
}

void ManagementNotifier::operatorRemoved(const ManagedOperator& entry) const {
    const JsonValue param = managementDto::encodeOperator(entry);
    send("operators/removed", &param);
}

void ManagementNotifier::allowlistAdded(const ManagedPlayer& player) const {
    const JsonValue param = managementDto::encodePlayer(player);
    send("allowlist/added", &param);
}

void ManagementNotifier::allowlistRemoved(const ManagedPlayer& player) const {
    const JsonValue param = managementDto::encodePlayer(player);
    send("allowlist/removed", &param);
}

void ManagementNotifier::ipBanned(const ManagedIpBan& ban) const {
    const JsonValue param = managementDto::encodeIpBan(ban);
    send("ip_bans/added", &param);
}

void ManagementNotifier::ipUnbanned(const std::string_view ip) const {
    const JsonValue param = JsonValue::of(std::string{ip});
    send("ip_bans/removed", &param);
}

void ManagementNotifier::banned(const ManagedUserBan& ban) const {
    const JsonValue param = managementDto::encodeUserBan(ban);
    send("bans/added", &param);
}

void ManagementNotifier::unbanned(const ManagedPlayer& player) const {
    const JsonValue param = managementDto::encodePlayer(player);
    send("bans/removed", &param);
}

void ManagementNotifier::gameRuleChanged(const ManagedGameRule& rule) const {
    const JsonValue param = managementDto::encodeTypedRule(rule);
    send("gamerules/updated", &param);
}

void ManagementNotifier::heartbeat(const bool started, const std::vector<ManagedPlayer>& players,
                                   const std::string_view versionName, const std::int32_t protocol) const {
    const JsonValue param = managementDto::encodeServerState(started, players, versionName, protocol);
    send("server/status", &param);
}

} // namespace basalt::server::remote
