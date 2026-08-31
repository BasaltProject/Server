#include <basalt/server/runtime/Connection.hpp>
#include <basalt/server/runtime/LoginHandler.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/Platform.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <algorithm>
#include <charconv>
#include <string>
#include <thread>
#include <vector>

using namespace basalt;
using namespace basalt::server::runtime;

namespace {

constexpr std::uint16_t kDefaultPort = 25565;

std::uint16_t portFromArguments(int argc, char** argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string_view{argv[i]} == "--port") {
            std::uint16_t port = 0;
            const std::string_view text{argv[i + 1]};
            if (std::from_chars(text.data(), text.data() + text.size(), port).ec == std::errc{}) {
                return port;
            }
        }
    }
    return kDefaultPort;
}

} // namespace

int main(int argc, char** argv) {
    wire::common::logOutput::setUseColor(wire::io::isConsoleInteractive());
    const wire::common::Logger logger{"Basalt"};

    wire::io::shutdownSignal::install();

    const std::uint16_t port = portFromArguments(argc, argv);
    auto listener = wire::io::TcpListener::bind(port);
    if (!listener) {
        logger.error([&] { return "Could not bind port " + std::to_string(port); });
        return 1;
    }

    const ServerSettings settings;
    logger.info([&] { return "Listening on port " + std::to_string(port); });

    wire::io::EventLoop loop;
    loop.watch(listener->nativeHandle());

    // One thread per connection, which is what the login handshake needs and no more. The tick loop
    // and its scheduler land with the runtime proper.
    std::vector<std::jthread> connections;
    while (!wire::io::shutdownSignal::isRequested()) {
        const auto ready = loop.waitReadable(250);
        if (!ready) {
            break;
        }
        if (ready->empty()) {
            continue;
        }

        auto accepted = listener->accept();
        if (!accepted) {
            continue;
        }
        connections.emplace_back([socket = std::move(*accepted), &settings]() mutable {
            Connection connection{std::move(socket)};
            LoginHandler handler{settings};
            handler.run(connection);
            connection.close();
        });

        // Threads that have finished are joined by jthread's own destructor when the vector is
        // trimmed; nothing here keeps a finished connection alive beyond its own return.
        std::erase_if(connections, [](const std::jthread& thread) { return !thread.joinable(); });
    }

    logger.info([] { return "Shutting down"; });
    return 0;
}
