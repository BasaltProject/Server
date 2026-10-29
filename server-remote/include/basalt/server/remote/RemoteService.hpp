#pragma once

#include <basalt/server/remote/RemoteHost.hpp>
#include <basalt/server/remote/RemoteSettings.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace basalt::server::remote {

/// RCON and the management API on one thread blocked in the event loop, so an idle server costs nothing.
class RemoteService {
public:
    RemoteService(RemoteSettings settings, RemoteHost& host, wire::common::Logger logger);
    ~RemoteService();
    RemoteService(const RemoteService&) = delete;
    RemoteService& operator=(const RemoteService&) = delete;

    [[nodiscard]] std::expected<void, std::string> start();

    void stop();

    /// Callable from any thread; a no-op while nobody listens to the console stream.
    void publishLog(std::string_view line);

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace basalt::server::remote
