#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginSurfaces.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/PacketNames.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <random>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

// parity: `DedicatedServerProperties.getServerPackInfo`.
std::optional<ClientSession::Options::ResourcePack> resourcePackOf(const config::BasaltConfig& config) {
    if (config.resourcePackUrl.empty()) {
        return std::nullopt;
    }
    const wire::common::Logger logger{"ResourcePack"};
    const std::string& hash = config.resourcePackSha1;
    const bool sha1 = hash.size() == 40 && std::ranges::all_of(hash, [](char c) { return std::isxdigit(c) != 0; });
    if (hash.empty()) {
        logger.warn([] {
            return std::string{"You specified a resource pack without providing a sha1 hash. Pack will be updated on "
                               "the client only if you change the name of the pack."};
        });
    } else if (!sha1) {
        logger.warn([] { return std::string{"Invalid sha1 for resource-pack-sha1"}; });
    }
    wire::common::Uuid id = wire::common::Uuid::nameBased(config.resourcePackUrl);
    if (config.resourcePackId.empty()) {
        logger.warn([&id] { return "resource-pack-id missing, using default of " + id.toString(); });
    } else if (const std::optional<wire::common::Uuid> parsed = wire::common::Uuid::parse(config.resourcePackId);
               parsed.has_value()) {
        id = *parsed;
    } else {
        logger.warn([&config] { return "Failed to parse '" + config.resourcePackId + "' into UUID"; });
        return std::nullopt;
    }
    std::optional<wire::text::Component> prompt;
    if (!config.resourcePackPrompt.empty()) {
        const std::expected<wire::common::JsonValue, wire::common::JsonError> json =
            wire::common::parseJson(config.resourcePackPrompt);
        if (json.has_value()) {
            prompt = wire::text::componentJson::deserialize(*json);
        }
    }
    return ClientSession::Options::ResourcePack{.id = id,
                                                .url = config.resourcePackUrl,
                                                .hash = hash,
                                                .required = config.requireResourcePack,
                                                .prompt = prompt};
}

[[nodiscard]] std::mt19937_64& scratchRandom() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    return random;
}

void fillPlayer(server::plugins::PluginEvent& event, const player::PlayerSession& player) {
    event.set(0, player.name());
    event.set(1, player.uuid().toString());
    event.set(2, player.worldName);
}

[[nodiscard]] Announcement announcementOf(const server::plugins::PluginEvent& event) {
    Announcement announcement;
    if (const auto* text = std::get_if<std::string>(&event.get("message")); text != nullptr) {
        announcement.text = *text;
    }
    if (const auto* silent = std::get_if<bool>(&event.get("silent")); silent != nullptr) {
        announcement.silent = *silent;
    }
    return announcement;
}

[[nodiscard]] std::string textOf(const server::plugins::PluginEventValue& value, const std::string& fallback) {
    const std::string* edited = std::get_if<std::string>(&value);
    return edited == nullptr ? fallback : *edited;
}

[[nodiscard]] std::optional<ui::SurfaceContent> surfaceContent(server::plugins::PluginHost& plugins,
                                                               server::plugins::PluginPlayers& players,
                                                               const server::plugins::PluginSurface surface,
                                                               const std::string& playerUuid) {
    const std::shared_ptr<server::plugins::PluginPlayerRef> player = players.byUuid(playerUuid);
    if (player == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::pair<const server::plugins::PluginSurfaceClaim*, server::plugins::PluginSurfaceContent>>
        drawn = plugins.surfaces().content(surface, *player);
    if (!drawn.has_value()) {
        return std::nullopt;
    }
    return ui::SurfaceContent{.title = drawn->second.title, .lines = drawn->second.lines};
}

} // namespace

// parity: `MinecraftServer.kickUnlistedPlayers`.
void BasaltServer::kickUnlistedPlayers() {
    if (!config_->tuning.enforceWhitelist || !whitelistEnforced_ || playerStore_ == nullptr ||
        !playerStore_->isOpen()) {
        return;
    }
    const std::shared_ptr<const config::ConfigSnapshot> snapshot = live_->current();
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        if (!playerStore_->access().isWhitelisted(player->uuid().toString()) && player->connection().disconnect) {
            const std::string* custom = snapshot->messages.find("not-whitelisted");
            player->connection().disconnect(
                custom != nullptr ? wire::text::miniMessage::parse(*custom)
                                  : wire::text::Component::translatable("multiplayer.disconnect.not_whitelisted"));
        }
    }
}

