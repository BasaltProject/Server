#pragma once

#include <basalt/server/runtime/Connection.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime {

struct ServerSettings {
    std::string motd = "A Basalt server";
    std::int32_t maxPlayers = 20;
    std::string versionName = "Basalt 26.2";
    std::int32_t protocolVersion = 776;
    std::int32_t compressionThreshold = 256;
};

class LoginHandler {
public:
    explicit LoginHandler(const ServerSettings& settings) : settings_(settings) {}

    void run(Connection& connection);

private:
    [[nodiscard]] wire::protocol::status::ServerStatusPayload buildStatus(std::int32_t onlinePlayers) const;

    bool handleHandshake(Connection& connection, wire::io::ByteBuffer& packet);
    bool handleStatus(Connection& connection, wire::io::ByteBuffer& packet);
    bool handleLogin(Connection& connection, wire::io::ByteBuffer& packet);

    const ServerSettings& settings_;
};

} // namespace basalt::server::runtime
