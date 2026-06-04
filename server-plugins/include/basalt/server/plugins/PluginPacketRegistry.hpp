#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/wire/lua/LuaRef.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace basalt::server::plugins {
class LoadedPlugin;

class PluginPacketRegistry final : public PluginPacketBus {
public:
    using Deliver = std::function<void(LoadedPlugin& plugin, const wire::lua::LuaRef& fn, PluginPacket& packet)>;

    /// Gives a Lua listener the vanilla name of the packet it is about to see.
    using Namer = std::function<std::string(const PacketAddress&)>;

    PluginPacketRegistry(Deliver deliver, Namer namer) : deliver_(std::move(deliver)), namer_(std::move(namer)) {}

    PluginHandle add(LoadedPlugin& plugin, const PacketScope& scope, wire::lua::LuaRef fn, PluginEventPriority priority,
                     bool observer);

    [[nodiscard]] bool listening(const PacketAddress& address) const noexcept override;

    bool fire(PluginPacket& packet) override;

    [[nodiscard]] PluginHandle addNative(const PacketScope& scope, std::int32_t priority, bool observer,
                                         NativePacketListener listener) override;

    void connectionClosed(std::uint64_t connection) override;

    [[nodiscard]] PluginHandle addCloseListener(std::function<void(std::uint64_t connection)> listener) override;

private:
    struct Listener {
        PacketScope scope;
        LoadedPlugin* plugin = nullptr;
        wire::lua::LuaRef fn;
        NativePacketListener native;
        std::int32_t priority = 0;
        bool observer = false;
        std::int64_t id = 0;
    };

    using Snapshot = std::vector<std::shared_ptr<const Listener>>;

    static constexpr std::int32_t kCountedIds = 256;

    PluginHandle insert(std::shared_ptr<Listener> listener);
    void remove(std::int64_t id);
    void count(const PacketScope& scope, std::int32_t delta);

    Deliver deliver_;
    Namer namer_;
    std::mutex mutex_;
    std::shared_ptr<const Snapshot> listeners_ = std::make_shared<const Snapshot>();
    std::vector<std::pair<std::int64_t, std::function<void(std::uint64_t)>>> closers_;
    std::int64_t nextId_ = 0;

    // Read without a lock by every packet: how many listeners could want it.
    std::atomic<std::int32_t> everything_{0};
    std::array<std::atomic<std::int32_t>, 2> perDirection_{};
    std::array<std::atomic<std::int32_t>, 2 * kPacketPhaseCount> perPhase_{};
    std::array<std::atomic<std::int32_t>, 2 * kPacketPhaseCount * kCountedIds> perId_{};
    std::atomic<std::int32_t> oddIds_{0};
};
} // namespace basalt::server::plugins