bool BasaltServer::underSpawnProtection(const player::PlayerSession& who, const world::LoadedWorld& world,
                                        const std::int32_t x, const std::int32_t z) {
    const std::int32_t radius = settings_.spawnProtection();
    if (radius <= 0 || world.declaration().dimension != config::WorldDimension::Overworld) {
        return false;
    }
    const config::GroupsConfig& groups = wired_.dispatcher->environment().groups();
    if (groups.resolve(who.group, command::kAdminPermission)) {
        return false;
    }
    // parity: with nobody opped the whole spawn is open, so a fresh server stays buildable.
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - operatorsCheckedAt_.load(std::memory_order_relaxed) >= 2 && playerStore_ != nullptr) {
        operatorsCheckedAt_.store(now, std::memory_order_relaxed);
        anyOperator_.store(!playerStore_->players().findInGroup(groups.operatorGroup().name).empty(),
                           std::memory_order_relaxed);
    }
    if (!anyOperator_.load(std::memory_order_relaxed)) {
        return false;
    }
    const std::array<double, 3>& spawn = world.spawn();
    return std::max(std::abs(x - static_cast<std::int32_t>(std::floor(spawn[0]))),
                    std::abs(z - static_cast<std::int32_t>(std::floor(spawn[2])))) <= radius;
}

