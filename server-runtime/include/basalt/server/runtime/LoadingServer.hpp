#pragma once

#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/MessagesConfig.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/io/TcpSocket.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <atomic>
#include <memory>
#include <optional>
#include <thread>

namespace basalt::server::runtime {

/// Holds the Java port while the real server is still starting: it answers the server list and turns logins away.
class LoadingServer {
public:
    struct Look {
        wire::protocol::status::ServerStatusPayload status;
        wire::text::Component kick;
    };

    [[nodiscard]] static std::unique_ptr<LoadingServer> start(const config::BasaltConfig& config,
                                                              const config::ServerListConfig& serverList,
                                                              const config::MessagesConfig& messages,
                                                              const wire::common::Logger& logger);

    [[nodiscard]] static Look lookOf(const config::BasaltConfig& config, const config::ServerListConfig& serverList,
                                     const config::MessagesConfig& messages);

    LoadingServer(wire::io::TcpListener listener, std::shared_ptr<const Look> look);

    LoadingServer(const LoadingServer&) = delete;
    LoadingServer& operator=(const LoadingServer&) = delete;
    ~LoadingServer();

    [[nodiscard]] bool holdsListener() const noexcept { return held_.load(); }

    /// Stops answering and gives the bound port to whoever serves the real thing; empty if already released.
    [[nodiscard]] std::optional<wire::io::TcpListener> release();

private:
    void acceptLoop();

    std::optional<wire::io::TcpListener> listener_;
    std::shared_ptr<const Look> look_;
    std::shared_ptr<std::atomic<int>> open_;
    std::atomic<bool> held_{true};
    std::atomic<bool> stopping_{false};
    std::thread thread_;
};

} // namespace basalt::server::runtime
