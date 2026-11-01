#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/Connection.hpp>
#include <basalt/server/runtime/Favicon.hpp>
#include <basalt/server/runtime/LoadingServer.hpp>
#include <basalt/server/runtime/net/SocketOptions.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime {
namespace {

using wire::protocol::ProtocolState;
using wire::protocol::handshake::ClientIntent;
using wire::protocol::handshake::HandshakePacket;
using wire::text::Component;

constexpr std::string_view kProfileName = "loading";
constexpr std::int32_t kReadTimeoutMillis = 5000;
constexpr int kMaxOpenConnections = 64;
constexpr std::chrono::milliseconds kAcceptPoll{10};

constexpr std::string_view kDefaultKick =
    "<red>The server is still starting.</red>\n<gray>Please wait a moment and try again.</gray>";

[[nodiscard]] config::ServerListProfile builtInProfile() {
    config::ServerListProfile profile;
    profile.motd = std::vector<std::string>{
        "              <bold><gray>BASALT SERVER</gray></bold> <dark_gray>[1.8-26.3]</dark_gray>",
        "         <red>The server is loading, please wait</red>",
    };
    profile.hidePlayerCount = true;
    profile.versionName = "Loading";
    profile.versionProtocol = -1;
    return profile;
}

[[nodiscard]] Component linesOf(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return Component::empty();
    }
    Component result = wire::text::miniMessage::parse(lines.front());
    for (std::size_t index = 1; index < lines.size(); index++) {
        result = result.append(Component::text("\n")).append(wire::text::miniMessage::parse(lines[index]));
    }
    return result;
}

[[nodiscard]] Component kickOf(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) {
            return linesOf(lines);
        }
        start = end + 1;
    }
}

template <typename Body>
void sendPacket(Connection& connection, std::int32_t id, Body&& body) {
    wire::io::ByteBuffer& buffer = connection.scratch();
    wire::protocol::varInt::write(buffer, id);
    body(buffer);
    connection.send(buffer.readable());
}

[[nodiscard]] bool handleStatus(Connection& connection, wire::io::ByteBuffer& packet, const LoadingServer::Look& look) {
    const std::int32_t id = wire::protocol::varInt::read(packet);
    if (id == wire::protocol::status::kStatusRequestId) {
        sendPacket(connection, wire::protocol::status::StatusResponsePacket::kId,
                   [&look](wire::io::ByteBuffer& buffer) {
                       wire::protocol::status::StatusResponsePacket{.status = look.status}.write(buffer);
                   });
        return true;
    }
    if (id == wire::protocol::status::PingPacket::kId) {
        const auto ping = wire::protocol::status::PingPacket::read(packet);
        if (ping.has_value()) {
            sendPacket(connection, wire::protocol::status::PingPacket::kId,
                       [&ping](wire::io::ByteBuffer& buffer) { ping->write(buffer); });
        }
    }
    return false;
}

void serve(wire::io::TcpConnection socket, const LoadingServer::Look& look) {
    if (!socket.setNonBlocking(false).has_value() ||
        !net::setReceiveTimeout(socket.nativeHandle(), kReadTimeoutMillis)) {
        return;
    }
    Connection connection{std::move(socket)};
    wire::io::ByteBuffer packet{4096};
    while (connection.isOpen()) {
        if (!connection.fill()) {
            return;
        }
        while (connection.nextPacket(packet)) {
            if (connection.state() == ProtocolState::Handshake) {
                if (wire::protocol::varInt::read(packet) != HandshakePacket::kId) {
                    return;
                }
                const auto handshake = HandshakePacket::read(packet);
                if (!handshake.has_value()) {
                    return;
                }
                connection.setState(handshake->intent == ClientIntent::Status ? ProtocolState::Status
                                                                              : ProtocolState::Login);
            } else if (connection.state() == ProtocolState::Status) {
                if (!handleStatus(connection, packet, look)) {
                    return;
                }
            } else {
                sendPacket(connection, wire::protocol::login::LoginDisconnectPacket::kId,
                           [&look](wire::io::ByteBuffer& buffer) {
                               wire::protocol::login::LoginDisconnectPacket{look.kick}.write(buffer);
                           });
                return;
            }
            if (packet.failed()) {
                return;
            }
        }
    }
}

} // namespace

