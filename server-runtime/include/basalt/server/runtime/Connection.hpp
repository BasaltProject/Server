#pragma once

#include <basalt/wire/auth/Cfb8Cipher.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/io/TcpSocket.hpp>
#include <basalt/wire/protocol/ProtocolState.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace basalt::server::runtime {

/// One client connection and the buffers it owns.
///
/// Both buffers live for the whole connection and are reused packet after packet: a read buffer that
/// accumulates whatever the socket handed over, a write buffer the codecs append into. Nothing per
/// packet is allocated on either side, which is the point of the rewrite.
class Connection {
public:
    explicit Connection(wire::io::TcpConnection socket) : socket_(std::move(socket)) {}

    [[nodiscard]] wire::protocol::ProtocolState state() const noexcept { return state_; }

    void setState(wire::protocol::ProtocolState state) noexcept { state_ = state; }

    [[nodiscard]] std::int32_t compressionThreshold() const noexcept { return compressionThreshold_; }

    void setCompressionThreshold(std::int32_t threshold) noexcept { compressionThreshold_ = threshold; }

    [[nodiscard]] bool compressionEnabled() const noexcept { return compressionThreshold_ >= 0; }

    [[nodiscard]] const std::string& remoteAddress() const noexcept { return socket_.remoteAddress(); }

    [[nodiscard]] bool isOpen() const noexcept { return !socket_.isClosed(); }

    /// Turns on the session cipher from here on, in both directions. The key is the AES-128 shared
    /// secret the client sent, and vanilla reuses it as the IV.
    [[nodiscard]] bool enableEncryption(std::span<const std::uint8_t> secret);

    [[nodiscard]] bool encryptionEnabled() const noexcept { return inbound_.has_value(); }

    /// Pulls whatever the socket has into the read buffer. False once the peer is gone.
    [[nodiscard]] bool fill();

    /// Takes the next complete packet's `id + data` into `into`, if one is buffered.
    [[nodiscard]] bool nextPacket(wire::io::ByteBuffer& into);

    /// Frames and sends one packet body, which must already start with its varint id.
    void send(std::span<const std::uint8_t> packetIdAndData);

    void close() noexcept { socket_.close(); }

    /// A scratch buffer for building one outgoing packet. Cleared on every call, never reallocated.
    [[nodiscard]] wire::io::ByteBuffer& scratch() noexcept {
        scratch_.clear();
        return scratch_;
    }

private:
    wire::io::TcpConnection socket_;
    wire::io::ByteBuffer read_{16 * 1024};
    wire::io::ByteBuffer write_{16 * 1024};
    wire::io::ByteBuffer scratch_{4 * 1024};
    // CFB8 is a stream cipher with one continuous keystream per direction, so the two halves are
    // separate objects and neither may skip a byte.
    std::optional<wire::auth::Cfb8Cipher> inbound_;
    std::optional<wire::auth::Cfb8Cipher> outbound_;
    wire::protocol::ProtocolState state_ = wire::protocol::ProtocolState::Handshake;
    std::int32_t compressionThreshold_ = -1;
};

} // namespace basalt::server::runtime
