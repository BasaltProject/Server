#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ChunkStreamer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/PlayerLifecycle.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/Owner.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;

/// How long the outbound thread waits between passes. One tick.
constexpr std::chrono::milliseconds kOutboundInterval{50};

/// A client that has been idle cannot then be handed an enormous burst at once.
constexpr float kMaxBurstChunks = 64.0F;

/// The per-player half of the surface hooks: `PlayerUi` draws one player and asks about that one,
/// while a claim is made on the server as a whole.
[[nodiscard]] ui::PlayerUi::Plugin surfacesOf(ClientSession& session) {
    const ClientSession::PluginHooks::Surfaces& surfaces = session.options().plugins.surfaces;
    ui::PlayerUi::Plugin plugin;
    plugin.holdsSidebar = surfaces.holdsSidebar;
    plugin.holdsBossBar = surfaces.holdsBossBar;
    if (surfaces.sidebar) {
        plugin.sidebar = [&session]() -> std::optional<ui::SurfaceContent> {
            const player::PlayerSession* player = session.playerSession();
            return player == nullptr ? std::nullopt
                                     : session.options().plugins.surfaces.sidebar(player->uuid().toString());
        };
    }
    if (surfaces.bossBar) {
        plugin.bossBar = [&session]() -> std::optional<ui::SurfaceContent> {
            const player::PlayerSession* player = session.playerSession();
            return player == nullptr ? std::nullopt
                                     : session.options().plugins.surfaces.bossBar(player->uuid().toString());
        };
    }
    return plugin;
}

} // namespace

ClientSession::ClientSession(wire::io::TcpConnection connection, Options options)
    : connection_(std::move(connection)), options_(std::move(options)), proxy_(options_.forwarding, logger_),
      playerUi_(
          options_.ui, *options_.placeholders,
          [this](std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& write) {
              sendPlayPacket(id, write);
          },
          surfacesOf(*this)),
      menus_(std::make_unique<ContainerMenuController>(*this)),
      blockInteraction_(std::make_unique<BlockInteractionHandler>(*this)),
      chunkStreamer_(std::make_unique<ChunkStreamer>(*this)), combat_(std::make_unique<CombatHandler>(*this)),
      entityTracker_(std::make_unique<EntityVisibilityTracker>(*this)),
      itemUseTimers_(std::make_unique<ItemUseTimers>(*this)), lifecycle_(std::make_unique<PlayerLifecycle>(*this)),
      uiState_(std::make_unique<UiStateSync>(*this)), trades_(std::make_unique<VillagerTradeController>(*this)) {}

ClientSession::~ClientSession() {
    streaming_.store(false);
    outbox_.close();
}

std::optional<std::string> ClientSession::remoteAddress() const {
    if (proxy_.login().has_value()) {
        return proxy_.login()->address;
    }
    return connection_.remoteAddress();
}

void ClientSession::onOwner(std::function<void()> work) {
    // Before the join finishes there is no world to own anything, and after it there always is.
    world::LoadedWorld* world = currentWorld_;
    if (world == nullptr) {
        work();
        return;
    }
    sched::Owner& owner = regionOwner_ == nullptr ? world->owner() : *regionOwner_;
    static_cast<void>(owner.submit(std::move(work)));
}

void ClientSession::handOverToRegionOf(world::LoadedWorld& world) {
    // Called from the global phase, where no region is running, which is the safe point for it: a
    // player who walks across a border must not change owner mid-tick.
    const player::PlayerSession* session = playerSession_.get();
    if (session == nullptr || session->worldName != world.name()) {
        return;
    }
    regionOwner_ = &world.ownerOf(static_cast<std::int32_t>(session->x), static_cast<std::int32_t>(session->z));
}

