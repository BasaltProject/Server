#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/server/runtime/chat/ChatMessage.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/PlayerOps.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <string_view>

namespace basalt::server::runtime::plugins::playerOps {
namespace {

namespace play = wire::protocol::play;

constexpr std::string_view kPrefix = "receive_";

[[nodiscard]] play::Hand handOf(const Value& value) {
    return asInt(value) == 1 ? play::Hand::OffHand : play::Hand::MainHand;
}

[[nodiscard]] play::BlockPos blockOf(const Args& args, const std::size_t at) {
    return play::BlockPos{static_cast<std::int32_t>(asInt(arg(args, at))),
                          static_cast<std::int32_t>(asInt(arg(args, at + 1))),
                          static_cast<std::int32_t>(asInt(arg(args, at + 2)))};
}

template <typename Packet>
bool deliver(const player::PlayerConnection& connection, const std::int32_t id, const Packet& packet) {
    if (!connection.packetEndpoint) {
        return false;
    }
    const std::shared_ptr<server::plugins::PacketEndpoint> endpoint = connection.packetEndpoint();
    if (endpoint == nullptr) {
        return false;
    }
    wire::io::ByteBuffer buffer;
    packet.write(buffer);
    const std::span<const std::uint8_t> bytes = buffer.readable();
    return endpoint->receiveServerbound(id, bytes, false);
}

template <typename Packet>
bool deliver(const player::PlayerConnection& connection, const Packet& packet) {
    return deliver(connection, Packet::kId, packet);
}

} // namespace

std::optional<Value> packets(Context& context, const Verb verb, const std::string_view name, const Args& args) {
    if (verb != Verb::Call || !name.starts_with(kPrefix)) {
        return std::nullopt;
    }
    const player::PlayerConnection& connection = context.session.connection();
    const std::string_view what = name.substr(kPrefix.size());
    const Value& first = arg(args, 0);
    if (what == "use_item") {
        return deliver(connection,
                       play::ServerboundUseItemPacket{.hand = handOf(first),
                                                      .sequence = static_cast<std::int32_t>(asInt(arg(args, 3))),
                                                      .yaw = static_cast<float>(asReal(arg(args, 1))),
                                                      .pitch = static_cast<float>(asReal(arg(args, 2)))});
    }
    if (what == "use_item_on") {
        play::BlockHitResult hit;
        hit.pos = blockOf(args, 1);
        hit.direction = static_cast<play::Direction>(asInt(arg(args, 4)) % 6);
        hit.clickX = static_cast<float>(asReal(arg(args, 5)));
        hit.clickY = static_cast<float>(asReal(arg(args, 6)));
        hit.clickZ = static_cast<float>(asReal(arg(args, 7)));
        hit.inside = asBool(arg(args, 8));
        return deliver(connection,
                       play::ServerboundUseItemOnPacket{.hand = handOf(first),
                                                        .blockHit = hit,
                                                        .sequence = static_cast<std::int32_t>(asInt(arg(args, 9)))});
    }
    if (what == "container_click") {
        return deliver(connection,
                       play::ServerboundContainerClickPacket{
                           .containerId = static_cast<std::int32_t>(asInt(first)),
                           .stateId = static_cast<std::int32_t>(asInt(arg(args, 1))),
                           .slot = static_cast<std::int16_t>(asInt(arg(args, 2))),
                           .button = static_cast<std::int8_t>(asInt(arg(args, 3))),
                           .mode = static_cast<play::ServerboundContainerClickPacket::Mode>(asInt(arg(args, 4)) % 7),
                           .clientHasCarriedItem = asBool(arg(args, 5))});
    }
    if (what == "interact") {
        return deliver(connection, play::InteractPacket{.entityId = static_cast<std::int32_t>(asInt(first)),
                                                        .hand = static_cast<std::int32_t>(asInt(arg(args, 1))),
                                                        .x = asReal(arg(args, 2)),
                                                        .y = asReal(arg(args, 3)),
                                                        .z = asReal(arg(args, 4)),
                                                        .usingSecondaryAction = asBool(arg(args, 5))});
    }
    if (what == "attack") {
        return deliver(connection, play::AttackPacket{.entityId = static_cast<std::int32_t>(asInt(first))});
    }
    if (what == "custom_payload") {
        const std::string data = asText(arg(args, 1));
        return deliver(connection, play::CustomPayloadPacket::kServerboundId,
                       play::CustomPayloadPacket{.channel = asText(first),
                                                 .data = std::vector<std::uint8_t>{data.begin(), data.end()}});
    }
    if (what == "chat") {
        return deliver(connection, play::ChatMessagePacket{.message = asText(first), .timestamp = chat::nowMillis()});
    }
    if (what == "edit_book") {
        play::ServerboundEditBookPacket packet{.slot = static_cast<std::int32_t>(asInt(first))};
        if (const auto* pages = std::get_if<std::vector<std::string>>(&arg(args, 1)); pages != nullptr) {
            packet.pages = *pages;
        }
        if (!isNone(arg(args, 2))) {
            packet.title = asText(arg(args, 2));
        }
        return deliver(connection, packet);
    }
    if (what == "chat_command") {
        return deliver(connection, play::ChatCommandPacket{.command = asText(first)});
    }
    if (what == "move") {
        return deliver(connection,
                       play::ServerboundPositionLookPacket{.x = asReal(first),
                                                           .y = asReal(arg(args, 1)),
                                                           .z = asReal(arg(args, 2)),
                                                           .yaw = static_cast<float>(asReal(arg(args, 3))),
                                                           .pitch = static_cast<float>(asReal(arg(args, 4))),
                                                           .onGround = asBool(arg(args, 5))});
    }
    if (what == "pick_block") {
        return deliver(connection,
                       play::PickItemFromBlockPacket{.pos = blockOf(args, 0), .includeData = asBool(arg(args, 3))});
    }
    if (what == "pick_entity") {
        return deliver(connection, play::PickItemFromEntityPacket{.entityId = static_cast<std::int32_t>(asInt(first)),
                                                                  .includeData = asBool(arg(args, 1))});
    }
    if (what == "place_recipe") {
        return deliver(connection, play::PlaceRecipePacket{.containerId = static_cast<std::int32_t>(asInt(first)),
                                                           .recipeId = static_cast<std::int32_t>(asInt(arg(args, 1))),
                                                           .useMaxItems = asBool(arg(args, 2))});
    }
    if (what == "creative_slot") {
        play::ServerboundSetCreativeModeSlotPacket packet{.slot = static_cast<std::int16_t>(asInt(first))};
        packet.item = context.session.inventory.toWireStack(decodeItem(asText(arg(args, 1))));
        return deliver(connection, packet);
    }
    if (what == "sign_update") {
        play::SignUpdatePacket packet;
        packet.pos = blockOf(args, 0);
        packet.isFrontText = asBool(arg(args, 3));
        for (std::size_t line = 0; line < play::SignUpdatePacket::kLines; ++line) {
            packet.lines[line] = asText(arg(args, 4 + line));
        }
        return deliver(connection, packet);
    }
    if (what == "rename_item") {
        return deliver(connection, play::ServerboundRenameItemPacket{.name = asText(first)});
    }
    if (what == "beacon") {
        play::ServerboundSetBeaconPacket packet;
        if (!isNone(first)) {
            packet.primary = static_cast<std::int32_t>(asInt(first));
        }
        if (!isNone(arg(args, 1))) {
            packet.secondary = static_cast<std::int32_t>(asInt(arg(args, 1)));
        }
        return deliver(connection, packet);
    }
    if (what == "select_trade") {
        return deliver(connection, play::SelectTradePacket{.index = static_cast<std::int32_t>(asInt(first))});
    }
    if (what == "container_button") {
        return deliver(connection,
                       play::ContainerButtonClickPacket{.containerId = static_cast<std::int32_t>(asInt(first)),
                                                        .buttonId = static_cast<std::int32_t>(asInt(arg(args, 1)))});
    }
    if (what == "container_close") {
        return deliver(connection, play::ContainerClosePacket::kServerboundId,
                       play::ContainerClosePacket{.containerId = static_cast<std::int32_t>(asInt(first))});
    }
    if (what == "held_slot") {
        return deliver(connection,
                       play::ServerboundSetCarriedItemPacket{.slot = static_cast<std::int16_t>(asInt(first))});
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::playerOps