void BasaltServer::serveConnection(wire::io::TcpConnection connection) {
    ClientSession::Options options;
    if (!net::setTcpNoDelay(connection.nativeHandle(), config_->tuning.tcpNoDelay)) {
        logger_.debug([] { return std::string{"The system refused TCP_NODELAY on a new connection."}; });
    }
    if (config_->tuning.proxyProtocol) {
        const std::expected<net::proxyProtocol::Parsed, std::string> header =
            net::proxyProtocol::readHeader([&connection](std::span<std::uint8_t> into) -> std::size_t {
                const std::expected<std::size_t, wire::io::SocketError> got = connection.read(into);
                return got.has_value() ? *got : 0;
            });
        if (!header.has_value()) {
            logger_.warn([&connection, &header] {
                return "Dropped " + connection.remoteAddress() + ": " + header.error() + ".";
            });
            connection.close();
            return;
        }
        options.origin = *header;
    }
    const std::string& peer = options.origin.has_value() ? options.origin->address : connection.remoteAddress();
    const std::optional<net::ConnectionGate::Slot> slot =
        connectionGate_.open(peer, config_->tuning.maxConnectionsPerAddress);
    if (!slot.has_value()) {
        connection.close();
        return;
    }
    options.gate = &connectionGate_;
    options.statusProvider = [this](std::int32_t protocolVersion, const std::string& host,
                                    const std::optional<std::string>& address) {
        return serverList_->statusFor(protocolVersion, host, address);
    };
    options.tuning = config_->tuning;
    {
        const std::shared_ptr<const config::ConfigSnapshot> snapshot = live_->current();
        options.limits = std::shared_ptr<const config::LimitsConfig>{snapshot, &snapshot->limits};
        options.messages = std::shared_ptr<const config::MessagesConfig>{snapshot, &snapshot->messages};
    }
    options.idleTimeoutMinutes = [this] { return idleTimeoutMinutes_; };
    options.maxPlayers = [this] { return config_->maxPlayers; };
    options.compressionThreshold = config_->tuning.compressionThreshold;
    options.onlineMode = options_.sessionService;
    options.keyPair = options_.keyPair;
    options.servicesKeys = options_.servicesKeys;
    // parity: `DedicatedServer.enforceSecureProfile`.
    options.enforceSecureProfile = config_->enforceSecureProfile && options_.sessionService != nullptr &&
                                   options_.servicesKeys != nullptr && options_.servicesKeys->canValidateProfileKeys();
    options.bugReportLink = config_->bugReportLink;
    options.serverLinks = [this] { return miscServices_.control->links(); };
    options.resourcePack = resourcePackOf(*config_);
    options.forwarding = config_->forwarding;
    options.playerRegistry = &playerRegistry_;
    options.worlds = namedWorlds();
    options.defaultWorld = &worldRegistry_->defaultWorld();
    options.commands = wired_.dispatcher.get();
    options.defaultGamemode = defaultGamemode_;
    options.difficulty = difficulty_;
    options.playerStore = playerStore_.get();
    options.rememberIp = live_->current()->serverList.rememberIp();
    options.ui = [this] { return live_->current()->ui; };
    options.placeholders = &placeholders_;
    options.lootTables = &vanillaData_.lootTables;
    options.recipes = &vanillaData_.recipes;
    options.villagerTrades = &vanillaData_.villagerTrades;
    options.blockTransformers = &vanillaData_.blockTransformers;
    options.jukeboxes = &vanillaData_.jukeboxSongs;
    options.whitelistEnforced = [this] { return whitelistEnforced_; };
    options.respawnWorld = [this]() -> world::LoadedWorld* {
        const std::optional<std::string> name = settings_.respawnWorldOverride();
        const auto worlds = namedWorlds();
        const auto found = name.has_value() ? worlds.find(*name) : worlds.end();
        return found == worlds.end() ? &worldRegistry_->defaultWorld() : found->second;
    };
    options.spawnProtected = [this](const player::PlayerSession& who, const world::LoadedWorld& world, std::int32_t x,
                                    std::int32_t z) { return underSpawnProtection(who, world, x, z); };
    options.enchantments = &vanillaData_.enchantments;
    options.progress = playerProgress_.get();
    options.advancements = playerAdvancements_.get();
    options.departed = [this](const player::PlayerSession& who) {
        if (miscServices_.bossBars != nullptr) {
            miscServices_.bossBars->left(who.uuid().toString());
        }
        if (miscServices_.scoreboards != nullptr) {
            miscServices_.scoreboards->left(who.uuid().toString());
        }
    };
    options.advancementTriggers = advancementTriggers_.get();
    options.blockTags = &vanillaData_.blockTags;
    options.itemTags = &vanillaData_.itemTags;
    options.enchantmentTags = &vanillaData_.enchantmentTags;
    options.bannerPatterns = &vanillaData_.bannerPatterns;
    options.damageTypeTags = &vanillaData_.damageTypeTags;
    options.fluidTags = &vanillaData_.fluidTags;
    options.timelineTags = &vanillaData_.timelineTags;
    options.entityTypeTags = &vanillaData_.entityTags;
    options.dialogs = &vanillaData_.dialogs;
    options.timelines = &vanillaData_.timelines;
    options.substitutionLabels = substitutionLabels_.get();
    options.bossBars = &bossBars_;
    options.protocolGate = protocolGate_;
    options.nextSeed = [] { return static_cast<std::int64_t>(scratchRandom()()); };
    options.nextFloat = [] { return std::uniform_real_distribution<float>{0.0F, 1.0F}(scratchRandom()); };
    options.nextDouble = [] { return std::uniform_real_distribution<double>{0.0, 1.0}(scratchRandom()); };
    options.nextBool = [] { return std::uniform_int_distribution<int>{0, 1}(scratchRandom()) == 1; };
    options.blocks = broadcaster_.get();
    options.explosions = explosions_.get();
    options.combat = combat_.get();
    options.vehicles = vehicles_.get();
    options.enabledFeatures = [this] { return enabledFeatures(); };
    options.endFight = endFight_.get();
    options.fishing = fishing_.get();
    options.maps = maps_.get();
    options.withers = withers_.get();
    options.fireworks = fireworks_.get();
    options.mobs = mobActions_.get();
    options.structureBlocks = structureBlocks_.get();
    options.awardExperience = [this](world::LoadedWorld& world, double x, double y, double z, std::int32_t amount) {
        orbs_->award(world, x, y, z, amount);
    };
    options.spawnOrb = [this](world::LoadedWorld& world, double x, double y, double z, std::int32_t value) {
        const ExperienceOrbSystem::ReasonScope reason{ExperienceOrbSystem::Reason::VillagerTrade};
        orbs_->spawn(world, x, y, z, value);
    };
    options.plugins = pluginHooks();
    options.events = &eventSink_;
    options.blockTests = blockTests_.get();
    options.carryBedrock = [this](const bedrock::Identity& who, std::unique_ptr<bedrock::Channel> channel) {
        return bedrockListener_ == nullptr ? nullptr : bedrockListener_->carry(who, std::move(channel));
    };

    // On the heap: the tick loop may hold a departed session, and its callbacks reach back into this connection.
    const std::shared_ptr<ClientSession> session =
        std::make_shared<ClientSession>(std::move(connection), std::move(options));
    session->run();
    playerRegistry_.keepAlive(session);
}

