#include <basalt/server/runtime/LoginHandler.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/text/Component.hpp>

namespace basalt::server::runtime {
namespace {

using namespace basalt::wire;
using protocol::ProtocolState;

const common::Logger& logger() {
    static const common::Logger instance{"Connection"};
    return instance;
}

/// Writes one packet id and hands the body builder the buffer to fill in behind it.
template <typename Body>
void sendPacket(Connection& connection, std::int32_t id, Body&& body) {
    io::ByteBuffer& buffer = connection.scratch();
    protocol::varInt::write(buffer, id);
    body(buffer);
    connection.send(buffer.readable());
}

} // namespace

protocol::status::ServerStatusPayload LoginHandler::buildStatus(std::int32_t onlinePlayers) const {
    protocol::status::ServerStatusPayload payload;
    payload.description = text::Component::text(settings_.motd);
    payload.players = protocol::status::ServerStatusPayload::Players{settings_.maxPlayers, onlinePlayers, {}};
    payload.version = protocol::status::ServerStatusPayload::Version{settings_.versionName, settings_.protocolVersion};
    return payload;
}

bool LoginHandler::handleHandshake(Connection& connection, io::ByteBuffer& packet) {
    const std::int32_t id = protocol::varInt::read(packet);
    if (id != protocol::handshake::HandshakePacket::kId) {
        return false;
    }
    const auto handshake = protocol::handshake::HandshakePacket::read(packet);
    if (!handshake) {
        return false;
    }

    switch (handshake->intent) {
    case protocol::handshake::ClientIntent::Status:
        connection.setState(ProtocolState::Status);
        return true;
    case protocol::handshake::ClientIntent::Login:
    case protocol::handshake::ClientIntent::Transfer:
        connection.setState(ProtocolState::Login);
        if (handshake->protocolVersion != settings_.protocolVersion) {
            logger().info([&] {
                return "Refused a client speaking protocol " + std::to_string(handshake->protocolVersion) +
                       ", this server speaks " + std::to_string(settings_.protocolVersion);
            });
            sendPacket(connection, protocol::login::LoginDisconnectPacket::kId, [this](io::ByteBuffer& buffer) {
                protocol::login::LoginDisconnectPacket{
                    text::Component::text("This server runs " + settings_.versionName)}
                    .write(buffer);
            });
            return false;
        }
        return true;
    }
    return false;
}

bool LoginHandler::handleStatus(Connection& connection, io::ByteBuffer& packet) {
    const std::int32_t id = protocol::varInt::read(packet);
    if (id == protocol::status::kStatusRequestId) {
        sendPacket(connection, protocol::status::StatusResponsePacket::kId, [this](io::ByteBuffer& buffer) {
            protocol::status::StatusResponsePacket{buildStatus(0)}.write(buffer);
        });
        return true;
    }
    if (id == protocol::status::PingPacket::kId) {
        const auto ping = protocol::status::PingPacket::read(packet);
        if (!ping) {
            return false;
        }
        sendPacket(connection, protocol::status::PingPacket::kId,
                   [&ping](io::ByteBuffer& buffer) { ping->write(buffer); });
        // The client closes right after the pong, which is not an error: it got what it asked for.
        return false;
    }
    return false;
}

bool LoginHandler::handleLogin(Connection& connection, io::ByteBuffer& packet) {
    const std::int32_t id = protocol::varInt::read(packet);

    if (id == protocol::login::LoginStartPacket::kId) {
        const auto start = protocol::login::LoginStartPacket::read(packet);
        if (!start) {
            return false;
        }

        if (settings_.compressionThreshold >= 0) {
            sendPacket(connection, protocol::login::LoginCompressionPacket::kId, [this](io::ByteBuffer& buffer) {
                protocol::login::LoginCompressionPacket{settings_.compressionThreshold}.write(buffer);
            });
            // Every packet after this one is framed the compressed way, this one included on the
            // client's side, so the threshold is applied only once the packet has left.
            connection.setCompressionThreshold(settings_.compressionThreshold);
        }

        protocol::login::LoginFinishedPacket finished;
        finished.gameProfile.id = common::Uuid::offlinePlayer(start->name);
        finished.gameProfile.name = start->name;
        finished.sessionId = finished.gameProfile.id;
        sendPacket(connection, protocol::login::LoginFinishedPacket::kId,
                   [&finished](io::ByteBuffer& buffer) { finished.write(buffer); });

        logger().info([&] {
            return start->name + " (" + finished.gameProfile.id.toString() + ") logged in from " +
                   connection.remoteAddress();
        });
        return true;
    }

    if (id == protocol::login::kLoginAcknowledgedId) {
        connection.setState(ProtocolState::Configuration);
        return true;
    }
    return false;
}

void LoginHandler::run(Connection& connection) {
    io::ByteBuffer packet{4096};

    while (connection.isOpen()) {
        if (!connection.fill()) {
            return;
        }

        while (connection.nextPacket(packet)) {
            bool keepGoing = false;
            switch (connection.state()) {
            case ProtocolState::Handshake:
                keepGoing = handleHandshake(connection, packet);
                break;
            case ProtocolState::Status:
                keepGoing = handleStatus(connection, packet);
                break;
            case ProtocolState::Login:
                keepGoing = handleLogin(connection, packet);
                break;
            case ProtocolState::Configuration:
            case ProtocolState::Play:
                // Nothing past the login handshake is implemented yet, so the connection is held
                // open and idle rather than fed packets nothing can answer.
                keepGoing = true;
                break;
            }

            if (!keepGoing || packet.failed()) {
                return;
            }
        }
    }
}

} // namespace basalt::server::runtime
