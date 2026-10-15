#include <basalt/server/runtime/Connection.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::runtime {

bool Connection::enableEncryption(std::span<const std::uint8_t> secret) {
    auto inbound = wire::auth::Cfb8Cipher::create(secret);
    auto outbound = wire::auth::Cfb8Cipher::create(secret);
    if (!inbound || !outbound) {
        return false;
    }
    inbound_ = std::move(inbound);
    outbound_ = std::move(outbound);
    return true;
}

bool Connection::fill() {
    std::array<std::uint8_t, 8192> chunk{};
    const auto received = socket_.read(chunk);
    if (!received || *received == 0) {
        return false;
    }

    const std::span<std::uint8_t> arrived{chunk.data(), *received};
    if (inbound_) {
        // Decrypted in place, before framing sees it: the cipher runs under the framing, not over
        // it, so a partial frame decrypts correctly and waits for the rest.
        inbound_->decrypt(arrived);
    }
    read_.writeBytes(arrived);
    return true;
}

bool Connection::nextPacket(wire::io::ByteBuffer& into) {
    const auto got =
        wire::protocol::packetFraming::tryReadFrame(read_, compressionEnabled(), into, compressionThreshold_);
    if (!got) {
        socket_.close();
        return false;
    }
    if (*got) {
        // The frame is out of the way; slide the rest to the front so the buffer stays bounded by
        // one packet rather than by the whole session.
        read_.discardReadBytes();
    }
    return *got;
}

void Connection::send(std::span<const std::uint8_t> packetIdAndData) {
    write_.clear();
    if (compressionEnabled()) {
        if (!wire::protocol::packetFraming::writeCompressed(write_, packetIdAndData, compressionThreshold_)) {
            socket_.close();
            return;
        }
    } else {
        if (!wire::protocol::packetFraming::writeUncompressed(write_, packetIdAndData)) {
            socket_.close();
            return;
        }
    }
    if (outbound_) {
        outbound_->encrypt(write_.mutableSpan(0));
    }
    static_cast<void>(socket_.write(write_.readable()));
}

} // namespace basalt::server::runtime