ClientSession::PluginHooks BasaltServer::pluginHooks() {
    ClientSession::PluginHooks hooks;
    server::plugins::PluginHost* host = plugins_.get();
    if (host == nullptr) {
        return hooks;
    }
    server::plugins::PluginEventBus* bus = &host->events();

    hooks.loginRefused = [bus](const std::string& name, const std::string& uuid,
                               const std::string& address) -> std::optional<std::string> {
        const server::plugins::PluginEvent* login = server::plugins::emit(
            bus, &server::plugins::pluginEventTypes::playerLogin(), [&](server::plugins::PluginEvent& event) {
                event.set(0, name);
                event.set(1, uuid);
                event.set(2, address);
            });
        if (login == nullptr || !login->cancelled()) {
            return std::nullopt;
        }
        if (login->cancelMessage().has_value()) {
            return *login->cancelMessage();
        }
        const auto* message = std::get_if<std::string>(&login->get("message"));
        return message == nullptr ? std::string{} : *message;
    };

    hooks.packets = &host->packets();

    hooks.listensOn = [host](const std::string& channel) { return host->listeningOnChannel(channel); };
    hooks.channelMessage = [host](const std::string& uuid, const std::string& channel,
                                  std::span<const std::uint8_t> payload) {
        host->channelMessage(channel, uuid, payload);
    };

    server::plugins::PluginPlayers* players = pluginServices_ == nullptr ? nullptr : &pluginServices_->players();
    if (players != nullptr) {
        hooks.surfaces.holdsSidebar = [host] {
            return host->surfaces().anyClaimOn(server::plugins::PluginSurface::Sidebar);
        };
        hooks.surfaces.sidebar = [host, players](const std::string& uuid) {
            return surfaceContent(*host, *players, server::plugins::PluginSurface::Sidebar, uuid);
        };
        hooks.surfaces.holdsBossBar = [host] {
            return host->surfaces().anyClaimOn(server::plugins::PluginSurface::BossBar);
        };
        hooks.surfaces.bossBar = [host, players](const std::string& uuid) {
            return surfaceContent(*host, *players, server::plugins::PluginSurface::BossBar, uuid);
        };
    }

    hooks.npcs = [this] {
        std::vector<NpcView> views;
        for (const std::shared_ptr<plugins::Npc>& npc : npcs_.all()) {
            std::optional<std::vector<std::string>> visibleTo;
            if (npc->visibleTo.has_value()) {
                visibleTo = std::vector<std::string>{npc->visibleTo->begin(), npc->visibleTo->end()};
            }
            views.push_back(NpcView{.entityId = npc->entityId,
                                    .uuid = npc->uuid,
                                    .version = npc->version(),
                                    .world = npc->world,
                                    .x = npc->x,
                                    .y = npc->y,
                                    .z = npc->z,
                                    .yaw = npc->yaw,
                                    .pitch = npc->pitch,
                                    .looksAtPlayers = npc->looksAtPlayers,
                                    .skinTexture = npc->skinTexture,
                                    .skinSignature = npc->skinSignature,
                                    .visibleTo = std::move(visibleTo)});
        }
        return views;
    };
    hooks.npcClicked = [this](std::int32_t entityId, const std::string& playerUuid) {
        return npcs_.clicked(entityId, playerUuid);
    };

    hooks.pickupAllowed = [bus](const player::PlayerSession& player, const std::string& item, std::int32_t count,
                                std::int32_t entityId) {
        const server::plugins::PluginEvent* event = server::plugins::emit(
            bus, &server::plugins::pluginEventTypes::playerItemPickup(), [&](server::plugins::PluginEvent& filled) {
                fillPlayer(filled, player);
                filled.set(3, item);
                filled.set(4, static_cast<std::int64_t>(count));
                filled.set(5, static_cast<std::int64_t>(entityId));
            });
        return event == nullptr || !event->cancelled();
    };
    hooks.moveAllowed = [bus](const player::PlayerSession& player, double fromX, double fromY, double fromZ, double toX,
                              double toY, double toZ) {
        const server::plugins::PluginEvent* event = server::plugins::emit(
            bus, &server::plugins::pluginEventTypes::playerMove(), [&](server::plugins::PluginEvent& filled) {
                fillPlayer(filled, player);
                filled.set(3, fromX);
                filled.set(4, fromY);
                filled.set(5, fromZ);
                filled.set(6, toX);
                filled.set(7, toY);
                filled.set(8, toZ);
            });
        return event == nullptr || !event->cancelled();
    };
    hooks.chatAllowed = [bus](const player::PlayerSession& player, std::string& message) {
        const server::plugins::PluginEvent* event = server::plugins::emit(
            bus, &server::plugins::pluginEventTypes::playerChat(), [&](server::plugins::PluginEvent& filled) {
                fillPlayer(filled, player);
                filled.set(3, message);
            });
        if (event == nullptr) {
            return true;
        }
        message = textOf(event->get(3), message);
        return !event->cancelled();
    };
    hooks.commandAllowed = [bus](const player::PlayerSession& player, std::string& command) {
        const server::plugins::PluginEvent* event = server::plugins::emit(
            bus, &server::plugins::pluginEventTypes::playerCommand(), [&](server::plugins::PluginEvent& filled) {
                fillPlayer(filled, player);
                filled.set(3, command);
            });
        if (event == nullptr) {
            return true;
        }
        command = textOf(event->get(3), command);
        return !event->cancelled();
    };
    hooks.swapHandsAllowed = [bus](const player::PlayerSession& player) {
        const server::plugins::PluginEvent* event =
            server::plugins::emit(bus, &server::plugins::pluginEventTypes::playerSwapHands(),
                                  [&](server::plugins::PluginEvent& filled) { fillPlayer(filled, player); });
        return event == nullptr || !event->cancelled();
    };
    hooks.broadcastAllowed = [bus](std::string& message) {
        const server::plugins::PluginEvent* event =
            server::plugins::emit(bus, &server::plugins::pluginEventTypes::serverBroadcast(),
                                  [&](server::plugins::PluginEvent& filled) { filled.set(0, message); });
        if (event == nullptr) {
            return true;
        }
        message = textOf(event->get(0), message);
        return !event->cancelled();
    };

    hooks.joined = [this, bus](const player::PlayerSession& player) {
        if (miscServices_.scoreboards != nullptr) {
            miscServices_.scoreboards->joined(player);
        }
        const server::plugins::PluginEvent* joined =
            server::plugins::emit(bus, &server::plugins::pluginEventTypes::playerJoin(),
                                  [&player](server::plugins::PluginEvent& filled) { fillPlayer(filled, player); });
        return joined == nullptr ? Announcement{} : announcementOf(*joined);
    };
    hooks.sprintChanged = [bus](const player::PlayerSession& player, bool sprinting) {
        static_cast<void>(server::plugins::emit(bus, &server::plugins::pluginEventTypes::playerSprint(),
                                                [&player, sprinting](server::plugins::PluginEvent& filled) {
                                                    fillPlayer(filled, player);
                                                    filled.set(3, sprinting);
                                                }));
    };
    hooks.sneakChanged = [bus](const player::PlayerSession& player, bool sneaking) {
        static_cast<void>(server::plugins::emit(bus, &server::plugins::pluginEventTypes::playerSneak(),
                                                [&player, sneaking](server::plugins::PluginEvent& filled) {
                                                    fillPlayer(filled, player);
                                                    filled.set(3, sneaking);
                                                }));
    };
    hooks.heldSlotChanged = [bus](const player::PlayerSession& player, std::int32_t slot, std::int32_t fromSlot) {
        static_cast<void>(server::plugins::emit(bus, &server::plugins::pluginEventTypes::playerItemHeld(),
                                                [&player, slot, fromSlot](server::plugins::PluginEvent& filled) {
                                                    fillPlayer(filled, player);
                                                    filled.set(3, static_cast<std::int64_t>(slot));
                                                    filled.set(4, static_cast<std::int64_t>(fromSlot));
                                                }));
    };
    return hooks;
}

} // namespace basalt::server::runtime