void ClientSession::startOutboundLoop() {
    bool expected = false;
    if (!streaming_.compare_exchange_strong(expected, true)) {
        return;
    }
    // From here on this thread owns the socket: everything else queues. Sockets here block, so a
    // client that stops draining would otherwise park whoever writes to it.
    outbox_.open();
    outbound_ = std::jthread([this] {
        while (streaming_.load()) {
            bool idle = false;
            if (tickTaskPending_.compare_exchange_strong(idle, true)) {
                // The player's own turn goes to the owner of the world they are in: what they
                // produce is a packet, and a packet is queued rather than written, so the owner
                // never waits on a socket.
                onOwner([this] {
                    // A tick's worth of chunk allowance. Reading and serializing a chunk is the
                    // world's business; writing the bytes is still this thread's.
                    chunkStreamer_->sendBudget =
                        std::min(chunkStreamer_->sendBudget + chunkStreamer_->desiredChunksPerTick, kMaxBurstChunks);
                    const auto allowed = static_cast<std::int32_t>(chunkStreamer_->sendBudget);
                    if (allowed > 0) {
                        chunkStreamer_->sendBudget -= static_cast<float>(chunkStreamer_->flushReadyChunks(allowed));
                    }
                    lifecycle_->tickPlayer();
                    entityTracker_->syncPlayers();
                    entityTracker_->syncNpcs();
                    uiState_->syncPosture();
                    entityTracker_->syncItemEntities();
                    entityTracker_->syncSummonedEntities();
                    entityTracker_->syncGeneratedEntities();
                    uiState_->tickUi();
                    if (menus_->openBlock.has_value() && menus_->openBlockSize == world::furnaces::kSize) {
                        menus_->syncOpenContainer();
                    }
                    tickTaskPending_.store(false);
                });
            }
            sendKeepAliveIfDue();
            flushOutbox();
            std::this_thread::sleep_for(kOutboundInterval);
        }
    });
}

void ClientSession::run() {
    std::array<std::uint8_t, 4096> readChunk{};
    while (true) {
        const std::expected<std::size_t, wire::io::SocketError> read = connection_.read(readChunk);
        if (!read.has_value() || *read == 0) {
            break;
        }
        const std::span<const std::uint8_t> chunk{readChunk.data(), *read};
        if (decryptCipher_.has_value()) {
            decryptCipher_->decrypt(std::span{readChunk.data(), *read});
        }
        incoming_.writeBytes(chunk);
        drainFrames();
        // Whatever answering those frames queued goes out now: this is a thread of this connection,
        // so it may write, and waiting for the outbound loop would add its whole interval to every
        // reply.
        if (outbox_.isOpen()) {
            flushOutbox();
        }
    }
    streaming_.store(false);
    lifecycle_->leavePlayIfPresent();
    // Whatever the leaving player's own thread had queued for others is already theirs; what was
    // queued for this socket has nowhere to go.
    outbox_.close();
    connection_.close();
}

void ClientSession::drainFrames() {
    wire::io::ByteBuffer frame;
    while (true) {
        frame.clear();
        const std::expected<bool, wire::protocol::FramingError> read =
            wire::protocol::packetFraming::tryReadFrame(incoming_, compressionEnabled_, frame);
        if (!read.has_value()) {
            // One malformed frame costs one connection, not the thread reading it.
            logger_.debug([] { return "Dropping a connection whose frame could not be read."; });
            connection_.close();
            return;
        }
        if (!*read) {
            break;
        }
        incoming_.discardReadBytes();
        handleFrame(frame);
    }
}

void ClientSession::handleFrame(wire::io::ByteBuffer& body) {
    const std::int32_t packetId = wire::protocol::varInt::read(body);
    if (body.failed()) {
        return;
    }
    lastPacketId_ = packetId;
    if (translation_ == nullptr) {
        handleFrameBody(packetId, body);
        return;
    }
    // An older client's packet is brought up to what this server reads before anything here looks at
    // it, so nothing past this line knows there is more than one version.
    const std::span<const std::uint8_t> readable = body.readable();
    const std::vector<std::uint8_t> rest(readable.begin(), readable.end());
    translation_->chain().serverbound(state_, packetId, rest,
                                      [this](std::int32_t id, std::span<const std::uint8_t> bytes) {
                                          lastPacketId_ = id;
                                          wire::io::ByteBuffer translated;
                                          translated.writeBytes(bytes);
                                          handleFrameBody(id, translated);
                                      });
}