LoadingServer::Look LoadingServer::lookOf(const config::BasaltConfig& config,
                                          const config::ServerListConfig& serverList,
                                          const config::MessagesConfig& messages) {
    config::ServerListProfile profile = builtInProfile();
    for (const config::ServerListProfile& candidate : serverList.profiles()) {
        if (candidate.header.name == kProfileName) {
            profile = candidate;
        }
    }
    profile = profile.inheriting(serverList.defaultProfile());

    Look look;
    look.status.description = linesOf(profile.motd.value_or(std::vector<std::string>{}));
    if (!profile.hidePlayerCount.value_or(false)) {
        wire::protocol::status::ServerStatusPayload::Players players{
            .max = profile.maxPlayers.value_or(config.maxPlayers), .online = profile.onlinePlayers.value_or(0)};
        for (const std::string& line : profile.hover.value_or(std::vector<std::string>{})) {
            players.sample.push_back({.name = line, .id = "00000000-0000-0000-0000-000000000000"});
        }
        look.status.players = std::move(players);
    }
    look.status.version = wire::protocol::status::ServerStatusPayload::Version{
        .name = profile.versionName.value_or(std::string{buildInfo::kMinecraftVersion}),
        .protocol = profile.versionProtocol.value_or(buildInfo::kProtocolVersion)};
    const std::span<const std::uint8_t> icon = favicon::bytes();
    look.status.favicon = std::vector<std::uint8_t>{icon.begin(), icon.end()};
    const std::string* custom = messages.find("server-loading");
    look.kick = kickOf(custom != nullptr ? *custom : std::string{kDefaultKick});
    return look;
}

std::unique_ptr<LoadingServer> LoadingServer::start(const config::BasaltConfig& config,
                                                    const config::ServerListConfig& serverList,
                                                    const config::MessagesConfig& messages,
                                                    const wire::common::Logger& logger) {
    if (!config.javaEnabled) {
        return nullptr;
    }
    const auto address = wire::io::TcpAddress::literal(config.javaBind, static_cast<std::uint16_t>(config.port));
    if (!address.has_value()) {
        return nullptr;
    }
    std::expected<wire::io::TcpListener, wire::io::SocketError> bound =
        wire::io::TcpListener::bind(*address, 128, config.javaDualStack);
    if (!bound.has_value() || !bound->setNonBlocking(true).has_value()) {
        logger.warn([&config] {
            return "Could not open port " + std::to_string(config.port) +
                   " early; the server list will not answer until startup finishes.";
        });
        return nullptr;
    }
    logger.info([&config] { return "Listening on port " + std::to_string(config.port) + " while the server starts."; });
    return std::make_unique<LoadingServer>(std::move(*bound),
                                           std::make_shared<const Look>(lookOf(config, serverList, messages)));
}

LoadingServer::LoadingServer(wire::io::TcpListener listener, std::shared_ptr<const Look> look)
    : listener_(std::move(listener)), look_(std::move(look)), open_(std::make_shared<std::atomic<int>>(0)),
      thread_([this] { acceptLoop(); }) {}

LoadingServer::~LoadingServer() { static_cast<void>(release()); }

void LoadingServer::acceptLoop() {
    while (!stopping_.load()) {
        std::expected<wire::io::TcpConnection, wire::io::SocketError> accepted = listener_->accept();
        if (!accepted.has_value()) {
            if (accepted.error().kind != wire::io::SocketError::Kind::WouldBlock) {
                return;
            }
            std::this_thread::sleep_for(kAcceptPoll);
            continue;
        }
        if (open_->load() >= kMaxOpenConnections) {
            continue;
        }
        open_->fetch_add(1);
        std::thread{[look = look_, open = open_, connection = std::move(*accepted)]() mutable {
            serve(std::move(connection), *look);
            open->fetch_sub(1);
        }}.detach();
    }
}

std::optional<wire::io::TcpListener> LoadingServer::release() {
    stopping_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    if (!listener_.has_value()) {
        return std::nullopt;
    }
    held_.store(false);
    static_cast<void>(listener_->setNonBlocking(false));
    std::optional<wire::io::TcpListener> released = std::move(listener_);
    listener_.reset();
    return released;
}

} // namespace basalt::server::runtime
