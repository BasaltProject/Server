#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace basalt::server::remote {

enum class Origin : std::uint8_t { Rcon, Api };

struct CommandResult {
    bool cancelled = false;
    std::string output;
};

/// What the remote listeners need from the server; every method except runOnServerThread runs on the server thread.
class RemoteHost {
public:
    RemoteHost() = default;
    RemoteHost(const RemoteHost&) = delete;
    RemoteHost& operator=(const RemoteHost&) = delete;
    virtual ~RemoteHost() = default;

    /// Callable from any thread.
    virtual void runOnServerThread(std::function<void()> work) = 0;

    [[nodiscard]] virtual CommandResult executeCommand(Origin origin, std::string_view peer,
                                                       std::string_view command) = 0;

    [[nodiscard]] virtual std::string statusJson() = 0;
    [[nodiscard]] virtual std::string playersJson() = 0;
    [[nodiscard]] virtual std::string pluginsJson() = 0;
};

} // namespace basalt::server::remote
