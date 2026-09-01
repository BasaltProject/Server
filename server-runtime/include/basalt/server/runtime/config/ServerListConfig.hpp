#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/server/runtime/config/ProfileSelection.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {

/// What the server knows about whoever is pinging. All of it comes from the handshake or the
/// socket.
///
/// The known player is resolved lazily and only once, because working it out means a database read
/// and a ping is a request anyone on the internet can send as fast as they like. A file that never
/// asks who is pinging never pays for the answer, which is why nothing has to scan the profiles up
/// front to decide whether the lookup is worth doing.
class PingRequest {
public:
    using Lookup = std::function<std::optional<std::string>()>;

    PingRequest(std::int32_t protocolVersion, std::string host, std::optional<std::string> address,
                Lookup lookupKnownPlayer = {})
        : protocolVersion_(protocolVersion), host_(std::move(host)), address_(std::move(address)),
          lookup_(std::move(lookupKnownPlayer)) {}

    PingRequest(std::int32_t protocolVersion, std::string host, std::optional<std::string> address,
                std::optional<std::string> knownPlayer)
        : protocolVersion_(protocolVersion), host_(std::move(host)), address_(std::move(address)),
          knownPlayer_(std::move(knownPlayer)), asked_(true) {}

    [[nodiscard]] std::int32_t protocolVersion() const noexcept { return protocolVersion_; }

    /// The address the client was pointed at, which is the handshake's virtual host.
    [[nodiscard]] const std::string& host() const noexcept { return host_; }

    [[nodiscard]] const std::optional<std::string>& address() const noexcept { return address_; }

    [[nodiscard]] const std::optional<std::string>& knownPlayer() const;

private:
    std::int32_t protocolVersion_ = 0;
    std::string host_;
    std::optional<std::string> address_;
    Lookup lookup_;
    mutable std::optional<std::string> knownPlayer_;
    mutable bool asked_ = false;
};

/// One complete look for the server list.
///
/// Every field except the header is optional because a profile is a set of overrides on the default
/// one: a maintenance profile that only changes the text should not have to restate the icon, the
/// version and the player count to avoid silently resetting them.
struct ServerListProfile {
    ProfileHeader header;
    std::optional<std::vector<std::string>> motd;
    std::optional<std::string> versionName;
    std::optional<std::int32_t> versionProtocol;
    std::optional<bool> hidePlayerCount;
    std::optional<std::int32_t> maxPlayers;
    std::optional<std::int32_t> onlinePlayers;
    std::optional<std::vector<std::string>> hover;
    std::optional<std::string> icon;

    [[nodiscard]] static ServerListProfile from(ProfileHeader header, const wire::common::toml::TomlTable& table);

    /// This profile's own values, with `fallback`'s used wherever it declared nothing.
    [[nodiscard]] ServerListProfile inheriting(const ServerListProfile& fallback) const;
};

/// `config/server_list.toml`: the profiles, and which one a given ping gets. Selection is
/// deliberately not first-match, since ordering rules that depend on file order are why permission
/// files are hard to reason about: highest priority wins, ties broken by declaration order.
///
/// A profile with no `when` and `active = false` is not a mistake: it is how an operator writes one
/// they switch on by hand.
class ServerListConfig {
public:
    static constexpr std::string_view kFileName = "server_list.toml";

    ServerListConfig(std::vector<ServerListProfile> profiles, std::size_t defaultIndex, bool rememberIp)
        : profiles_(std::move(profiles)), defaultIndex_(defaultIndex), rememberIp_(rememberIp),
          reporter_(std::make_unique<OnceReporter>()) {}

    [[nodiscard]] static std::expected<ServerListConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const std::optional<std::string>& legacyMotd = std::nullopt,
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const std::vector<ServerListProfile>& profiles() const noexcept { return profiles_; }

    [[nodiscard]] const ServerListProfile& defaultProfile() const { return profiles_[defaultIndex_]; }

    /// Whether the address somebody pinged from is remembered, so the greeting can name them.
    [[nodiscard]] bool rememberIp() const noexcept { return rememberIp_; }

    /// The profile this ping gets, by the same rule every UI file uses, already filled in from the
    /// default one.
    [[nodiscard]] ServerListProfile select(
        const wire::common::placeholder::PlaceholderRegistry& registry,
        const wire::common::placeholder::PlaceholderContext& context = wire::common::placeholder::PlaceholderContext{},
        const wire::common::Logger& logger = wire::common::Logger{"ServerList"}) const;

private:
    std::vector<ServerListProfile> profiles_;
    std::size_t defaultIndex_ = 0;
    bool rememberIp_ = false;
    /// Held indirectly so the configuration stays movable: the reporter owns a mutex.
    std::unique_ptr<OnceReporter> reporter_;
};

/// The file to generate on first run: the shipped one, or the shipped one with a vanilla
/// `server.properties` MOTD in place of both greetings.
///
/// Both, and not just the modern one, because the point of the import is that somebody arriving
/// from vanilla sees the message they already had. Leaving Basalt's own text on the older-client
/// profile would mean the same server greeted two people differently for no reason they chose.
[[nodiscard]] std::string serverListDefaultContents(const std::optional<std::string>& legacyMotd,
                                                    const wire::common::Logger& logger);

} // namespace basalt::server::runtime::config
