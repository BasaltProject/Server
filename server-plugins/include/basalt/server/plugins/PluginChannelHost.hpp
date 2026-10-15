#pragma once

#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>

#include <cstdint>
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
    void listenOn(LoadedPlugin& plugin, const std::string& channel, lua::LuaRef fn);

    [[nodiscard]] bool listeningOnChannel(std::string_view channel) const;

    void channelMessage(PluginHost& host, const std::string& channel, const std::string& playerUuid,
                        std::span<const std::uint8_t> bytes);

private:
    struct ChannelListener {
        LoadedPlugin* plugin = nullptr;
        lua::LuaRef fn;
        std::int64_t id = 0;
    };

    std::map<std::string, std::vector<ChannelListener>, std::less<>> channels_;
    std::int64_t nextId_ = 0;
};
} // namespace basalt::server::plugins
