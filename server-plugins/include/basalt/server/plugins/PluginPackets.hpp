#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

/// One packet on its way past a plugin.
///
/// Reused between deliveries the way an event is, for the same reason: a packet listener sits on the
/// busiest path in the server, and a table per packet per player is not a cost anybody would accept
/// for the ability to watch one.
struct PluginPacket {
    std::string name;

    /// True on the way in, false on the way out.
    bool serverbound = false;

    std::string playerName;
    std::string playerUuid;

    /// The packet's own bytes, without the id in front.
    ///
    /// The escape hatch, not the road: a plugin that only wants to see one packet go by should not
    /// have to decode it, and one that wants a field this build does not name yet can still have it.
    std::vector<std::uint8_t> bytes;

    bool cancelled = false;

    /// Set when a listener replaced the body. Empty means it goes on unchanged.
    std::optional<std::vector<std::uint8_t>> replacement;

    void clear() {
        cancelled = false;
        replacement.reset();
    }
};

/// Whether anybody is listening at all, which is what keeps a packet from being opened for nothing.
class PluginPacketBus {
public:
    PluginPacketBus() = default;
    PluginPacketBus(const PluginPacketBus&) = delete;
    PluginPacketBus& operator=(const PluginPacketBus&) = delete;
    PluginPacketBus(PluginPacketBus&&) = delete;
    PluginPacketBus& operator=(PluginPacketBus&&) = delete;
    virtual ~PluginPacketBus() = default;

    [[nodiscard]] virtual bool listening(std::string_view name) const = 0;

    /// Runs every listener on `packet` and answers whether it should still be sent or handled.
    virtual bool fire(PluginPacket& packet) = 0;

    /// The reusable packet, so firing one allocates nothing.
    [[nodiscard]] virtual PluginPacket& packet() = 0;
};

} // namespace basalt::server::plugins
