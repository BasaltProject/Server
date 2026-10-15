#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
struct PluginPacket {
    std::string name;

    bool serverbound = false;

    std::string playerName;
    std::string playerUuid;

    std::vector<std::uint8_t> bytes;

    bool cancelled = false;

    std::optional<std::vector<std::uint8_t>> replacement;

    void clear() {
        cancelled = false;
        replacement.reset();
    }
};

class PluginPacketBus {
public:
    PluginPacketBus() = default;
    PluginPacketBus(const PluginPacketBus&) = delete;
    PluginPacketBus& operator=(const PluginPacketBus&) = delete;
    PluginPacketBus(PluginPacketBus&&) = delete;
    PluginPacketBus& operator=(PluginPacketBus&&) = delete;
    virtual ~PluginPacketBus() = default;

    [[nodiscard]] virtual bool listening(std::string_view name) const = 0;

    virtual bool fire(PluginPacket& packet) = 0;

    [[nodiscard]] virtual PluginPacket& packet() = 0;
};
} // namespace basalt::server::plugins
