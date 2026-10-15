#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginPacketRegistry.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
PluginHandle PluginPacketRegistry::add(LoadedPlugin& plugin, const std::string& name, lua::LuaRef fn,
                                       PluginEventPriority priority, bool observer) {
    const std::int64_t id = nextId_++;
    std::vector<Listener>& list = listeners_[name];
    list.push_back(
        Listener{.plugin = &plugin, .fn = std::move(fn), .priority = priority, .observer = observer, .id = id});
    std::ranges::stable_sort(list, [](const Listener& left, const Listener& right) {
        if (left.observer != right.observer) {
            return !left.observer;
        }
        return static_cast<std::uint8_t>(left.priority) < static_cast<std::uint8_t>(right.priority);
    });
    return PluginHandle{[this, name, id] {
        const auto found = listeners_.find(name);
        if (found == listeners_.end()) {
            return;
        }
        std::erase_if(found->second, [id](const Listener& listener) { return listener.id == id; });
        if (found->second.empty()) {
            listeners_.erase(found);
        }
    }};
}

bool PluginPacketRegistry::listening(std::string_view name) const { return listeners_.contains(name); }

bool PluginPacketRegistry::fire(PluginPacket& packet) {
    const auto found = listeners_.find(packet.name);
    if (found == listeners_.end()) {
        return true;
    }
    packet.clear();
    // The listeners by address, because delivering one may register or drop another.
    std::vector<std::pair<LoadedPlugin*, const lua::LuaRef*>> targets;
    targets.reserve(found->second.size());
    std::vector<bool> observers;
    observers.reserve(found->second.size());
    for (const Listener& listener : found->second) {
        targets.emplace_back(listener.plugin, &listener.fn);
        observers.push_back(listener.observer);
    }
    for (std::size_t index = 0; index < targets.size(); index++) {
        if (packet.cancelled && !observers[index]) {
            continue;
        }
        if (targets[index].first != nullptr && deliver_) {
            deliver_(*targets[index].first, *targets[index].second, packet);
        }
    }
    return !packet.cancelled;
}
} // namespace basalt::server::plugins