void ClientSession::handleFrameBody(std::int32_t packetId, wire::io::ByteBuffer& body) {
    if (options_.plugins.incoming) {
        const std::optional<std::vector<std::uint8_t>> edited =
            options_.plugins.incoming(state_, packetId, body.readable());
        if (!edited.has_value()) {
            return;
        }
        if (*edited != std::vector<std::uint8_t>(body.readable().begin(), body.readable().end())) {
            wire::io::ByteBuffer replaced;
            replaced.writeBytes(*edited);
            body = std::move(replaced);
        }
    }
    switch (state_) {
    case wire::protocol::ProtocolState::Handshake:
        handleHandshake(packetId, body);
        return;
    case wire::protocol::ProtocolState::Status:
        handleStatus(packetId, body);
        return;
    case wire::protocol::ProtocolState::Login:
        handleLogin(packetId, body);
        return;
    case wire::protocol::ProtocolState::Configuration:
        handleConfiguration(packetId, body);
        return;
    case wire::protocol::ProtocolState::Play:
        break;
    }
    // parity: `PacketUtils.ensureRunningOnSameThread`, which is what vanilla does with every play
    // packet: the network thread reads it and the server thread runs it. Here that is the world's
    // owner, so everything a packet touches is touched by whoever owns it.
    wire::io::ByteBuffer owned;
    owned.writeBytes(body.readable());
    onOwner([this, packetId, moved = std::move(owned)]() mutable { handlePlay(packetId, moved); });
}

void ClientSession::sendPlayPacket(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody) {
    // A write failure is swallowed on purpose: the owning read loop already notices the close, and
    // letting it propagate used to abort a broadcast partway through whenever one recipient had
    // just disconnected.
    sendCompressed(id, writeBody);
}

void ClientSession::sendKeepAliveIfDue() {
    // Vanilla's own 15-second interval, driven from this connection rather than a shared timer, so
    // a client that has stopped draining its socket delays nothing but itself.
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastKeepAlive_).count() < kKeepAliveIntervalSeconds) {
        return;
    }
    lastKeepAlive_ = now;
    const std::int64_t token = options_.nextSeed ? options_.nextSeed() : 0;
    sendCompressed(play::PlayKeepAlivePacket::kClientboundId, [token](wire::io::ByteBuffer& buffer) {
        play::PlayKeepAlivePacket{.keepAliveId = token}.write(buffer);
    });
}

void ClientSession::disconnect(const wire::text::Component& reason) {
    sendCompressed(play::PlayDisconnectPacket::kId, [&reason](wire::io::ByteBuffer& buffer) {
        play::PlayDisconnectPacket{.reason = reason}.write(buffer);
    });
    // Everything queued has to be on the wire before the socket goes, or the client is left guessing
    // why it was dropped. Whoever asked for the kick is often the world's owner, and the owner must
    // not park on a socket, so the closing is left to this connection's own thread.
    closeWhenDrained_.store(true);
    if (!outbox_.isOpen()) {
        outbox_.close();
        connection_.close();
    }
}

void ClientSession::sendUncompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody) {
    wire::io::ByteBuffer body;
    wire::protocol::varInt::write(body, id);
    writeBody(body);
    wire::io::ByteBuffer frame;
    wire::protocol::packetFraming::writeUncompressed(frame, body.readable());
    writeToSocket(frame);
}

void ClientSession::sendCompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody) {
    wire::io::ByteBuffer body;
    wire::protocol::varInt::write(body, id);
    writeBody(body);
    std::vector<std::uint8_t> bytes(body.readable().begin(), body.readable().end());
    const std::size_t idBytes = static_cast<std::size_t>(wire::protocol::varInt::sizeOf(id));
    if (options_.plugins.outgoing) {
        // The id is in front of the body, and a plugin is shown the packet without it.
        const std::optional<std::vector<std::uint8_t>> edited =
            options_.plugins.outgoing(state_, id, std::span{bytes}.subspan(idBytes));
        if (!edited.has_value()) {
            return;
        }
        std::vector<std::uint8_t> rebuilt(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(idBytes));
        rebuilt.insert(rebuilt.end(), edited->begin(), edited->end());
        bytes = std::move(rebuilt);
    }
    if (translation_ != nullptr) {
        // After the plugins on purpose: a plugin's packet API is this server's version, so it sees
        // and edits the 26.2 shape and the translation happens on the way out of here.
        sendTranslated(translation_->chain(), id, std::span{bytes}.subspan(idBytes));
        return;
    }
    wire::io::ByteBuffer frame;
    static_cast<void>(wire::protocol::packetFraming::writeCompressed(frame, bytes, options_.compressionThreshold));
    writeToSocket(frame);
}

