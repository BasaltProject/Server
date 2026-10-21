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
#include <basalt/server/runtime/player/PlayerGliding.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventHooks.hpp>
#include <basalt/server/runtime/sched/Owner.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {

namespace mv = wire::multiversion;

namespace {

namespace play = wire::protocol::play;

constexpr std::chrono::milliseconds kOutboundInterval{50};

// An idle client cannot then be handed an enormous burst at once.
constexpr float kMaxBurstChunks = 64.0F;

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
      menus_(std::make_unique<ContainerMenuController>(
          *this,
          options_.events == nullptr ? ContainerMenuController::Hooks{}
                                     : plugins::eventHooks::containerMenu(*this, *options_.events),
          ContainerMenuController::Options{
              .nextDouble = options_.nextDouble, .nextFloat = options_.nextFloat, .nextSeed = options_.nextSeed})),
      // Left unset, every broken block rolled its loot from seed zero.
      blockInteraction_(std::make_unique<BlockInteractionHandler>(
          *this,
          options_.events == nullptr ? BlockInteractionHandler::Hooks{}
                                     : plugins::eventHooks::blockInteraction(*this, *options_.events),
          BlockInteractionHandler::Options{
              .nextDouble = options_.nextDouble, .nextSeed = options_.nextSeed, .nextBool = options_.nextBool})),
      chunkStreamer_(std::make_unique<ChunkStreamer>(*this)),
      combat_(std::make_unique<CombatHandler>(
          *this,
          options_.events == nullptr ? CombatHandler::Hooks{} : plugins::eventHooks::combat(*this, *options_.events),
          CombatHandler::Options{.nextDouble = options_.nextDouble, .nextSeed = options_.nextSeed})),
      entityTracker_(std::make_unique<EntityVisibilityTracker>(*this)),
      itemUseTimers_(std::make_unique<ItemUseTimers>(
          *this,
          options_.events == nullptr ? ItemUseTimers::Hooks{} : plugins::eventHooks::itemUse(*this, *options_.events),
          ItemUseTimers::Options{.nextSeed = options_.nextSeed, .nextFloat = options_.nextFloat})),
      lifecycle_(std::make_unique<PlayerLifecycle>(
          *this, options_.events == nullptr ? PlayerLifecycle::Hooks{}
                                            : plugins::eventHooks::lifecycle(*this, *options_.events))),
      uiState_(std::make_unique<UiStateSync>(*this, options_.events == nullptr
                                                        ? UiStateSync::Hooks{}
                                                        : plugins::eventHooks::uiState(*this, *options_.events))),
      trades_(std::make_unique<VillagerTradeController>(
          *this, options_.events == nullptr ? VillagerTradeController::Hooks{}
                                            : plugins::eventHooks::villagerTrade(*this, *options_.events))) {
    attachEndpoint();
}

ClientSession::~ClientSession() {
    detachEndpoint();
    streaming_.store(false);
    if (carried_ != nullptr) {
        carried_->end();
    }
    outbox_.close();
    // Joined here: this thread hands work to the world's owner, so nothing below means anything while it runs.
    if (outbound_.joinable()) {
        outbound_.request_stop();
        outbound_.join();
    }
    // Nothing queued runs from here on and running tasks are waited for: the tick thread holds this session.
    ownerWork_->alive.store(false);
    while (ownerWork_->running.load() > 0) {
        std::this_thread::yield();
    }
}

std::optional<std::string> ClientSession::remoteAddress() const {
    if (proxy_.login().has_value()) {
        return proxy_.login()->address;
    }
    return connection_.remoteAddress();
}

void ClientSession::onOwner(std::function<void()> work) {
    world::LoadedWorld* world = currentWorld_;
    if (world == nullptr) {
        work();
        return;
    }
    sched::Owner& owner = regionOwner_ == nullptr ? world->owner() : *regionOwner_;
    static_cast<void>(owner.submit([work = std::move(work), guard = ownerWork_] {
        // Counted before the question is asked, so a destructor that already asked sees it and waits.
        guard->running.fetch_add(1);
        if (guard->alive.load()) {
            work();
        }
        guard->running.fetch_sub(1);
    }));
}

void ClientSession::handOverToRegionOf(world::LoadedWorld& world) {
    // Called from the global phase: a player walking across a border must not change owner mid-tick.
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
    // From here this thread owns the socket; sockets block, so a client that stops draining would park any writer.
    outbox_.open();
    outbound_ = std::jthread([this] {
        while (streaming_.load()) {
            bool idle = false;
            if (tickTaskPending_.compare_exchange_strong(idle, true)) {
                // The player's turn goes to the world's owner; it only queues packets, so it never waits on a socket.
                onOwner([this] {
                    chunkStreamer_->sendBudget =
                        std::min(chunkStreamer_->sendBudget + chunkStreamer_->desiredChunksPerTick, kMaxBurstChunks);
                    const auto allowed = static_cast<std::int32_t>(chunkStreamer_->sendBudget);
                    if (allowed > 0) {
                        chunkStreamer_->sendBudget -= static_cast<float>(chunkStreamer_->flushReadyChunks(allowed));
                    }
                    lifecycle_->tickPlayer();
                    chatSpam_.tick();
                    commandSpam_.tick();
                    trades_->tick();
                    if (player::PlayerSession* glider = playerSession();
                        glider != nullptr && currentWorld() != nullptr) {
                        player::gliding::tick(blockInteraction_->breakContext(), *glider, *currentWorld());
                    }
                    entityTracker_->syncPlayers();
                    entityTracker_->syncNpcs();
                    entityTracker_->syncItemEntities();
                    entityTracker_->syncSummonedEntities();
                    entityTracker_->syncGeneratedEntities();
                    uiState_->tickUi();
                    if (menus_->openBlock.has_value() && (menus_->openBlockSize == world::furnaces::kSize ||
                                                          menus_->openBlockSize == world::brewingStands::kSize)) {
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
        // Flushed now: waiting for the outbound loop would add its whole interval to every reply.
        if (outbox_.isOpen()) {
            flushOutbox();
        }
    }
    streaming_.store(false);
    if (carried_ != nullptr) {
        carried_->end();
    }
    lifecycle_->leavePlayIfPresent();
    announceClosed();
    if (closeWhenDrained_.load()) {
        // The goodbye is still queued when the read ended on purpose.
        flushOutbox();
    }
    outbox_.close();
    connection_.close();
}

void ClientSession::drainFrames() {
    wire::io::ByteBuffer frame;
    while (true) {
        frame.clear();
        const std::expected<bool, wire::protocol::FramingError> read = wire::protocol::packetFraming::tryReadFrame(
            incoming_, compressionEnabled_, frame, options_.compressionThreshold);
        if (!read.has_value()) {
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
    if (envelope_.has_value()) {
        handleEnvelopeFrame(body);
        return;
    }
    handleOriginalPacket(body);
}

void ClientSession::handleEnvelopeFrame(wire::io::ByteBuffer& body) {
    namespace envelope = wire::protocol::sessionEnvelope;
    const bool wasActive = envelope_->state() == envelope::EnvelopeEndpoint::State::kActive;
    const envelope::EnvelopeEndpoint::Outcome outcome = envelope_->receive(body.readable());
    if (!outcome.reply.empty()) {
        sendEnvelopeControl(outcome.reply);
    }
    if (outcome.closed) {
        if (carried_ != nullptr) {
            carried_->end();
        }
        connection_.close();
        return;
    }
    if (!wasActive && envelope_->state() == envelope::EnvelopeEndpoint::State::kActive) {
        activateEnvelope(*envelope_->agreed());
        return;
    }
    if (outcome.packet.empty()) {
        return;
    }
    if (carried_ != nullptr) {
        carried_->receive(outcome.packet);
        return;
    }
    wire::io::ByteBuffer original;
    original.writeBytes(outcome.packet);
    handleOriginalPacket(original);
}

class ClientSession::BedrockChannel final : public bedrock::Channel {
public:
    BedrockChannel(ClientSession& session, int protocol) : session_(session), protocol_(protocol) {}

    [[nodiscard]] int protocolVersion() const override { return protocol_; }

    void sendPacket(wire::bedrock::BedrockPacket packet, std::span<const std::uint8_t> body) override {
        if (packet == wire::bedrock::BedrockPacket::Disconnect) {
            // The words are all the proxy needs: it writes the disconnect of the client's own protocol.
            wire::io::ByteBuffer reader;
            reader.writeBytes(body);
            static_cast<void>(wire::bedrock::readVarInt(reader));
            static_cast<void>(wire::bedrock::readVarUInt(reader));
            const std::string message = wire::bedrock::readString(reader);
            session_.sendEnvelopeControl(session_.envelope_->disconnect(reader.failed() ? std::string{} : message));
            return;
        }
        wire::io::ByteBuffer whole{body.size() + 2};
        wire::bedrock::writeVarUInt(whole, static_cast<std::uint32_t>(packet));
        whole.writeBytes(body);
        session_.frameOutgoing(whole.readable());
    }

    void flush() override {}

    void close() override { session_.closeAfterGoodbye(); }

private:
    ClientSession& session_;
    int protocol_;
};

void ClientSession::closeAfterGoodbye() {
    closeWhenDrained_.store(true);
    if (!outbox_.isOpen()) {
        outbox_.close();
        connection_.close();
        return;
    }
    connection_.shutdownRead();
}

void ClientSession::activateEnvelope(const wire::protocol::sessionEnvelope::Open& open) {
    namespace envelope = wire::protocol::sessionEnvelope;
    if (open.edition == envelope::Edition::Bedrock) {
        const std::optional<envelope::BedrockIdentity>& who = envelope_->identity();
        if (!who.has_value() || !options_.carryBedrock) {
            sendEnvelopeControl(envelope_->close(envelope::CloseReason::Unsupported));
            connection_.close();
            return;
        }
        carried_ = options_.carryBedrock(bedrock::Identity{.gamertag = who->gamertag,
                                                           .xuid = who->xuid,
                                                           .authenticated = who->authenticated,
                                                           .address = who->address,
                                                           .protocol = open.protocol,
                                                           .skinId = who->skinId,
                                                           .resourcePatch = who->resourcePatch,
                                                           .skinWidth = who->skinWidth,
                                                           .skinHeight = who->skinHeight,
                                                           .skinPixels = who->skinPixels},
                                         std::make_unique<BedrockChannel>(*this, open.protocol));
        if (carried_ == nullptr) {
            sendEnvelopeControl(envelope_->close(envelope::CloseReason::Unsupported));
            connection_.close();
        }
        return;
    }
    if (open.edition != envelope::Edition::Java || !handshake_.has_value()) {
        sendEnvelopeControl(envelope_->close(envelope::CloseReason::Unsupported));
        connection_.close();
        return;
    }
    verdict_ = options_.protocolGate.verdictFor(open.protocol);
    if (std::holds_alternative<mv::ProtocolVerdict::Refused>(verdict_.value)) {
        sendEnvelopeControl(envelope_->close(envelope::CloseReason::Unsupported));
        connection_.close();
        return;
    }
    handshake_->protocolVersion = open.protocol;
    translation_ = mv::java::javaTranslators::chainFor(open.protocol, options_.substitutionLabels);
    logger_.debug([&open] { return "The proxy carries a client on protocol " + std::to_string(open.protocol) + "."; });
    // Before 1.20.2 there is no configuration phase to acknowledge: the server walks its own by itself.
    if (open.protocol < mv::protocolVersions::kV1_20_2) {
        enterConfiguration();
    }
}

void ClientSession::sendEnvelopeControl(std::span<const std::uint8_t> control) {
    wire::io::ByteBuffer frame;
    static_cast<void>(wire::protocol::packetFraming::writeCompressed(frame, control, options_.compressionThreshold));
    writeToSocket(frame);
}

void ClientSession::frameOutgoing(std::span<const std::uint8_t> whole) {
    wire::io::ByteBuffer frame;
    if (envelope_.has_value()) {
        const std::vector<std::uint8_t> carried = envelope_->data(whole);
        if (carried.empty()) {
            return;
        }
        static_cast<void>(
            wire::protocol::packetFraming::writeCompressed(frame, carried, options_.compressionThreshold));
    } else {
        static_cast<void>(wire::protocol::packetFraming::writeCompressed(frame, whole, options_.compressionThreshold));
    }
    writeToSocket(frame);
}

void ClientSession::handleOriginalPacket(wire::io::ByteBuffer& body) {
    const std::int32_t packetId = wire::protocol::varInt::read(body);
    if (body.failed()) {
        return;
    }
    lastPacketId_ = packetId;
    if (translation_ == nullptr) {
        handleFrameBody(packetId, body);
        return;
    }
    // Older client packets are brought up to what this server reads before anything looks at them.
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
    if (tapListens(true, packetId)) {
        std::optional<std::vector<std::uint8_t>> replacement;
        if (!tap(true, packetId, body.readable(), replacement)) {
            return;
        }
        if (replacement.has_value()) {
            wire::io::ByteBuffer replaced;
            replaced.writeBytes(*replacement);
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
    // parity: `PacketUtils.ensureRunningOnSameThread`.
    wire::io::ByteBuffer owned;
    owned.writeBytes(body.readable());
    onOwner([this, packetId, moved = std::move(owned)]() mutable { handlePlay(packetId, moved); });
}

void ClientSession::sendPlayPacket(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody) {
    // A write failure is swallowed on purpose: the read loop notices the close, and propagating aborted broadcasts.
    sendCompressed(id, writeBody);
}

void ClientSession::sendKeepAliveIfDue() {
    // Vanilla's 15-second interval, driven per connection so a client that stops draining delays only itself.
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
    // Queued frames must reach the wire first; the owner must not park on a socket, so closing is left to this thread.
    closeWhenDrained_.store(true);
    if (!outbox_.isOpen()) {
        outbox_.close();
        connection_.close();
        return;
    }
    // A client that does not hang up would keep the session thread in `read` for as long as it likes.
    connection_.shutdownRead();
}

void ClientSession::sendUncompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody) {
    wire::io::ByteBuffer body;
    wire::protocol::varInt::write(body, id);
    writeBody(body);
    if (tapListens(false, id)) {
        std::optional<std::vector<std::uint8_t>> replacement;
        const std::size_t idLength = static_cast<std::size_t>(wire::protocol::varInt::sizeOf(id));
        if (!tap(false, id, body.readable().subspan(idLength), replacement)) {
            return;
        }
        if (replacement.has_value()) {
            wire::io::ByteBuffer rebuilt;
            wire::protocol::varInt::write(rebuilt, id);
            rebuilt.writeBytes(*replacement);
            body = std::move(rebuilt);
        }
    }
    wire::io::ByteBuffer frame;
    if (!wire::protocol::packetFraming::writeUncompressed(frame, body.readable())) {
        connection_.close();
        return;
    }
    writeToSocket(frame);
}

void ClientSession::sendCompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody) {
    wire::io::ByteBuffer body;
    wire::protocol::varInt::write(body, id);
    writeBody(body);
    dispatchOutgoing(id, body.readable(), true);
}

void ClientSession::sendTranslated(const wire::multiversion::ProtocolChain& chain, std::int32_t id,
                                   std::span<const std::uint8_t> body) {
    // Own buffers, not a shared scratch: a link may answer with several packets and reuse would overwrite the first.
    chain.clientbound(state_, id, body, [this](std::int32_t translatedId, std::span<const std::uint8_t> translated) {
        wire::io::ByteBuffer whole;
        wire::protocol::varInt::write(whole, translatedId);
        whole.writeBytes(translated);
        frameOutgoing(whole.readable());
    });
}

void ClientSession::handleCustomPayload(wire::io::ByteBuffer& body) {
    // Read only when somebody listens on that name: decoding modded channel chatter just to drop it is a cost for all.
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
    // With an outbound thread every frame goes through the outbox; during login and configuration writes go direct.
    const std::span<const std::uint8_t> bytes = frame.readable();
    if (outbox_.isOpen()) {
        static_cast<void>(outbox_.offer(std::vector<std::uint8_t>(bytes.begin(), bytes.end())));
        return;
    }
    // One packet at a time: broadcasts from other threads would interleave mid-packet, and CFB8 is stateful.
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
    // Like vanilla's outbound queue: a stalled client delays only itself and past the limit is disconnected.
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
    // Taking and writing under one lock, or two flushers would write their frames in whichever order they woke.
    const std::lock_guard<std::mutex> guard{sendMutex_};
    for (const std::vector<std::uint8_t>& frame : outbox_.drain()) {
        writeFrame(frame);
    }
}

} // namespace basalt::server::runtime
