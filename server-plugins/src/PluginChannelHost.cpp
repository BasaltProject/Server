#include <basalt/server/plugins/PluginChannelHost.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
PluginHandle PluginChannelHost::listenNative(const std::string& channel, NativeListener listener) {
    const std::lock_guard<std::mutex> guard{mutex_};
    const std::int64_t id = nextId_++;
    native_[channel].push_back(NativeEntry{.id = id, .fn = std::move(listener)});
    return PluginHandle{[this, channel, id] {
        const std::lock_guard<std::mutex> inner{mutex_};
        const auto found = native_.find(channel);
        if (found == native_.end()) {
            return;
        }
        std::erase_if(found->second, [id](const NativeEntry& entry) { return entry.id == id; });
        if (found->second.empty()) {
            native_.erase(found);
        }
    }};
}

bool PluginChannelHost::listeningOnChannel(std::string_view channel) const {
    const std::lock_guard<std::mutex> guard{mutex_};
    return native_.contains(channel);
}

void PluginChannelHost::channelMessage(const std::string& channel, const std::string& playerUuid,
                                       std::span<const std::uint8_t> bytes) {
    // A copy, because a listener may register or drop one while this runs.
    std::vector<NativeListener> listeners;
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        const auto natives = native_.find(channel);
        if (natives == native_.end()) {
            return;
        }
        for (const NativeEntry& entry : natives->second) {
            listeners.push_back(entry.fn);
        }
    }
    for (const NativeListener& listener : listeners) {
        listener(channel, playerUuid, bytes);
    }
}
} // namespace basalt::server::plugins
