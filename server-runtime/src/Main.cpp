#include <basalt/server/assets/AssetCache.hpp>
#include <basalt/server/assets/EulaFile.hpp>
#include <basalt/server/assets/LangAssets.hpp>
#include <basalt/server/assets/ServerJarAssets.hpp>
#include <basalt/server/assets/VersionMetadata.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/command/ConsoleReader.hpp>
#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/CompatibilityConfig.hpp>
#include <basalt/server/runtime/config/LiveConfigs.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/world/VanillaWorldGeneratorFactory.hpp>
#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/io/LogFile.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt;
using namespace basalt::server::runtime;

namespace {

/// Exit codes an orchestrator can tell apart, the same way the EULA already has one.
///
/// A configuration mistake is the failure an operator actually hits (a typo in a world type, a
/// malformed table), and it deserves one readable line rather than a crash.
constexpr int kConfigExitCode = 2;
constexpr int kBindExitCode = 3;
constexpr int kDatabaseExitCode = 4;

constexpr std::string_view kLogRoot = "./.basalt/logs";

// Raw string, and deliberately not indented: the art is full of backslashes, and every leading
// space in it is significant.
constexpr std::string_view kBanner = R"(
  ____                  _ _      _____
 |  _ \                | | |    / ____|
 | |_) | __ _ ___  __ _| | |_  | (___   ___ _ ____   _____ _ __
 |  _ < / _` / __|/ _` | | __|  \___ \ / _ \ '__\ \ / / _ \ '__|
 | |_) | (_| \__ \ (_| | | |_   ____) |  __/ |   \ V /  __/ |
 |____/ \__,_|___/\__,_|_|\__| |_____/ \___|_|    \_/ \___|_|
)";

[[nodiscard]] std::string versionLine() {
    return "Basalt Server " + std::string{buildInfo::kVersion} + " for Minecraft " +
           std::string{buildInfo::kMinecraftVersion} + " (protocol " + std::to_string(buildInfo::kProtocolVersion) +
           ")";
}

[[nodiscard]] bool hasFlag(int argc, char** argv, std::string_view flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == flag) {
            return true;
        }
    }
    return false;
}

/// Everything one instance of the server owns that outlives its constructor. A restart drops the
/// whole thing and builds another from the files as they are then.
struct Instance {
    config::BasaltConfig config;
    config::WorldsConfig worlds;
    std::unique_ptr<config::LiveConfigs> live;
    std::unique_ptr<server::assets::ServerJarAssets> serverJar;
    std::unique_ptr<world::VanillaWorldGeneratorFactory> generators;
    std::optional<wire::auth::SessionKeyPair> keyPair;
    std::unique_ptr<wire::auth::MojangSessionService> sessions;
    std::unique_ptr<BasaltServer> server;
};

/// Fails the whole start rather than carrying on with a file nobody wrote.
template <typename T>
[[nodiscard]] T orExit(std::expected<T, std::string> loaded, const wire::common::Logger& logger) {
    if (!loaded.has_value()) {
        logger.error([&loaded] { return loaded.error(); });
        std::exit(kConfigExitCode);
    }
    return std::move(*loaded);
}

