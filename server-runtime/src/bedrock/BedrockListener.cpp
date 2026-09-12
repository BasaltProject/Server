#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <chrono>
#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

using wire::bedrock::BedrockConnection;
using wire::bedrock::BedrockPacket;
using wire::bedrock::RakSession;

/// How long a poll waits for a datagram before going round again. Twenty milliseconds is two ticks
/// of nothing, which is short enough that an acknowledgement is never late and long enough that an
/// idle server is not a spinning thread.
constexpr int kPollTimeoutMillis = 20;

/// The overworld is three hundred and eighty four blocks tall, which is twenty four sections.
constexpr std::uint32_t kOverworldSubChunks = 24;

/// What a client is told to draw before it has asked for anything.
constexpr std::int32_t kInitialRadius = 4;

[[nodiscard]] std::uint64_t nowMillis() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

/// A semicolon separates every field of the line a Bedrock client reads, so one inside the message
/// would shift everything after it. A newline would do worse.
[[nodiscard]] std::string oneLine(std::string_view text) {
    std::string clean;
    clean.reserve(text.size());
    for (const char character : text) {
        clean += (character == ';' || character == '\n' || character == '\r') ? ' ' : character;
    }
    while (!clean.empty() && clean.front() == ' ') {
        clean.erase(clean.begin());
    }
    while (!clean.empty() && clean.back() == ' ') {
        clean.pop_back();
    }
    return clean;
}

/// A Bedrock server list shows two lines, and a Java message of the day is written as two. The
/// first is the name, the second is whatever the operator put under it.
[[nodiscard]] std::pair<std::string, std::string> splitMotd(std::string_view text) {
    const std::size_t newline = text.find('\n');
    if (newline == std::string_view::npos) {
        return {oneLine(text), std::string{}};
    }
    return {oneLine(text.substr(0, newline)), oneLine(text.substr(newline + 1))};
}

} // namespace

/// One client: its transport, its protocol state, and whether it has been told about a world yet.
struct BedrockListener::Session {
    RakSession& transport;
    BedrockConnection connection;
    bool started = false;

    Session(RakSession& session, std::shared_ptr<const wire::bedrock::EcKeyPair> keys, std::string gameVersion)
        : transport(session), connection(session, std::move(keys), std::move(gameVersion)) {}
};

BedrockListener::BedrockListener(BedrockOptions options, Hooks hooks, std::string gameVersion,
                                 wire::common::Logger logger)
    : options_(std::move(options)), hooks_(std::move(hooks)), gameVersion_(std::move(gameVersion)),
      logger_(std::move(logger)) {}

BedrockListener::~BedrockListener() { stop(); }

std::string BedrockListener::prefixedName(std::string_view gamertag) const {
    return options_.prefix + std::string{gamertag};
}

wire::common::Uuid BedrockListener::uuidOf(std::string_view xuid) {
    // The same shape as the offline uuid vanilla derives from a name, with a prefix of its own so
    // the two spaces cannot meet.
    return wire::common::Uuid::nameBased("BedrockPlayer:" + std::string{xuid});
}

std::string BedrockListener::advertisement() const {
    const Status status = hooks_.status ? hooks_.status() : Status{"Basalt", 0, 20};
    const auto [motd, subMotd] = splitMotd(status.motd);

    // The order is the client's, not ours: edition, the line, the protocol, the version, who is on,
    // how many fit, the server's own id, the second line, the game mode twice, and the two ports.
    std::string line = "MCPE;";
    line += motd + ";";
    line += std::to_string(wire::bedrock::kNativeBedrockProtocol) + ";";
    line += gameVersion_ + ";";
    line += std::to_string(status.online) + ";";
    line += std::to_string(status.max) + ";";
    line += std::to_string(server_ ? server_->guid() : 0) + ";";
    line += subMotd + ";";
    line += "Survival;1;";
    line += std::to_string(port()) + ";";
    line += std::to_string(port()) + ";";
    return line;
}

