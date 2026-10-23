#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerTabList.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

namespace basalt::server::runtime::player {
namespace {

namespace protocol = wire::protocol;

// parity: `ClientboundPlayerInfoUpdatePacket.Action` ordinals.
constexpr std::uint8_t kUpdateListed = 1 << 3;
constexpr std::uint8_t kUpdateDisplayName = 1 << 5;
constexpr std::uint8_t kUpdateListOrder = 1 << 6;

} // namespace

bool hasCustomListEntry(const PlayerSession& subject) {
    const PlayerExtras& extras = subject.extras;
    return extras.unlisted || extras.listName.has_value() || extras.listOrder != 0;
}

void sendListEntry(const PlayerSession& viewer, const PlayerSession& subject, const bool listedForViewer) {
    if (!viewer.connection().sendPlayPacket) {
        return;
    }
    const PlayerExtras& extras = subject.extras;
    viewer.connection().sendPlayPacket(protocol::play::PlayerInfoUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
        buffer.writeByte(kUpdateListed | kUpdateDisplayName | kUpdateListOrder);
        protocol::varInt::write(buffer, 1);
        protocol::writeUuid(buffer, subject.uuid());
        protocol::writeBoolean(buffer, listedForViewer && !extras.unlisted);
        protocol::writeBoolean(buffer, extras.listName.has_value());
        if (extras.listName.has_value()) {
            protocol::writeAnonymousNbt(
                buffer, wire::text::componentNbt::serialize(wire::text::miniMessage::parse(*extras.listName)));
        }
        protocol::varInt::write(buffer, extras.listOrder);
    });
}

void announceListEntry(const PlayerRegistry& registry, const PlayerSession& subject) {
    for (const PlayerSession* viewer : registry.snapshot()) {
        sendListEntry(*viewer, subject, !viewer->extras.unlistedPlayers.contains(subject.entityId()));
    }
}

void sendTabHeaderFooter(const PlayerSession& viewer) {
    if (!viewer.connection().sendPlayPacket) {
        return;
    }
    const auto parse = [](const std::optional<std::string>& text) {
        return text.has_value() ? wire::text::miniMessage::parse(*text) : wire::text::Component{};
    };
    const protocol::play::TabListPacket packet{.header = parse(viewer.extras.tabHeader),
                                               .footer = parse(viewer.extras.tabFooter)};
    viewer.connection().sendPlayPacket(protocol::play::TabListPacket::kId,
                                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

} // namespace basalt::server::runtime::player