/// Builds one server from the configuration as it stands right now. Called again on every restart.
[[nodiscard]] std::unique_ptr<Instance> buildServer() {
    const wire::common::Logger configLogger{"Config"};
    // Read again on every start, so a restart really does pick up an edited server.toml: the port,
    // the game mode and the log level are all decided here.
    auto instance = std::make_unique<Instance>(Instance{
        .config = orExit(config::BasaltConfig::load(config::ConfigDirectory{}, "server.properties", configLogger),
                         configLogger),
        .worlds =
            orExit(config::WorldsConfig::load(config::ConfigDirectory{},
                                              std::string{config::WorldsConfig::kDefaultWorldsRoot}, configLogger),
                   configLogger),
    });
    wire::common::Logger::setDefaultMinLevel(instance->config.logLevel);

    // Downloading is the fallback, not the path: once a version's data is unpacked the source below
    // is never asked, so a warm install resolves no metadata and contacts no network.
    const wire::common::Logger assetsLogger{"Assets"};
    const std::string dataRoot = "./.basalt/assets/" + std::string{buildInfo::kMinecraftVersion};
    std::expected<server::assets::ServerJarAssets, server::assets::AssetError> jar =
        server::assets::ServerJarAssets::open(
            buildInfo::kMinecraftVersion, dataRoot,
            [&assetsLogger]() -> std::expected<std::string, server::assets::AssetError> {
                const std::expected<server::assets::VersionMetadata, server::assets::AssetError> metadata =
                    server::assets::versionManifest::resolveCached(
                        buildInfo::kMinecraftVersion,
                        [](std::string_view url) -> std::optional<std::string> {
                            const std::expected<wire::auth::HttpsResponse, wire::auth::HttpsError> response =
                                wire::auth::httpsGet(url);
                            return response.has_value() && response->ok() ? std::optional{response->text()}
                                                                          : std::nullopt;
                        },
                        std::optional<std::string>{"./.basalt/assets"});
                if (!metadata.has_value()) {
                    return std::unexpected(metadata.error());
                }
                return server::assets::AssetCache{
                    [](std::string_view url) {
                        const std::expected<wire::auth::HttpsResponse, wire::auth::HttpsError> response =
                            wire::auth::httpsGet(url);
                        return response.has_value() && response->ok() ? std::optional{response->body} : std::nullopt;
                    },
                    "./.basalt/assets", assetsLogger}
                    .ensure(*metadata, server::assets::AssetCache::Artifact::Server);
            },
            assetsLogger);
    if (jar.has_value()) {
        instance->serverJar = std::make_unique<server::assets::ServerJarAssets>(std::move(*jar));
        assetsLogger.info(
            [&dataRoot] { return std::string{buildInfo::kMinecraftVersion} + " data ready at " + dataRoot + "."; });
    } else {
        assetsLogger.warn([] {
            return std::string{"No server data: the worlds this build can generate are limited to what needs no jar."};
        });
    }

    const VanillaGameData::Jar jarHooks = VanillaGameData::jarOf(instance->serverJar.get());
    instance->generators =
        std::make_unique<world::VanillaWorldGeneratorFactory>(world::VanillaWorldGeneratorFactory::Jar{
            .readText = jarHooks.readText, .readEntry = jarHooks.readEntry, .dataIds = jarHooks.dataIds});

    // The MOTD is the one thing the server list takes from a vanilla server.properties, and only
    // while generating the file for the first time.
    static_cast<void>(orExit(
        config::ServerListConfig::load(config::ConfigDirectory{}, config::BasaltConfig::legacyMotd(), configLogger),
        configLogger));
    instance->live = std::make_unique<config::LiveConfigs>(
        orExit(config::LiveConfigs::read(config::ConfigDirectory{}, configLogger), configLogger));

    BasaltServer::Options options;
    options.serverJar = instance->serverJar.get();
    options.generators = instance->generators.get();
    options.compatibility =
        orExit(config::CompatibilityConfig::load(config::ConfigDirectory{}, configLogger), configLogger);
    // A forwarding proxy has already authenticated everyone, so the session exchange never runs
    // here: keeping it would cost every player a second login they cannot answer.
    if (instance->config.onlineMode && !instance->config.forwarding.enabled()) {
        instance->keyPair = wire::auth::SessionKeyPair::generate();
        instance->sessions = std::make_unique<wire::auth::MojangSessionService>();
        options.keyPair = instance->keyPair.has_value() ? &*instance->keyPair : nullptr;
        options.sessionService = instance->sessions.get();
    }
    // Console-only: clients translate for themselves, a log file cannot. Unpacked from the same
    // server.jar as everything else, so this costs no extra download.
    options.lang = server::assets::langAssets::loadCachedOrEmpty(dataRoot);

    instance->server =
        std::make_unique<BasaltServer>(instance->config, instance->worlds, *instance->live, std::move(options));
    return instance;
}

} // namespace

