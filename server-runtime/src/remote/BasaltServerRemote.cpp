#include <basalt/server/remote/ApiToken.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/remote/RemoteHostImpl.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>

#include <mutex>

namespace basalt::server::runtime {
namespace {

constexpr std::string_view kTokenPath = ".basalt/remote-token";

std::mutex gStreamLock;
std::shared_ptr<server::remote::RemoteService> gStream;

void installLogSink() {
    static std::once_flag once;
    std::call_once(once, [] {
        wire::common::logOutput::addSink([](std::string_view line) {
            std::shared_ptr<server::remote::RemoteService> service;
            {
                const std::lock_guard lock{gStreamLock};
                service = gStream;
            }
            if (service) {
                service->publishLog(line);
            }
        });
    });
}

} // namespace

void BasaltServer::startRemote(const std::chrono::steady_clock::time_point startedAt) {
    server::remote::RemoteSettings settings = config_->remote;
    const wire::common::Logger log{"Remote"};
    if (settings.rcon.enabled && settings.rcon.password.empty()) {
        log.error([] {
            return std::string{"RCON stays off: [network.rcon] has enabled = true but an empty password, and an "
                               "open console with no password is never safe. Set a password in config/server.toml."};
        });
        settings.rcon.enabled = false;
    }
    if (settings.rcon.enabled) {
        log.warn([] {
            return std::string{"RCON is enabled and is PLAINTEXT: its password and every command cross the network "
                               "unencrypted. Keep it on loopback or a VPN, and prefer the API."};
        });
    }
    if (settings.api.enabled) {
        std::expected<server::remote::StoredToken, std::string> token =
            server::remote::loadOrCreateToken(std::string{kTokenPath});
        if (!token.has_value()) {
            log.error([&token] { return "The management API stays off: " + token.error(); });
            settings.api.enabled = false;
        } else {
            settings.api.token = token->token;
            if (token->created) {
                log.info(
                    [] { return "Management API token generated and stored in " + std::string{kTokenPath} + "."; });
            }
        }
    }
    if (!settings.api.enabled && !settings.rcon.enabled) {
        return;
    }
    remoteHost_ = remoteHost::create(remoteHost::Sources{.dispatcher = commands(),
                                                         .plugins = plugins_.get(),
                                                         .tick = tickLoop_.get(),
                                                         .players = &playerRegistry_,
                                                         .maxPlayers = config_->maxPlayers,
                                                         .javaPlugins = [this] { return javaPlugins_; },
                                                         .startedAt = startedAt});
    auto service = std::make_shared<server::remote::RemoteService>(settings, *remoteHost_, log);
    if (std::expected<void, std::string> started = service->start(); !started.has_value()) {
        throw std::runtime_error("Remote access could not start: " + started.error() +
                                 ". Change [network.rcon] or [network.api] in config/server.toml.");
    }
    if (settings.rcon.enabled) {
        log.info([&settings] {
            return "RCON listening on " + settings.rcon.bind + ":" + std::to_string(settings.rcon.port);
        });
    }
    if (settings.api.enabled) {
        log.info([&settings] {
            return std::string{"API listening on "} + (settings.api.tls ? "https://" : "http://") + settings.api.bind +
                   ":" + std::to_string(settings.api.port) + "/v1";
        });
        if (!settings.api.tls && settings.api.bind != "127.0.0.1" && settings.api.bind != "::1") {
            log.warn([] {
                return std::string{"The API is reachable beyond loopback without TLS: its bearer token crosses "
                                   "the network in the clear. Set tls = true in [network.api]."};
            });
        }
    }
    installLogSink();
    {
        const std::lock_guard lock{gStreamLock};
        gStream = service;
    }
    remote_ = std::move(service);
}

void BasaltServer::stopRemote() {
    {
        const std::lock_guard lock{gStreamLock};
        gStream.reset();
    }
    if (remote_) {
        remote_->stop();
    }
}

} // namespace basalt::server::runtime
