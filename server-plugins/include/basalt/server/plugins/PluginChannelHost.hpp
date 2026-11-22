#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
class PluginChannelHost {
public:
    using NativeListener =
        std::function<void(const std::string& channel, const std::string& playerUuid, std::span<const std::uint8_t>)>;

    [[nodiscard]] PluginHandle listenNative(const std::string& channel, NativeListener listener);

    [[nodiscard]] bool listeningOnChannel(std::string_view channel) const;

    void channelMessage(const std::string& channel, const std::string& playerUuid, std::span<const std::uint8_t> bytes);

private:
    struct NativeEntry {
        std::int64_t id = 0;
        NativeListener fn;
    };

    mutable std::mutex mutex_;
    std::map<std::string, std::vector<NativeEntry>, std::less<>> native_;
    std::int64_t nextId_ = 0;
};
} // namespace basalt::server::plugins
