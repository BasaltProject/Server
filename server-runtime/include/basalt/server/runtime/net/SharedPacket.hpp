#pragma once

#include <basalt/server/runtime/net/SessionOutbox.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace basalt::server::runtime::net {

/// One packet written once and framed once per framing setup, for a broadcast to many plain connections.
class SharedPacket {
public:
    SharedPacket(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody);

    [[nodiscard]] std::int32_t id() const noexcept { return id_; }

    [[nodiscard]] std::span<const std::uint8_t> payload() const noexcept;

    /// Null when the packet cannot be framed. Only the thread that built the packet may call it.
    [[nodiscard]] SessionOutbox::SharedFrame frame(bool compressed, std::int32_t threshold, std::int32_t level) const;

private:
    struct Built {
        bool compressed;
        std::int32_t threshold;
        std::int32_t level;
        SessionOutbox::SharedFrame frame;
    };

    std::int32_t id_;
    std::size_t idLength_;
    wire::io::ByteBuffer body_;
    mutable std::vector<Built> built_;
};

} // namespace basalt::server::runtime::net