int main(int argc, char** argv) {
    const std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();
    std::puts(std::string{kBanner}.c_str());
    std::puts(versionLine().c_str());

    // No prompts and no TTY detection: identical behaviour interactive, headless or containerized.
    // `--accept-eula` is a deliberate operator act, not a fallback.
    if (server::assets::eulaFile::check(hasFlag(argc, argv, "--accept-eula"), wire::common::Logger{"Eula"}) !=
        server::assets::eulaFile::Outcome::Accepted) {
        return server::assets::eulaFile::kRejectedExitCode;
    }

    // Escape codes are a terminal affordance; piped or redirected output gets none. Applied before
    // the config is read so that loading it is itself coloured, then again once the operator's own
    // choice is known.
    const bool attachedToTerminal = wire::io::isConsoleInteractive();
    wire::common::logOutput::setUseColor(attachedToTerminal);

    // Read once for the things that cannot be redone per instance: the log file would archive itself
    // on every restart, and colour is a property of the terminal, not of the server.
    const wire::common::Logger configLogger{"Config"};
    const std::expected<config::BasaltConfig, std::string> boot =
        config::BasaltConfig::load(config::ConfigDirectory{}, "server.properties", configLogger, false);
    if (!boot.has_value()) {
        configLogger.error([&boot] { return boot.error(); });
        return kConfigExitCode;
    }
    wire::common::Logger::setDefaultMinLevel(boot->logLevel);
    switch (boot->logColor) {
    case config::ColorMode::Always:
        wire::common::logOutput::setUseColor(true);
        break;
    case config::ColorMode::Never:
        wire::common::logOutput::setUseColor(false);
        break;
    case config::ColorMode::Auto:
        wire::common::logOutput::setUseColor(attachedToTerminal);
        break;
    }
    if (boot->logToFile) {
        static_cast<void>(wire::io::logFile::install(std::string{kLogRoot}, boot->logKeepDays, versionLine()));
    }

    // Installed once for the process, not once per server: a second handler would leave the first
    // one pointing at an instance that has already stopped.
    wire::io::shutdownSignal::install();

    // The console outlives the server it talks to, because `/restart` replaces that server while
    // this thread is parked inside a blocking read of standard input.
    std::unique_ptr<Instance> running;
    command::consoleReader::start([&running]() -> command::CommandDispatcher* {
        return running == nullptr ? nullptr : running->server->commands();
    });

    // One instance per turn around this loop. A restart is not a re-exec and not a new process: the
    // old instance winds itself down exactly as `/stop` does, and then a new one is built from the
    // files as they are now. That is what makes server.toml and worlds.toml applicable, which
    // `/reload` deliberately cannot do.
    int exitCode = 0;
    bool first = true;
    while (true) {
        const std::chrono::steady_clock::time_point instanceStartedAt =
            first ? startedAt : std::chrono::steady_clock::now();
        first = false;
        try {
            running = buildServer();
            running->server->start(instanceStartedAt);
        } catch (const std::exception& failure) {
            wire::common::Logger{"Server"}.error([&failure] { return failure.what(); });
            exitCode = std::string_view{failure.what()}.starts_with("players.db") ? kDatabaseExitCode : kBindExitCode;
            break;
        }
        if (!running->server->shouldRestart()) {
            break;
        }
        running.reset();
    }
    // The console thread is parked in a blocking read and never gets to run its own cleanup, so the
    // terminal is put back from here. Skipping it leaves the operator's shell with no echo after the
    // server stops, which looks like their terminal broke.
    wire::io::terminal::restoreMode();
    return exitCode;
}
