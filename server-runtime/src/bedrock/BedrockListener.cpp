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

/// One client: its transport, its protocol state, the player the server made of it, and the
/// mailbox anyone else writes to.
struct BedrockListener::Session {
    RakSession& transport;
    BedrockConnection connection;
    std::shared_ptr<BedrockMailbox> mailbox = std::make_shared<BedrockMailbox>();
    std::unique_ptr<BedrockListener::Player> player;
    wire::bedrock::LoginRequest login;
    bool started = false;

    Session(RakSession& session, std::shared_ptr<const wire::bedrock::EcKeyPair> keys, std::string gameVersion,
            wire::bedrock::BedrockGate gate)
        : transport(session), connection(session, std::move(keys), std::move(gameVersion), std::move(gate)) {}
};

void BedrockMailbox::post(const std::uint16_t packetId, std::vector<std::uint8_t> body, const bool last) {
    const std::lock_guard<std::mutex> guard{lock_};
    if (!open_) {
        return;
    }
    items_.push_back(Item{packetId, std::move(body), last});
    if (last) {
        open_ = false;
    }
}

std::vector<BedrockMailbox::Item> BedrockMailbox::drain() {
    const std::lock_guard<std::mutex> guard{lock_};
    return std::exchange(items_, {});
}

void BedrockMailbox::close() {
    const std::lock_guard<std::mutex> guard{lock_};
    open_ = false;
    items_.clear();
}

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
        std::string protocols;
        for (const std::int32_t protocol : options_.gate.supported()) {
            if (!protocols.empty()) {
                protocols += ", ";
            }
            protocols += std::to_string(protocol);
        }
        return "Bedrock clients are welcome on UDP port " + std::to_string(port()) + " (" + gameVersion_ +
               ", protocol " + protocols + ").";
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
        // Before the poll rather than after it: what the poll flushes is what is queued now, so a
        // line left by another thread goes out on this turn instead of the next.
        for (const auto& [peer, session] : sessions_) {
            if (session->player != nullptr) {
                session->player->tick();
            }
            deliverMail(*session);
        }
        server_->poll(nowMillis(), kPollTimeoutMillis);
    }
}

void BedrockListener::onConnect(RakSession& session) {
    auto owned = std::make_unique<Session>(session, keys_, gameVersion_, options_.gate);
    Session& entry = *owned;
    sessions_.emplace(session.peer(), std::move(owned));

    entry.connection.onLogin([this, &entry](const wire::bedrock::LoginRequest& login) {
        entry.login = login;
        logger_.info([this, &login] {
            return prefixedName(login.name) + " is joining from Bedrock" +
                   (login.authenticated ? "" : " (unauthenticated)") + ".";
        });
    });
    entry.connection.onRefused([this](const int protocol, const std::string& told) {
        logger_.info([protocol, &told] {
            return "A Bedrock client on protocol " + std::to_string(protocol) + " was turned away: " + told;
        });
    });
    entry.connection.onWarning([this](const int protocol) {
        logger_.warn([protocol] {
            return "A Bedrock client on protocol " + std::to_string(protocol) +
                   " is newer than this server speaks and was let in anyway; it may not get far.";
        });
    });
    entry.connection.onReady([this, &entry] { onReady(entry); });
    entry.connection.onPacket([&entry](BedrockPacket packet, wire::io::ByteBuffer& body) {
        if (packet == BedrockPacket::RequestChunkRadius) {
            // The client asks for a draw distance of its own once it is in.
            const auto radius = wire::bedrock::readRequestChunkRadius(body);
            if (!radius) {
                return;
            }
            wire::io::ByteBuffer answer;
            wire::bedrock::writeChunkRadiusUpdated(answer, *radius);
            entry.connection.sendPacket(BedrockPacket::ChunkRadiusUpdated, answer.readable());
            entry.connection.flush();
            return;
        }
        if (packet == BedrockPacket::PlayerAuthInput && entry.player) {
            const auto input = wire::bedrock::readPlayerAuthInput(body);
            if (input.has_value()) {
                entry.player->move(static_cast<double>(input->x),
                                   static_cast<double>(input->y) - static_cast<double>(wire::bedrock::kPlayerEyeHeight),
                                   static_cast<double>(input->z), input->yaw, input->pitch);
            }
            return;
        }
        if (packet == BedrockPacket::CommandRequest && entry.player) {
            const auto line = wire::bedrock::readCommandRequest(body);
            if (line.has_value()) {
                entry.player->command(*line);
            }
            return;
        }
        if (packet == BedrockPacket::ModalFormResponse && entry.player) {
            const auto answer = wire::bedrock::readModalFormResponse(body);
            if (answer.has_value()) {
                entry.player->formAnswer(answer->formId, answer->data);
            }
            return;
        }
        if (packet == BedrockPacket::Respawn && entry.player) {
            const auto state = wire::bedrock::readRespawn(body);
            if (state == wire::bedrock::RespawnState::ClientReady) {
                entry.player->respawn();
            }
            return;
        }
        if (packet == BedrockPacket::Text && entry.player) {
            const auto text = wire::bedrock::readText(body);
            if (text.has_value() && !text->message.empty()) {
                entry.player->chat(text->message);
            }
        }
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
    // The client is told where its eyes are, not its feet, which is the same offset it reports
    // back on every movement.
    info.y += wire::bedrock::kPlayerEyeHeight;
    info.vanillaVersion = gameVersion_;
    info.serverId = "basalt";
    info.worldId = "basalt";

    wire::io::ByteBuffer body;
    wire::bedrock::writeStartGame(body, info, session.connection.protocolVersion());
    session.connection.sendPacket(BedrockPacket::StartGame, body.readable());

    body.clear();
    wire::bedrock::writeChunkRadiusUpdated(body, kInitialRadius);
    session.connection.sendPacket(BedrockPacket::ChunkRadiusUpdated, body.readable());

    body.clear();
    wire::bedrock::writeNetworkChunkPublisherUpdate(body, static_cast<std::int32_t>(info.x),
                                                    static_cast<std::int32_t>(info.y),
                                                    static_cast<std::int32_t>(info.z), kInitialRadius * 16);
    session.connection.sendPacket(BedrockPacket::NetworkChunkPublisherUpdate, body.readable());

    // No empty columns here any more: the player made just below asks the world for the real
    // ones, which answer the same question and answer it with blocks in them. Sending a hole
    // first and the ground a moment later is worse than sending the ground a moment later.

    session.connection.flush();

    if (!hooks_.join) {
        return;
    }
    Joining joining;
    joining.gamertag = session.login.name;
    joining.name = prefixedName(session.login.name);
    joining.xuid = session.login.xuid;
    joining.uuid = uuidOf(session.login.xuid.empty() ? session.login.name : session.login.xuid);
    joining.address = session.transport.peer().toString();
    joining.authenticated = session.login.authenticated;
    joining.protocol = session.connection.protocolVersion();
    joining.mailbox = session.mailbox;
    session.player = hooks_.join(joining);
}

void BedrockListener::deliverMail(Session& session) {
    const std::vector<BedrockMailbox::Item> items = session.mailbox->drain();
    if (items.empty()) {
        return;
    }
    for (const BedrockMailbox::Item& item : items) {
        session.connection.sendPacket(static_cast<BedrockPacket>(item.packetId), item.body);
        if (item.last) {
            // The goodbye was written by whoever posted it, so this only has to stop: the client
            // leaves on its own once it has read it, and RakNet clears the session up after.
            session.connection.flush();
            session.connection.close();
            return;
        }
    }
    session.connection.flush();
}

} // namespace basalt::server::runtime::bedrock