void ClientSession::sendTranslated(const wire::multiversion::ProtocolChain& chain, std::int32_t id,
                                   std::span<const std::uint8_t> body) {
    // Its own buffers rather than a shared scratch: a link may answer with more than one packet, and
    // reusing one would have the second overwrite the first before it is on the wire.
    chain.clientbound(state_, id, body, [this](std::int32_t translatedId, std::span<const std::uint8_t> translated) {
        wire::io::ByteBuffer whole;
        wire::protocol::varInt::write(whole, translatedId);
        whole.writeBytes(translated);
        wire::io::ByteBuffer frame;
        static_cast<void>(
            wire::protocol::packetFraming::writeCompressed(frame, whole.readable(), options_.compressionThreshold));
        writeToSocket(frame);
    });
}

void ClientSession::handleCustomPayload(wire::io::ByteBuffer& body) {
    // Read only when somebody is listening on that name: a modded client chatters on channels this
    // server has no opinion about, and decoding all of it to throw it away would be a cost every
    // server pays for the few that use one.
    if (!options_.plugins.channelMessage || !options_.plugins.listensOn) {
        return;
    }
    const std::optional<play::CustomPayloadPacket> packet = play::CustomPayloadPacket::read(body);
    const player::PlayerSession* session = playerSession_.get();
    if (!packet.has_value() || session == nullptr || !options_.plugins.listensOn(packet->channel)) {
        return;
    }
    options_.plugins.channelMessage(session->uuid().toString(), packet->channel, packet->data);
}

void ClientSession::writeToSocket(const wire::io::ByteBuffer& frame) {
    // Once this connection has its own outbound thread, every frame goes through the outbox and that
    // thread is the only writer. Before then (login and configuration) there is no other thread to
    // hand it to, so it goes straight out.
    const std::span<const std::uint8_t> bytes = frame.readable();
    if (outbox_.isOpen()) {
        static_cast<void>(outbox_.offer(std::vector<std::uint8_t>(bytes.begin(), bytes.end())));
        return;
    }
    // One packet reaches the socket at a time: other threads broadcast into this session too, and
    // without the lock their writes interleave mid-packet. The cipher sits inside the same lock
    // because CFB8 is stateful and concurrent encryption would corrupt both packets.
    const std::lock_guard<std::mutex> guard{sendMutex_};
    writeFrame(bytes);
}

void ClientSession::writeFrame(std::span<const std::uint8_t> frame) {
    if (encryptCipher_.has_value()) {
        std::vector<std::uint8_t> ciphered(frame.begin(), frame.end());
        encryptCipher_->encrypt(ciphered);
        static_cast<void>(connection_.write(ciphered));
        return;
    }
    static_cast<void>(connection_.write(frame));
}

void ClientSession::flushOutbox() {
    // parity of intent with vanilla's own outbound queue: a client that stops draining delays only
    // itself, and past the outbox's limit it is disconnected rather than left growing a queue nobody
    // will ever read.
    if (closeWhenDrained_.load()) {
        {
            const std::lock_guard<std::mutex> guard{sendMutex_};
            for (const std::vector<std::uint8_t>& frame : outbox_.drain()) {
                writeFrame(frame);
            }
        }
        outbox_.close();
        connection_.close();
        return;
    }
    if (outbox_.hasOverflowed()) {
        logger_.debug([] { return "Disconnecting a client that stopped draining its socket."; });
        connection_.close();
        return;
    }
    // Taking and writing under the same lock, because two flushers that took their frames first and
    // queued for the socket afterwards would write them in whichever order they woke up.
    const std::lock_guard<std::mutex> guard{sendMutex_};
    for (const std::vector<std::uint8_t>& frame : outbox_.drain()) {
        writeFrame(frame);
    }
}

} // namespace basalt::server::runtime
