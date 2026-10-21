#include <basalt/server/plugins/PluginObject.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginPacketView.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;

[[nodiscard]] std::vector<std::uint8_t> bytesFrom(LuaArgs& lua, std::int32_t index) {
    if (!lua.isTable(index)) {
        lua.raise("packet bytes are a list of numbers from 0 to 255");
    }
    std::vector<std::uint8_t> out;
    const std::int64_t length = lua.length(index);
    out.reserve(static_cast<std::size_t>(length));
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        out.push_back(static_cast<std::uint8_t>(lua.toInteger(-1) & 0xFF));
        lua.pop(1);
    }
    return out;
}

[[nodiscard]] std::string_view phaseName(PacketPhase phase) {
    switch (phase) {
    case PacketPhase::Handshake:
        return "handshake";
    case PacketPhase::Status:
        return "status";
    case PacketPhase::Login:
        return "login";
    case PacketPhase::Configuration:
        return "configuration";
    case PacketPhase::Play:
        return "play";
    }
    return "play";
}

// Sends or injects another packet on the connection this one travels on, past the listeners unless asked.
[[nodiscard]] bool relay(LuaArgs& args, std::int32_t base, PluginHost& host, const PluginPacket& from, bool inbound) {
    if (from.endpoint == nullptr) {
        return false;
    }
    const std::string name = args.checkString(base);
    const std::optional<PacketAddress> address = host.services().packets().resolve(name);
    if (!address.has_value() || address->serverbound != inbound) {
        args.raise("'" + name + "' is not a " + (inbound ? "serverbound" : "clientbound") + " packet");
    }
    const std::vector<std::uint8_t> body = bytesFrom(args, base + 1);
    const bool intercept = args.isBoolean(base + 2) && args.booleanOr(base + 2, false);
    return inbound ? from.endpoint->receiveServerbound(address->id, body, intercept)
                   : from.endpoint->sendClientbound(address->id, body, intercept);
}
} // namespace

void pushPacket(LuaArgs& lua, PluginPacket& packet, PluginHost& host, LoadedPlugin& plugin) {
    PluginPacket* target = &packet;
    PluginHost* server = &host;
    LoadedPlugin* owner = &plugin;
    PluginObject object{"packet"};
    object.reads("name", [target] { return PluginValue{target->name}; })
        .reads("serverbound", [target] { return PluginValue{target->address.serverbound}; })
        .reads("phase", [target] { return PluginValue{std::string{phaseName(target->address.phase)}}; })
        .reads("id", [target] { return PluginValue{static_cast<std::int64_t>(target->address.id)}; })
        .reads("connection", [target] { return PluginValue{static_cast<std::int64_t>(target->connection)}; })
        .reads("protocol", [target] { return PluginValue{static_cast<std::int64_t>(target->clientProtocol)}; })
        .reads("translated", [target] { return PluginValue{target->translated}; })
        .reads("size", [target] { return PluginValue{static_cast<std::int64_t>(target->bytes.size())}; })
        .reads("player_name", [target] { return PluginValue{target->playerName}; })
        .reads("player_uuid", [target] { return PluginValue{target->playerUuid}; })
        .reads("cancelled", [target] { return PluginValue{target->cancelled}; })
        .pushes("player",
                [target, server, owner](LuaArgs& args) {
                    const std::shared_ptr<PluginPlayerRef> player =
                        server->services().players().byUuid(target->playerUuid);
                    if (player == nullptr) {
                        args.pushNil();
                        return;
                    }
                    pushPlayer(args, player, &server->services().storage(), owner->manifest.id, &owner->text);
                })
        .pushes("bytes",
                [target](LuaArgs& args) {
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const std::uint8_t byte : target->bytes) {
                        args.pushInteger(byte);
                        args.setIndex(position++);
                    }
                })
        .writes("cancelled",
                [target](LuaArgs& args, std::int32_t index) { target->cancelled = args.booleanOr(index, true); })
        .writes("bytes", [target](LuaArgs& args, std::int32_t index) { target->replacement = bytesFrom(args, index); })
        .method("cancel",
                [target](LuaArgs& args) {
                    static_cast<void>(args);
                    target->cancelled = true;
                    return 0;
                })
        .method("send",
                [target, server](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(relay(args, base, *server, *target, false));
                    return 1;
                })
        .method("receive", [target, server](LuaArgs& args) {
            const std::int32_t base = firstArgument(args);
            args.pushBoolean(relay(args, base, *server, *target, true));
            return 1;
        });
    std::move(object).push(lua);
}
} // namespace basalt::server::plugins
