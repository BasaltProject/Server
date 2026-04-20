#include <basalt/server/plugins/PluginChannelHost.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
void PluginChannelHost::listenOn(LoadedPlugin& plugin, const std::string& channel, wire::lua::LuaRef fn) {
    const std::int64_t id = nextId_++;
    channels_[channel].push_back(ChannelListener{.plugin = &plugin, .fn = std::move(fn), .id = id});
    static_cast<void>(plugin.registrations.add("channel(" + channel + ")", [this, channel, id] {
        const auto found = channels_.find(channel);
        if (found == channels_.end()) {
            return;
        }
        std::erase_if(found->second, [id](const ChannelListener& listener) { return listener.id == id; });
        if (found->second.empty()) {
            channels_.erase(found);
        }
    }));
}

bool PluginChannelHost::listeningOnChannel(std::string_view channel) const { return channels_.contains(channel); }

void PluginChannelHost::channelMessage(PluginHost& host, const std::string& channel, const std::string& playerUuid,
                                       std::span<const std::uint8_t> bytes) {
    const auto found = channels_.find(channel);
    if (found == channels_.end()) {
        return;
    }
    const std::shared_ptr<PluginPlayerRef> player = host.services().players().byUuid(playerUuid);
    // A copy, because a listener may register or drop one while this runs.
    std::vector<std::pair<LoadedPlugin*, const wire::lua::LuaRef*>> targets;
    targets.reserve(found->second.size());
    for (const ChannelListener& listener : found->second) {
        targets.emplace_back(listener.plugin, &listener.fn);
    }
    for (const auto& [plugin, fn] : targets) {
        if (plugin == nullptr || plugin->disabled) {
            continue;
        }
        static_cast<void>(host.ask(*plugin, *fn, "channel " + channel,
                                   [&host, plugin, &player, bytes](wire::lua::LuaArgs& args) {
                                       if (player == nullptr) {
                                           args.pushNil();
                                       } else {
                                           pushPlayer(args, player, &host.services().storage(), plugin->manifest.id,
                                                      &plugin->text);
                                       }
                                       args.pushTable();
                                       std::int64_t position = 1;
                                       for (const std::uint8_t byte : bytes) {
                                           args.pushInteger(byte);
                                           args.setIndex(position++);
                                       }
                                       return 2;
                                   },
                                   {}));
    }
}
} // namespace basalt::server::plugins
