#include <basalt/server/remote/ApiToken.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/remote/RemoteHostImpl.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>

#include <mutex>
#include <thread>

namespace basalt::server::runtime {
namespace {

constexpr std::string_view kTokenPath = ".basalt/remote-token";
constexpr std::string_view kSecretPath = ".basalt/management-secret";

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
    if (settings.management.enabled) {
        if (settings.management.secret.empty()) {
            std::expected<server::remote::StoredToken, std::string> secret =
                server::remote::loadOrCreateManagementSecret(std::string{kSecretPath});
            if (!secret.has_value()) {
                log.error([&secret] { return "The management server stays off: " + secret.error(); });
                settings.management.enabled = false;
            } else {
                settings.management.secret = secret->token;
                if (secret->created) {
                    log.info(
                        [] { return "Management secret generated and stored in " + std::string{kSecretPath} + "."; });
                }
            }
        }
    }
    if (!settings.api.enabled && !settings.rcon.enabled && !settings.management.enabled) {
        return;
    }
    heartbeatSeconds_ = settings.management.heartbeatSeconds;
    remoteHost::ManagementSources management{.dispatcher = commands(),
                                             .players = &playerRegistry_,
                                             .settings = &settings_,
                                             .notifier = managementNotifier_.get(),
                                             .saveAll = [this] { saveEverything(); },
                                             .stop =
                                                 [this] {
                                                     std::thread{[this] {
                                                         std::this_thread::sleep_for(std::chrono::milliseconds{200});
                                                         shutdown();
                                                     }}.detach();
                                                 },
                                             .ready = [this] { return !shuttingDown_.load(); }};
    if (settings.management.enabled) {
        management_ = remoteHost::createManagement(management);
        remoteHost::observeAccess(management);
    }
    remoteHost_ = remoteHost::create(remoteHost::Sources{.dispatcher = commands(),
                                                         .plugins = plugins_.get(),
                                                         .tick = tickLoop_.get(),
                                                         .players = &playerRegistry_,
                                                         .maxPlayers = config_->maxPlayers,
                                                         .javaPlugins = [this] { return javaPlugins_; },
                                                         .startedAt = startedAt,
                                                         .management = management_.get()});
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
    if (settings.management.enabled) {
        log.info([&settings] {
            return std::string{"Management server listening on "} + (settings.management.tls ? "wss://" : "ws://") +
                   settings.management.bind + ":" + std::to_string(settings.management.port);
        });
        if (!settings.management.tls && settings.management.bind != "127.0.0.1" && settings.management.bind != "::1") {
            log.warn([] {
                return std::string{"The management server is reachable beyond loopback without TLS: its secret "
                                   "crosses the network in the clear. Set tls = true in [network.management]."};
            });
        }
        managementNotifier_->attach([weak = std::weak_ptr<server::remote::RemoteService>{service}](std::string frame) {
            if (const auto alive = weak.lock(); alive != nullptr) {
                alive->publishNotification(std::move(frame));
            }
        });
        managementNotifier_->serverStarted();
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

void BasaltServer::saveEverything() {
    managementNotifier_->saveStarted();
    persistence_->persistEverything();
    managementNotifier_->saveCompleted();
}

void BasaltServer::stopRemote() {
    if (managementNotifier_->listening()) {
        managementNotifier_->serverStopping();
        std::this_thread::sleep_for(std::chrono::milliseconds{150});
        managementNotifier_->detach();
    }
    {
        const std::lock_guard lock{gStreamLock};
        gStream.reset();
    }
    if (remote_) {
        remote_->stop();
    }
}

} // namespace basalt::server::runtime
