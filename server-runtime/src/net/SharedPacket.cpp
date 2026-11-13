#include <basalt/server/runtime/net/SharedPacket.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <memory>
#include <utility>

namespace basalt::server::runtime::net {

SharedPacket::SharedPacket(const std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody)
    : id_(id), idLength_(static_cast<std::size_t>(wire::protocol::varInt::sizeOf(id))) {
    wire::protocol::varInt::write(body_, id);
    writeBody(body_);
}

std::span<const std::uint8_t> SharedPacket::payload() const noexcept { return body_.readable().subspan(idLength_); }

SessionOutbox::SharedFrame SharedPacket::frame(const bool compressed, const std::int32_t threshold,
                                               const std::int32_t level) const {
    for (const Built& built : built_) {
        if (built.compressed == compressed && built.threshold == threshold && built.level == level) {
            return built.frame;
        }
    }
    wire::io::ByteBuffer framed{body_.readableBytes() + 8};
    const bool written =
        compressed
            ? wire::protocol::packetFraming::writeCompressed(framed, body_.readable(), threshold, level).has_value()
            : wire::protocol::packetFraming::writeUncompressed(framed, body_.readable()).has_value();
    SessionOutbox::SharedFrame result =
        written ? std::make_shared<const SessionOutbox::Frame>(framed.toVector()) : nullptr;
    built_.push_back(Built{.compressed = compressed, .threshold = threshold, .level = level, .frame = result});
    return result;
}

} // namespace basalt::server::runtime::net
