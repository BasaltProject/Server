#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <array>
#include <atomic>
#include <mutex>
#include <thread>

namespace basalt::server::runtime {
namespace {
using server::plugins::PacketAddress;
using server::plugins::PacketPhase;
using server::plugins::PluginPacket;

std::atomic<std::uint64_t> gNextConnection{1};

// A listener that sends another packet re-enters on the same thread, so the scratch packets are a small stack.
struct Scratch {
    std::array<PluginPacket, 4> packets;
    std::size_t depth = 0;
};

thread_local Scratch tScratch;

class Lease {
public:
    Lease() : packet_(tScratch.depth < tScratch.packets.size() ? &tScratch.packets[tScratch.depth] : &spare_) {
        tScratch.depth++;
    }
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;
    ~Lease() { tScratch.depth--; }

    [[nodiscard]] PluginPacket& packet() noexcept { return *packet_; }

private:
    PluginPacket spare_;
    PluginPacket* packet_;
};
} // namespace

class ClientSession::Endpoint final : public server::plugins::PacketEndpoint {
public:
    explicit Endpoint(ClientSession& session) : id_(session.connectionId_), session_(&session) {}

    // Waits for calls in flight, so the session can go away; none of them holds a lock while it works.
    void detach() {
        {
            const std::lock_guard<std::mutex> guard{mutex_};
            session_ = nullptr;
        }
        while (inFlight_.load() > 0) {
            std::this_thread::yield();
        }
    }

    bool sendClientbound(std::int32_t id, std::span<const std::uint8_t> body, bool intercept) override {
        const Pin pin{*this};
        return pin.session != nullptr && pin.session->sendRaw(id, body, intercept);
    }

    bool receiveServerbound(std::int32_t id, std::span<const std::uint8_t> body, bool intercept) override {
        const Pin pin{*this};
        return pin.session != nullptr && pin.session->receiveRaw(id, body, intercept);
    }

    void close() override {
        const Pin pin{*this};
        if (pin.session != nullptr) {
            pin.session->connection_.close();
        }
    }

    [[nodiscard]] std::uint64_t connectionId() const override { return id_; }

    [[nodiscard]] std::string address() const override {
        const Pin pin{*this};
        return pin.session == nullptr ? std::string{} : pin.session->connection_.remoteAddress();
    }

    [[nodiscard]] std::uint16_t port() const override {
        const Pin pin{*this};
        return pin.session == nullptr ? 0 : pin.session->connection_.remotePort();
    }

private:
    struct Pin {
        explicit Pin(const Endpoint& owner) : owner_(&owner) {
            const std::lock_guard<std::mutex> guard{owner.mutex_};
            session = owner.session_;
            if (session != nullptr) {
                owner.inFlight_.fetch_add(1);
            }
        }
        Pin(const Pin&) = delete;
        Pin& operator=(const Pin&) = delete;
        ~Pin() {
            if (session != nullptr) {
                owner_->inFlight_.fetch_sub(1);
            }
        }

        const Endpoint* owner_;
        ClientSession* session = nullptr;
    };

    mutable std::mutex mutex_;
    mutable std::atomic<std::int32_t> inFlight_{0};
    std::uint64_t id_ = 0;
    ClientSession* session_;
};

void ClientSession::attachEndpoint() {
    connectionId_ = gNextConnection.fetch_add(1);
    endpoint_ = std::make_shared<Endpoint>(*this);
}

std::shared_ptr<server::plugins::PacketEndpoint> ClientSession::endpoint() const { return endpoint_; }

void ClientSession::detachEndpoint() {
    if (endpoint_ != nullptr) {
        endpoint_->detach();
    }
}

void ClientSession::announceClosed() {
    if (options_.plugins.packets != nullptr) {
        options_.plugins.packets->connectionClosed(connectionId_);
    }
}

bool ClientSession::tapListens(bool serverbound, std::int32_t id) const noexcept {
    return options_.plugins.packets != nullptr &&
           options_.plugins.packets->listening(PacketAddress{static_cast<PacketPhase>(state_), serverbound, id});
}

bool ClientSession::tap(bool serverbound, std::int32_t id, std::span<const std::uint8_t> body,
                        std::optional<std::vector<std::uint8_t>>& replacement) {
    Lease lease;
    PluginPacket& packet = lease.packet();
    packet.address = PacketAddress{static_cast<PacketPhase>(state_), serverbound, id};
    packet.connection = connectionId_;
    packet.clientProtocol = handshake_.has_value() ? handshake_->protocolVersion : 0;
    packet.translated = translation_ != nullptr;
    const player::PlayerSession* player = playerSession_.get();
    if (player != nullptr) {
        packet.playerName = player->name();
        packet.playerUuid = player->uuid().toString();
    } else {
        packet.playerName.clear();
        packet.playerUuid.clear();
    }
    packet.bytes.assign(body.begin(), body.end());
    packet.endpoint = endpoint_;
    const bool pass = options_.plugins.packets->fire(packet);
    packet.endpoint.reset();
    if (!pass) {
        return false;
    }
    if (packet.replacement.has_value() && *packet.replacement != packet.bytes) {
        replacement = std::move(packet.replacement);
    }
    return true;
}

void ClientSession::dispatchOutgoing(std::int32_t id, std::span<const std::uint8_t> whole, bool intercept) {
    std::vector<std::uint8_t> rebuilt;
    if (intercept && tapListens(false, id)) {
        const std::size_t idLength = static_cast<std::size_t>(wire::protocol::varInt::sizeOf(id));
        std::optional<std::vector<std::uint8_t>> replacement;
        if (!tap(false, id, whole.subspan(idLength), replacement)) {
            return;
        }
        if (replacement.has_value()) {
            rebuilt.assign(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(idLength));
            rebuilt.insert(rebuilt.end(), replacement->begin(), replacement->end());
            whole = rebuilt;
        }
    }
    if (translation_ != nullptr) {
        // After the plugins on purpose: they see the server's own version shape and translation happens on the way out.
        const std::size_t idLength = static_cast<std::size_t>(wire::protocol::varInt::sizeOf(id));
        sendTranslated(translation_->chain(), id, whole.subspan(idLength));
        return;
    }
    frameOutgoing(whole);
}

bool ClientSession::sendRaw(std::int32_t id, std::span<const std::uint8_t> body, bool intercept) {
    wire::io::ByteBuffer whole{body.size() + 5};
    wire::protocol::varInt::write(whole, id);
    whole.writeBytes(body);
    dispatchOutgoing(id, whole.readable(), intercept);
    return true;
}

bool ClientSession::receiveRaw(std::int32_t id, std::span<const std::uint8_t> body, bool intercept) {
    wire::io::ByteBuffer buffer{body.size()};
    buffer.writeBytes(body);
    lastPacketId_ = id;
    if (!intercept) {
        switch (state_) {
        case wire::protocol::ProtocolState::Handshake:
            handleHandshake(id, buffer);
            return true;
        case wire::protocol::ProtocolState::Status:
            handleStatus(id, buffer);
            return true;
        case wire::protocol::ProtocolState::Login:
            handleLogin(id, buffer);
            return true;
        case wire::protocol::ProtocolState::Configuration:
            handleConfiguration(id, buffer);
            return true;
        case wire::protocol::ProtocolState::Play: {
            onOwner([this, id, moved = std::move(buffer)]() mutable { handlePlay(id, moved); });
            return true;
        }
        }
        return false;
    }
    handleFrameBody(id, buffer);
    return true;
}
} // namespace basalt::server::runtime
