#pragma once

#include <basalt/server/plugins/PluginHost.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace basalt::server::plugins {

/// Lets a registration that other threads call into stop running the plugin's Lua before the plugin goes away.
class PluginCallGate {
public:
    /// Holds a call in flight; a closed gate turns it away without touching the plugin.
    class Pass {
    public:
        explicit Pass(PluginCallGate& gate) noexcept : gate_(&gate), entered_(gate.enter()) {}

        Pass(const Pass&) = delete;
        Pass& operator=(const Pass&) = delete;

        ~Pass() { gate_->leave(); }

        [[nodiscard]] bool entered() const noexcept { return entered_; }

    private:
        PluginCallGate* gate_;
        bool entered_;
    };

    /// Runs `body` under the interpreter lock only while the gate is open.
    template <typename Body>
    void run(PluginHost& host, Body&& body) {
        host.whileOpen(open_, body);
    }

    /// No call runs the plugin's Lua once this returns; then waits a moment for calls still holding the host.
    void close(PluginHost& host) {
        host.underLock([this] { open_.store(false, std::memory_order_release); });
        const auto until = std::chrono::steady_clock::now() + kDrainLimit;
        while (inFlight_.load(std::memory_order_acquire) > 0 && std::chrono::steady_clock::now() < until) {
            std::this_thread::yield();
        }
    }

private:
    // A call can only be left waiting here when the plugin unloads from inside Lua; it then sees the gate closed.
    static constexpr std::chrono::milliseconds kDrainLimit{1000};

    [[nodiscard]] bool enter() noexcept {
        inFlight_.fetch_add(1, std::memory_order_acq_rel);
        return open_.load(std::memory_order_acquire);
    }

    void leave() noexcept { inFlight_.fetch_sub(1, std::memory_order_acq_rel); }

    std::atomic<bool> open_{true};
    std::atomic<std::int32_t> inFlight_{0};
};

} // namespace basalt::server::plugins
