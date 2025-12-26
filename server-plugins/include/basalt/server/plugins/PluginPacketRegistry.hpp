#pragma once

#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

class LoadedPlugin;

/// Who is watching which packet.
///
/// The same two verbs as the event bus and the same five priorities: one kind decides and may
/// cancel, the other only looks. What is different is the promise underneath: with nobody listening
/// to a name, `listening` is a map lookup and the packet is never opened, never copied and never
/// handed to Lua. That is the whole reason this can sit on the path every packet takes.
class PluginPacketRegistry final : public PluginPacketBus {
public:
    /// Hands one packet to one listener, on its own state and its own budget.
    using Deliver = std::function<void(LoadedPlugin& plugin, const lua::LuaRef& fn, PluginPacket& packet)>;

    explicit PluginPacketRegistry(Deliver deliver) : deliver_(std::move(deliver)) {}

    PluginHandle add(LoadedPlugin& plugin, const std::string& name, lua::LuaRef fn, PluginEventPriority priority,
                     bool observer);

    [[nodiscard]] bool listening(std::string_view name) const override;

    [[nodiscard]] PluginPacket& packet() override { return reusable_; }

    bool fire(PluginPacket& packet) override;

private:
    struct Listener {
        LoadedPlugin* plugin = nullptr;
        lua::LuaRef fn;
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
