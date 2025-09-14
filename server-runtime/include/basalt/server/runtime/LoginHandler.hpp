#pragma once

#include <basalt/server/runtime/Connection.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime {

/// What the server answers a server list ping with, and how it greets a login.
struct ServerSettings {
    std::string motd = "A Basalt server";
    std::int32_t maxPlayers = 20;
    std::string versionName = "Basalt 26.2";
    std::int32_t protocolVersion = 776;
    /// Packets at least this large are compressed. Negative disables compression entirely.
    std::int32_t compressionThreshold = 256;
};

/// Drives one connection from the handshake through either a status exchange or an offline-mode
/// login, up to the point the client acknowledges and moves to configuration.
///
/// Online mode is not wired up here: it needs the RSA and AES work of wire-auth.
class LoginHandler {
public:
    explicit LoginHandler(const ServerSettings& settings) : settings_(settings) {}

    /// Runs the connection until the peer goes away or the state machine has nothing more to say.
    void run(Connection& connection);

private:
    [[nodiscard]] wire::protocol::status::ServerStatusPayload buildStatus(std::int32_t onlinePlayers) const;

    bool handleHandshake(Connection& connection, wire::io::ByteBuffer& packet);
    bool handleStatus(Connection& connection, wire::io::ByteBuffer& packet);
    bool handleLogin(Connection& connection, wire::io::ByteBuffer& packet);

    const ServerSettings& settings_;
};

} // namespace basalt::server::runtime
