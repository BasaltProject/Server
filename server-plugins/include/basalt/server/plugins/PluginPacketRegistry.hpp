#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/wire/lua/LuaRef.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
class LoadedPlugin;

class PluginPacketRegistry final : public PluginPacketBus {
public:
    using Deliver = std::function<void(LoadedPlugin& plugin, const wire::lua::LuaRef& fn, PluginPacket& packet)>;

    explicit PluginPacketRegistry(Deliver deliver) : deliver_(std::move(deliver)) {}

    PluginHandle add(LoadedPlugin& plugin, const std::string& name, wire::lua::LuaRef fn, PluginEventPriority priority,
                     bool observer);

    [[nodiscard]] bool listening(std::string_view name) const override;

    [[nodiscard]] PluginPacket& packet() override { return reusable_; }

    bool fire(PluginPacket& packet) override;

private:
    struct Listener {
        LoadedPlugin* plugin = nullptr;
        wire::lua::LuaRef fn;
        PluginEventPriority priority = PluginEventPriority::Normal;
        bool observer = false;
        std::int64_t id = 0;
    };

    Deliver deliver_;
    std::map<std::string, std::vector<Listener>, std::less<>> listeners_;
    PluginPacket reusable_;
    std::int64_t nextId_ = 0;
};
} // namespace basalt::server::plugins
