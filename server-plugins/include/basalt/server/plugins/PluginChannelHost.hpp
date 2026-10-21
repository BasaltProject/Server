#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/wire/lua/LuaRef.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
class LoadedPlugin;
class PluginHost;

class PluginChannelHost {
public:
    using NativeListener =
        std::function<void(const std::string& channel, const std::string& playerUuid, std::span<const std::uint8_t>)>;

    /// A listener written in C++ rather than Lua, for the layers that bridge another plugin API.
    [[nodiscard]] PluginHandle listenNative(const std::string& channel, NativeListener listener);

    void listenOn(LoadedPlugin& plugin, const std::string& channel, wire::lua::LuaRef fn);

    [[nodiscard]] bool listeningOnChannel(std::string_view channel) const;

    void channelMessage(PluginHost& host, const std::string& channel, const std::string& playerUuid,
                        std::span<const std::uint8_t> bytes);

private:
    struct ChannelListener {
        LoadedPlugin* plugin = nullptr;
        wire::lua::LuaRef fn;
        std::int64_t id = 0;
    };

    struct NativeEntry {
        std::int64_t id = 0;
        NativeListener fn;
    };

    std::map<std::string, std::vector<ChannelListener>, std::less<>> channels_;
    std::map<std::string, std::vector<NativeEntry>, std::less<>> native_;
    std::int64_t nextId_ = 0;
};
} // namespace basalt::server::plugins
