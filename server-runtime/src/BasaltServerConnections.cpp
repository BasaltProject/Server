#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginSurfaces.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/plugins/PacketNames.hpp>

#include <cstdint>
#include <random>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

/// One source for the numbers nobody checks bit for bit: a sound's seed, a scatter.
[[nodiscard]] std::mt19937_64& scratchRandom() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    return random;
}

/// The three fields every player event starts with, filled once here rather than at each call site.
void fillPlayer(server::plugins::PluginEvent& event, const player::PlayerSession& player) {
    event.set(0, player.name());
    event.set(1, player.uuid().toString());
    event.set(2, player.worldName);
}

[[nodiscard]] std::string textOf(const server::plugins::PluginEventValue& value, const std::string& fallback) {
    const std::string* edited = std::get_if<std::string>(&value);
    return edited == nullptr ? fallback : *edited;
}

/// What a plugin drawing a surface put there for this player, or nothing when nobody claims it.
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

void BasaltServer::serveConnection(wire::io::TcpConnection connection) {
    ClientSession::Options options;
    options.statusProvider = [this](std::int32_t protocolVersion, const std::string& host,
                                    const std::optional<std::string>& address) {
        return serverList_->statusFor(protocolVersion, host, address);
    };
    options.onlineMode = options_.sessionService;
    options.keyPair = options_.keyPair;
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
    options.jukeboxes = &vanillaData_.jukeboxSongs;
    options.whitelistEnforced = [this] { return whitelistEnforced_; };
    options.enchantments = &vanillaData_.enchantments;
    options.progress = playerProgress_.get();
    options.advancementTriggers = advancementTriggers_.get();
    options.blockTags = &vanillaData_.blockTags;
    options.itemTags = &vanillaData_.itemTags;
    options.enchantmentTags = &vanillaData_.enchantmentTags;
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
    options.plugins = pluginHooks();

    // On the heap and handed to the registry afterwards: a session that has left is kept for a
    // window because the tick loop may still hold a pointer to it, and its callbacks reach back
    // into this connection, so the connection has to outlive it by the same window.
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
        return login->cancelMessage().value_or("");
    };

    // Nothing is copied and nothing is decoded while nobody is watching that packet's own name: the
    // first line of each of these is a map lookup on the path every packet takes.
    server::plugins::PluginPacketBus* packets = &host->packets();
    const auto intercept = [packets](wire::protocol::ProtocolState state, bool serverbound, std::int32_t id,
                                     std::span<const std::uint8_t> bytes,
                                     const player::PlayerSession* player) -> std::optional<std::vector<std::uint8_t>> {
        const std::string name = plugins::packetNames::of(state, serverbound, id);
        if (!packets->listening(name)) {
            return std::vector<std::uint8_t>{bytes.begin(), bytes.end()};
        }
        server::plugins::PluginPacket& packet = packets->packet();
        packet.name = name;
        packet.serverbound = serverbound;
        packet.playerName = player == nullptr ? "" : player->name();
        packet.playerUuid = player == nullptr ? "" : player->uuid().toString();
        packet.bytes.assign(bytes.begin(), bytes.end());
        if (!packets->fire(packet)) {
            return std::nullopt;
        }
        return packet.replacement.value_or(packet.bytes);
    };
    hooks.incoming = [intercept](wire::protocol::ProtocolState state, std::int32_t id,
                                 std::span<const std::uint8_t> bytes) {
        return intercept(state, true, id, bytes, nullptr);
    };
    hooks.outgoing = [intercept](wire::protocol::ProtocolState state, std::int32_t id,
                                 std::span<const std::uint8_t> bytes) {
        return intercept(state, false, id, bytes, nullptr);
    };

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

    hooks.joined = [bus](const player::PlayerSession& player) {
        static_cast<void>(
            server::plugins::emit(bus, &server::plugins::pluginEventTypes::playerJoin(),
                                  [&player](server::plugins::PluginEvent& filled) { fillPlayer(filled, player); }));
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
    hooks.menuClosed = [bus](const player::PlayerSession& player, std::string_view kind) {
        static_cast<void>(server::plugins::emit(bus, &server::plugins::pluginEventTypes::inventoryClose(),
                                                [&player, kind](server::plugins::PluginEvent& filled) {
                                                    fillPlayer(filled, player);
                                                    filled.set(3, std::string{kind});
                                                }));
    };
    return hooks;
}

} // namespace basalt::server::runtime
