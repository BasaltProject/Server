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

struct ListedPlayer {
    std::string name;
    std::string uuid;

    friend bool operator==(const ListedPlayer&, const ListedPlayer&) = default;
};

class ServerListStatus {
public:
    struct Hooks {
        struct Answer {
            std::optional<std::string> motd;
            std::optional<std::int32_t> maxPlayers;
        };

        std::function<Answer(const std::optional<std::string>& address, const std::string& motd, std::int32_t online,
                             std::int32_t max)>
            ping;
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
    [[nodiscard]] std::vector<wire::protocol::status::ServerStatusPayload::SamplePlayer>
    sampleFor(const config::ServerListProfile& profile, const std::vector<ListedPlayer>& players,
              const wire::common::placeholder::PlaceholderContext& context) const;

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

    mutable std::mutex mutex_;
    mutable std::map<std::string, std::vector<std::uint8_t>> icons_;
    mutable std::set<std::string> reported_;
};

} // namespace basalt::server::runtime
