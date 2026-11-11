#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::remote {

struct ManagedPlayer {
    std::optional<std::string> id;
    std::optional<std::string> name;
};

struct ManagedMessage {
    std::optional<std::string> literal;
    std::optional<std::string> translatable;
    std::optional<std::vector<std::string>> translatableParams;
};

struct ManagedUserBan {
    ManagedPlayer player;
    std::optional<std::string> reason;
    std::optional<std::string> source;
    std::optional<std::int64_t> expiresEpochSeconds;
};

struct ManagedIpBan {
    std::string ip;
    std::optional<std::string> reason;
    std::optional<std::string> source;
    std::optional<std::int64_t> expiresEpochSeconds;
};

struct ManagedOperator {
    ManagedPlayer player;
    std::optional<std::int32_t> permissionLevel;
    std::optional<bool> bypassesPlayerLimit;
};

struct ManagedGameRule {
    std::string key;
    bool boolean = false;
    bool flag = false;
    std::int64_t number = 0;
};

/// What the management protocol needs from the server; every method runs on the server thread.
class ManagementBackend {
public:
    ManagementBackend() = default;
    ManagementBackend(const ManagementBackend&) = delete;
    ManagementBackend& operator=(const ManagementBackend&) = delete;
    virtual ~ManagementBackend() = default;

    [[nodiscard]] virtual bool ready() = 0;
    [[nodiscard]] virtual std::string versionName() = 0;
    [[nodiscard]] virtual std::int32_t protocolVersion() = 0;

    [[nodiscard]] virtual std::vector<ManagedPlayer> onlinePlayers() = 0;
    /// Fills both the id and the name of a player known by either one; empty when nobody can be found.
    [[nodiscard]] virtual std::optional<ManagedPlayer> resolveUser(const ManagedPlayer& hint) = 0;

    [[nodiscard]] virtual std::vector<ManagedPlayer> allowlist() = 0;
    virtual void allowlistAdd(const ManagedPlayer& player) = 0;
    virtual void allowlistRemove(const ManagedPlayer& player) = 0;
    virtual void kickUnlisted() = 0;

    [[nodiscard]] virtual std::vector<ManagedUserBan> userBans() = 0;
    virtual void banUser(const ManagedUserBan& ban) = 0;
    virtual void unbanUser(const ManagedPlayer& player) = 0;

    [[nodiscard]] virtual std::vector<ManagedIpBan> ipBans() = 0;
    virtual void banIp(const ManagedIpBan& ban) = 0;
    virtual void unbanIp(std::string_view ip) = 0;
    [[nodiscard]] virtual std::optional<std::string> addressOf(const ManagedPlayer& player) = 0;

    [[nodiscard]] virtual std::vector<ManagedOperator> operators() = 0;
    virtual void op(const ManagedPlayer& player, std::optional<std::int32_t> level, std::optional<bool> bypass) = 0;
    virtual void deop(const ManagedPlayer& player) = 0;

    virtual bool kick(const ManagedPlayer& player, const std::optional<ManagedMessage>& reason) = 0;
    virtual bool save(bool flush) = 0;
    virtual void stop() = 0;
    /// Sends the status heartbeat notification.
    virtual void heartbeat() = 0;
    /// Called about once a second while a client listens, to notice joins, leaves and game rule changes.
    virtual void poll() = 0;
    virtual bool systemMessage(const ManagedMessage& message, bool overlay,
                               const std::optional<std::vector<ManagedPlayer>>& receivers) = 0;

    [[nodiscard]] virtual std::vector<ManagedGameRule> gameRules() = 0;
    /// Sets one rule; the error names the problem when the key, the type or the range is wrong.
    [[nodiscard]] virtual std::optional<std::string> updateGameRule(const ManagedGameRule& rule) = 0;

    /// A server setting by its management key, as text; empty for a key the server does not have.
    [[nodiscard]] virtual std::optional<std::string> setting(std::string_view key) = 0;
    /// The error names why the value was refused, or is empty once it took effect.
    [[nodiscard]] virtual std::optional<std::string> changeSetting(std::string_view key, std::string_view value) = 0;
};

} // namespace basalt::server::remote
