#pragma once

#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime {

/// One player, as the list's hover text names them.
struct ListedPlayer {
    std::string name;
    std::string uuid;

    friend bool operator==(const ListedPlayer&, const ListedPlayer&) = default;
};

/// Turns a ping into the status the client draws, picking a profile out of `server_list.toml` and
/// rendering it. Neither this nor `ServerListConfig` resolves placeholders, which is what lets the
/// same text work in a chat format too.
///
/// Runs on a connection thread before login for a request anyone can send as fast as they like:
/// icons are cached and the database is only touched if the file asked for something that needs it.
class ServerListStatus {
public:
    /// What a plugin gets the last word on. Callbacks rather than the plugin bus itself: unset means
    /// the profile's own answer stands.
    struct Hooks {
        /// Given the pinger's address, the rendered line, the count and the slots, answers what to
        /// show instead. An empty answer changes nothing.
        struct Answer {
            std::optional<std::string> motd;
            std::optional<std::int32_t> maxPlayers;
        };

        std::function<Answer(const std::optional<std::string>& address, const std::string& motd, std::int32_t online,
                             std::int32_t max)>
            ping;
        /// The name last seen at an address, for a greeting that names whoever is pinging.
        std::function<std::optional<std::string>(const std::string& address)> playerByAddress;
    };

    struct Options {
        std::int32_t slots = 0;
        std::string versionName;
        std::int32_t protocolVersion = 0;
        std::string configRoot = "config";
    };

    ServerListStatus(std::function<const config::ServerListConfig&()> config,
                     const wire::common::placeholder::PlaceholderRegistry& placeholders,
                     std::function<std::vector<ListedPlayer>()> onlinePlayers, Options options, Hooks hooks = {},
                     wire::common::Logger logger = wire::common::Logger{"ServerList"});

    [[nodiscard]] wire::protocol::status::ServerStatusPayload
    statusFor(std::int32_t protocolVersion, const std::string& host, const std::optional<std::string>& address);

private:
    /// The hover text under the player count: the operator's own lines if a profile wrote any,
    /// otherwise the players actually online, which is what vanilla puts there.
    [[nodiscard]] std::vector<wire::protocol::status::ServerStatusPayload::SamplePlayer>
    sampleFor(const config::ServerListProfile& profile, const std::vector<ListedPlayer>& players,
              const wire::common::placeholder::PlaceholderContext& context) const;

    /// Reads a profile's icon once and remembers the answer, including a failure.
    ///
    /// Remembering the failure matters as much as remembering the file: a missing icon must not turn
    /// into a failed open on every ping, and it must not log the same complaint forever either.
    [[nodiscard]] std::vector<std::uint8_t> iconFor(const config::ServerListProfile& profile) const;

    [[nodiscard]] wire::text::Component render(const std::vector<std::string>& lines,
                                               const wire::common::placeholder::PlaceholderContext& context) const;

    [[nodiscard]] std::string substitute(const std::string& text,
                                         const wire::common::placeholder::PlaceholderContext& context) const;

    void reportOnce(const std::string& message) const;

    std::function<const config::ServerListConfig&()> config_;
    const wire::common::placeholder::PlaceholderRegistry* placeholders_;
    std::function<std::vector<ListedPlayer>()> onlinePlayers_;
    Options options_;
    Hooks hooks_;
    wire::common::Logger logger_;

    /// Any connection thread can ping, so both caches have one owner and it is this lock. An empty
    /// vector is a remembered failure, which is why the map holds one rather than an optional.
    mutable std::mutex mutex_;
    mutable std::map<std::string, std::vector<std::uint8_t>> icons_;
    mutable std::set<std::string> reported_;
};

} // namespace basalt::server::runtime
