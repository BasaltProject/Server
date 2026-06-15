#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginKnownPlayer {
    std::string uuid;
    std::string name;
    std::int64_t firstSeenMillis = 0;
    std::int64_t lastSeenMillis = 0;
    std::string world;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct PluginBan {
    std::string subject;
    std::string name;
    std::string reason;
    std::string source;
    std::int64_t createdMillis = 0;
    std::optional<std::int64_t> expiresMillis;
};

/// Who may join and who may do what: the player database, operators, the whitelist and the ban lists.
class PluginAccess {
public:
    PluginAccess() = default;
    PluginAccess(const PluginAccess&) = delete;
    PluginAccess& operator=(const PluginAccess&) = delete;
    PluginAccess(PluginAccess&&) = delete;
    PluginAccess& operator=(PluginAccess&&) = delete;
    virtual ~PluginAccess() = default;

    [[nodiscard]] static PluginAccess& unavailable();

    [[nodiscard]] virtual std::optional<PluginKnownPlayer> playerByUuid(std::string_view uuid) {
        static_cast<void>(uuid);
        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<PluginKnownPlayer> playerByName(std::string_view name) {
        static_cast<void>(name);
        return std::nullopt;
    }

    [[nodiscard]] virtual std::vector<PluginKnownPlayer> knownPlayers() { return {}; }

    [[nodiscard]] virtual bool isOperator(std::string_view uuid) {
        static_cast<void>(uuid);
        return false;
    }

    /// Moves the player into the operator group, or back to the default one; false when the player is unknown.
    virtual bool setOperator(std::string_view uuid, bool value) {
        static_cast<void>(uuid);
        static_cast<void>(value);
        return false;
    }

    [[nodiscard]] virtual std::vector<PluginKnownPlayer> operators() { return {}; }

    [[nodiscard]] virtual bool isWhitelisted(std::string_view uuid) {
        static_cast<void>(uuid);
        return false;
    }

    virtual void setWhitelisted(std::string_view uuid, std::string_view name, bool value) {
        static_cast<void>(uuid);
        static_cast<void>(name);
        static_cast<void>(value);
    }

    [[nodiscard]] virtual std::vector<PluginKnownPlayer> whitelisted() { return {}; }

    [[nodiscard]] virtual bool whitelistEnforced() { return false; }

    virtual void setWhitelistEnforced(bool value) { static_cast<void>(value); }

    /// `ip` selects the address list; otherwise `subject` is a player uuid.
    [[nodiscard]] virtual std::optional<PluginBan> banOf(std::string_view subject, bool ip) {
        static_cast<void>(subject);
        static_cast<void>(ip);
        return std::nullopt;
    }

    [[nodiscard]] virtual std::vector<PluginBan> bans(bool ip) {
        static_cast<void>(ip);
        return {};
    }

    /// Writes the ban and sends whoever it hits away; an existing ban of the same subject is replaced.
    virtual void ban(const PluginBan& record, bool ip) {
        static_cast<void>(record);
        static_cast<void>(ip);
    }

    virtual bool pardon(std::string_view subject, bool ip) {
        static_cast<void>(subject);
        static_cast<void>(ip);
        return false;
    }
};

inline PluginAccess& PluginAccess::unavailable() {
    static PluginAccess none;
    return none;
}

} // namespace basalt::server::plugins
