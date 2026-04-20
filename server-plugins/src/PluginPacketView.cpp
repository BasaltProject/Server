#include <basalt/server/plugins/PluginObject.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginPacketView.hpp>

#include <cstdint>
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
} // namespace

void pushPacket(LuaArgs& lua, PluginPacket& packet, PluginHost& host, LoadedPlugin& plugin) {
    PluginPacket* target = &packet;
    PluginHost* server = &host;
    LoadedPlugin* owner = &plugin;
    PluginObject object{"packet"};
    object.reads("name", [target] { return PluginValue{target->name}; })
        .reads("serverbound", [target] { return PluginValue{target->serverbound}; })
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
        .method("cancel", [target](LuaArgs& args) {
            static_cast<void>(args);
            target->cancelled = true;
            return 0;
        });
    std::move(object).push(lua);
}
} // namespace basalt::server::plugins