bool BedrockListener::start() {
    if (!options_.enabled) {
        logger_.info([] { return "Bedrock is off in compatibility.toml."; });
        return false;
    }

    auto keys = wire::bedrock::EcKeyPair::generate();
    if (!keys) {
        logger_.warn([] { return "Could not make the key pair Bedrock logins need; the door stays shut."; });
        return false;
    }
    keys_ = std::make_shared<const wire::bedrock::EcKeyPair>(std::move(*keys));

    wire::bedrock::RakServerConfig config;
    config.port = options_.port;
    server_ = std::make_unique<wire::bedrock::RakServer>(config);
    if (const auto started = server_->start(); !started) {
        logger_.warn([this] {
            return "Could not bind UDP port " + std::to_string(options_.port) +
                   " for Bedrock. Something else is on it; change `port` under [bedrock] in "
                   "config/compatibility.toml or turn it off.";
        });
        server_.reset();
        return false;
    }

    server_->onAdvertise([this] { return advertisement(); });
    server_->onConnect([this](RakSession& session) { onConnect(session); });
    server_->onMessage(
        [this](RakSession& session, std::span<const std::uint8_t> payload) { onMessage(session, payload); });
    server_->onDisconnect([this](RakSession& session) { onDisconnect(session); });

    running_.store(true);
    thread_ = std::jthread{[this] { run(); }};
    logger_.info([this] {
        return "Bedrock clients are welcome on UDP port " + std::to_string(port()) + " (protocol " +
               std::to_string(wire::bedrock::kNativeBedrockProtocol) + ", " + gameVersion_ + ").";
    });
    return true;
}

void BedrockListener::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    sessions_.clear();
    server_.reset();
}

std::uint16_t BedrockListener::port() const noexcept { return server_ ? server_->port() : options_.port; }

std::size_t BedrockListener::sessionCount() const { return sessions_.size(); }

void BedrockListener::run() {
    while (running_.load()) {
        server_->poll(nowMillis(), kPollTimeoutMillis);
    }
}

void BedrockListener::onConnect(RakSession& session) {
    auto owned = std::make_unique<Session>(session, keys_, gameVersion_);
    Session& entry = *owned;
    sessions_.emplace(session.peer(), std::move(owned));

    entry.connection.onLogin([this](const wire::bedrock::LoginRequest& login) {
        logger_.info([this, &login] {
            return prefixedName(login.name) + " is joining from Bedrock" +
                   (login.authenticated ? "" : " (unauthenticated)") + ".";
        });
    });
    entry.connection.onReady([this, &entry] { onReady(entry); });
    entry.connection.onPacket([&entry](BedrockPacket packet, wire::io::ByteBuffer& body) {
        // The client asks for a draw distance of its own once it is in; everything else waits for
        // the world to exist on this side.
        if (packet != BedrockPacket::RequestChunkRadius) {
            return;
        }
        const auto radius = wire::bedrock::readRequestChunkRadius(body);
        if (!radius) {
            return;
        }
        wire::io::ByteBuffer answer;
        wire::bedrock::writeChunkRadiusUpdated(answer, *radius);
        entry.connection.sendPacket(BedrockPacket::ChunkRadiusUpdated, answer.readable());
        entry.connection.flush();
    });
}

void BedrockListener::onMessage(RakSession& session, std::span<const std::uint8_t> payload) {
    const auto found = sessions_.find(session.peer());
    if (found == sessions_.end()) {
        return;
    }
    found->second->connection.receive(payload);
}

void BedrockListener::onDisconnect(RakSession& session) { sessions_.erase(session.peer()); }

void BedrockListener::onReady(Session& session) {
    if (session.started) {
        return;
    }
    session.started = true;

    wire::bedrock::StartGameInfo info;
    info.vanillaVersion = gameVersion_;
    info.serverId = "basalt";
    info.worldId = "basalt";

    wire::io::ByteBuffer body;
    wire::bedrock::writeStartGame(body, info);
    session.connection.sendPacket(BedrockPacket::StartGame, body.readable());

    body.clear();
    wire::bedrock::writeChunkRadiusUpdated(body, kInitialRadius);
    session.connection.sendPacket(BedrockPacket::ChunkRadiusUpdated, body.readable());

    body.clear();
    wire::bedrock::writeNetworkChunkPublisherUpdate(body, static_cast<std::int32_t>(info.x),
                                                    static_cast<std::int32_t>(info.y),
                                                    static_cast<std::int32_t>(info.z), kInitialRadius * 16);
    session.connection.sendPacket(BedrockPacket::NetworkChunkPublisherUpdate, body.readable());

    // Empty columns, because the world is not wired to this door yet: what the client needs before
    // it will stop loading is a chunk for every column it was told to expect, not blocks in them.
    for (std::int32_t x = -kInitialRadius; x <= kInitialRadius; ++x) {
        for (std::int32_t z = -kInitialRadius; z <= kInitialRadius; ++z) {
            body.clear();
            wire::bedrock::writeEmptyLevelChunk(body, x, z, kOverworldSubChunks, 0);
            session.connection.sendPacket(BedrockPacket::LevelChunk, body.readable());
        }
    }

    body.clear();
    wire::bedrock::writePlayStatus(body, wire::bedrock::PlayStatus::PlayerSpawn);
    session.connection.sendPacket(BedrockPacket::PlayStatus, body.readable());
    session.connection.flush();
}

} // namespace basalt::server::runtime::bedrock
