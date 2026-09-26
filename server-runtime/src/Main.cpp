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
#include <basalt/server/runtime/datapack/DataPacks.hpp>
#include <basalt/server/runtime/datapack/RegistryValidation.hpp>
#include <basalt/server/runtime/registry/PackRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/world/VanillaWorldGeneratorFactory.hpp>
#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/auth/SignatureValidator.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/logging/Panic.hpp>
#include <basalt/wire/io/LogFile.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <atomic>
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

/// Where a crash report goes. The server's own, not the project's: the proxy has its own tracker and
/// its own half of the code.
constexpr std::string_view kIssuesUrl = "https://github.com/BasaltProject/Server/issues";

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
    std::unique_ptr<datapack::DataPacks> datapacks;
    std::unique_ptr<world::VanillaWorldGeneratorFactory> generators;
    std::optional<wire::auth::SessionKeyPair> keyPair;
    std::unique_ptr<wire::auth::MojangSessionService> sessions;
    std::unique_ptr<wire::auth::ServicesKeySet> servicesKeys;
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

/// parity: `--safeMode`, which loads the vanilla data pack alone whatever the world saved.
bool gSafeMode = false;

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
                    [](std::string_view url, const server::assets::AssetCache::Progress& onProgress) {
                        const std::expected<wire::auth::HttpsResponse, wire::auth::HttpsError> response =
                            wire::auth::httpsGet(url, wire::auth::kDefaultMaxBytes, onProgress);
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

    // The packs come before anything that reads data: the world generators read the worldgen of the
    // selected packs, as vanilla loads its world registries from them.
    datapack::DataPacks::Options packOptions{
        .jar = VanillaGameData::jarFilesOf(instance->serverJar.get()),
        .worldRoot = std::string{config::WorldsConfig::kDefaultWorldsRoot} + "/" + instance->worlds.defaultWorld().name,
        .initial = datapack::DataPackConfig{.enabled = instance->config.initialEnabledPacks,
                                            .disabled = instance->config.initialDisabledPacks},
        .safeMode = gSafeMode,
    };
    instance->datapacks = std::make_unique<datapack::DataPacks>(packOptions);
    if (const std::vector<datapack::ElementError> errors = datapack::invalidElements(
            *instance->datapacks->resources(), datapack::builtInPackIds(instance->datapacks->repository()), true);
        !errors.empty()) {
        // parity: vanilla refuses to start here. Starting on vanilla alone, without saving that
        // choice, is the one way that neither crashes nor loses the operator's pack list.
        const wire::common::Logger packLogger{"Datapacks"};
        datapack::logElementErrors(errors, packLogger);
        packLogger.error([] {
            return std::string{"Failed to load datapacks, can't proceed with server load. You can either fix your "
                               "datapacks or reset to vanilla with --safeMode. Starting with vanilla alone."};
        });
        packOptions.safeMode = true;
        packOptions.persist = false;
        instance->datapacks = std::make_unique<datapack::DataPacks>(packOptions);
    }
    const VanillaGameData::Jar jarHooks = VanillaGameData::jarOf(instance->datapacks->resources());
    // The synchronised registries are world registries: laid over by the packs once, here.
    {
        std::vector<wire::protocol::configuration::RegistryDataPacket> registries =
            registry::vanillaConfigurationRegistries::packets();
        std::vector<std::string> builtIn;
        for (const datapack::Pack* pack : instance->datapacks->repository().availablePacks()) {
            if (pack->source != datapack::PackSource::World) {
                builtIn.push_back(pack->id);
            }
        }
        registry::applyPackRegistries(registries, *instance->datapacks->resources(), builtIn);
        registry::installRegistries(std::move(registries));
    }
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
    options.datapacks = instance->datapacks.get();
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
        // parity: authlib fetches Mojang's public keys once at start; without them no chat
        // session can be checked and secure chat is not enforced.
        instance->servicesKeys = std::make_unique<wire::auth::ServicesKeySet>(wire::auth::ServicesKeySet::fetch());
        if (!instance->servicesKeys->canValidateProfileKeys()) {
            wire::common::Logger{"Auth"}.warn(
                [] { return std::string{"Could not fetch Mojang's public keys: chat sessions will be ignored."}; });
        }
        options.servicesKeys = instance->servicesKeys.get();
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

    // Before anything that could fail: a crash from here on says what it was and where, instead of
    // leaving the operator with one line from their shell.
    wire::common::panic::install("Basalt Server", std::string{buildInfo::kVersion}, std::string{kIssuesUrl});

    gSafeMode = hasFlag(argc, argv, "--safeMode");

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
    //
    // What it is handed is an atomic and not the instance itself: the console thread reads this
    // while this one is building or dropping a server, and a plain pointer read there is a race
    // that hands the console a half-built dispatcher.
    std::unique_ptr<Instance> running;
    std::atomic<command::CommandDispatcher*> live{nullptr};
    command::consoleReader::start([&live] { return live.load(std::memory_order_acquire); });

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
            live.store(running->server->commands(), std::memory_order_release);
            running->server->start(instanceStartedAt);
        } catch (const std::exception& failure) {
            live.store(nullptr, std::memory_order_release);
            wire::common::Logger{"Server"}.error([&failure] { return failure.what(); });
            exitCode = std::string_view{failure.what()}.starts_with("players.db") ? kDatabaseExitCode : kBindExitCode;
            break;
        }
        live.store(nullptr, std::memory_order_release);
        if (!running->server->shouldRestart()) {
            break;
        }
        running.reset();
    }
    // Told to stop and waited for here, not left to a destructor at exit: standard input held open
    // by a supervisor, a pipe or a fifo never ends, so a reader nobody stops keeps the process alive
    // long after the server is gone. It puts the terminal back on its way out, and this is a second
    // go for the case where it never started.
    command::consoleReader::stop();
    wire::io::terminal::restoreMode();
    return exitCode;
}
