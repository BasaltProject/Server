#pragma once

#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/config/UiConfigs.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/net/ProxyLogin.hpp>
#include <basalt/server/runtime/net/SessionOutbox.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/ui/PlayerUi.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantmentTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/RealFluidTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealTimelineTags.hpp>
#include <basalt/wire/auth/Cfb8Cipher.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/auth/SessionProfile.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/io/TcpSocket.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/ProtocolGate.hpp>
#include <basalt/wire/multiversion/SubstitutionLabels.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/protocol/ProtocolState.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace basalt::server::runtime::command {
class CommandDispatcher;
}

namespace basalt::server::runtime::sched {
class Owner;
}

namespace basalt::server::runtime::command::commands {
class BossBarRegistry;
}

namespace basalt::server::runtime::world::jukeboxes {
class SongOutputs;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
class WorldState;
class ChunkGenerationQueue;
class RealLootTables;
class RealRecipes;
class RealVillagerTrades;
class RealEnchantments;
class PlayerProgress;
class AdvancementTriggers;
class WorldWeather;
class RealDialogs;
class RealTimelines;
} // namespace basalt::server::runtime::world

namespace basalt::server::storage {
class PlayerStore;
}

namespace basalt::server::runtime {

class ChunkStreamer;
class CombatHandler;
class BlockChangeBroadcaster;
class ContainerMenuController;
class ExplosionSystem;
class BlockInteractionHandler;
class ItemUseTimers;
class PlayerLifecycle;
class UiStateSync;
class VillagerTradeController;

/// Drives one client connection through Handshake, Status or Login, Configuration and Play. Login is
/// offline-mode when `onlineMode` is unset; otherwise everything from the compression packet on is
/// enciphered with an AES-128 CFB8 secret.
///
/// This connection's thread also runs the player tick, item pickup, commands, death and respawn.
class ClientSession {
public:
    /// parity: vanilla's own fifteen seconds between keep-alives.
    static constexpr std::int64_t kKeepAliveIntervalSeconds = 15;

    /// The first container id a block's window may use: the player's own inventory is zero.
    static constexpr std::int32_t kFirstBlockContainerId = 1;
    static constexpr std::int32_t kMaxContainerId = 100;

    /// What the client is told to send before it has said what it can keep up with.
    static constexpr float kInitialChunksPerTick = 4.0F;

    /// What a plugin gets a say in, and what it is told. Callbacks rather than the plugin bus
    /// itself: this has no business knowing what a plugin is, and unset hooks mean nobody objects.
    struct PluginHooks {
        /// Whether this login may go ahead, and the reason to refuse it with.
        std::function<std::optional<std::string>(const std::string& name, const std::string& uuid,
                                                 const std::string& address)>
            loginRefused;
        /// A packet on its way out, which a plugin may edit or drop. Answers nothing to drop it.
        ///
        /// The state comes with it because a packet id only names a packet within one: 0x01 is a
        /// different thing in configuration than it is in play.
        std::function<std::optional<std::vector<std::uint8_t>>(wire::protocol::ProtocolState state, std::int32_t id,
                                                               std::span<const std::uint8_t> packet)>
            outgoing;
        /// The same on the way in.
        std::function<std::optional<std::vector<std::uint8_t>>(wire::protocol::ProtocolState state, std::int32_t id,
                                                               std::span<const std::uint8_t> packet)>
            incoming;
        /// Whether anybody is listening on a plugin channel, and what to do with what arrives.
        std::function<bool(const std::string& channel)> listensOn;
        std::function<void(const std::string& uuid, const std::string& channel, std::span<const std::uint8_t> payload)>
            channelMessage;

        /// The surfaces a plugin may take over for this player.
        /// Which surfaces a plugin is drawing, asked per player: what one sees on the sidebar is
        /// not what the next one does.
        struct Surfaces {
            std::function<bool()> holdsSidebar;
            std::function<std::optional<ui::SurfaceContent>(const std::string& playerUuid)> sidebar;
            std::function<bool()> holdsBossBar;
            std::function<std::optional<ui::SurfaceContent>(const std::string& playerUuid)> bossBar;
        };

        Surfaces surfaces;
        /// The NPCs a plugin is drawing in this world, and what a click on one does.
        std::function<std::vector<NpcView>()> npcs;
        std::function<bool(std::int32_t entityId, const std::string& playerUuid)> npcClicked;
        /// Whether this player may pick that stack up off the ground.
        std::function<bool(const player::PlayerSession& player, const std::string& item, std::int32_t count,
                           std::int32_t entityId)>
            pickupAllowed;
        /// Whether the step may stand. Refusing puts the player back where they were.
        std::function<bool(const player::PlayerSession& player, double fromX, double fromY, double fromZ, double toX,
                           double toY, double toZ)>
            moveAllowed;
        /// Whether the line may be said, and what it really says: `message` is the plugin's to edit.
        std::function<bool(const player::PlayerSession& player, std::string& message)> chatAllowed;
        /// The same for a slash command, without its slash.
        std::function<bool(const player::PlayerSession& player, std::string& command)> commandAllowed;
        std::function<bool(const player::PlayerSession& player)> swapHandsAllowed;
        /// Whether the server-wide message goes out, and what it says.
        std::function<bool(std::string& message)> broadcastAllowed;

        /// The things a plugin is only told about, never asked. Each is what it says and no more:
        /// nothing here can refuse, so an unset one is a boolean on the packet path.
        std::function<void(const player::PlayerSession& player)> joined;
        std::function<void(const player::PlayerSession& player, bool sprinting)> sprintChanged;
        std::function<void(const player::PlayerSession& player, bool sneaking)> sneakChanged;
        std::function<void(const player::PlayerSession& player, std::int32_t slot, std::int32_t fromSlot)>
            heldSlotChanged;
        /// `kind` is "player" for the inventory itself and "container" for anything else.
        std::function<void(const player::PlayerSession& player, std::string_view kind)> menuClosed;
    };

    /// Everything the session is handed at construction. A struct rather than forty parameters:
    /// the server fills in what it has and the defaults are what a bare session runs with.
    struct Options {
        std::function<wire::protocol::status::ServerStatusPayload(std::int32_t protocolVersion, const std::string& host,
                                                                  const std::optional<std::string>& address)>
            statusProvider;
        std::int32_t compressionThreshold = 256;
        /// Unset is offline mode, which is what a server with no session service runs.
        wire::auth::SessionService* onlineMode = nullptr;
        wire::auth::forwarding::ProxyForwarding forwarding{};
        player::PlayerRegistry* playerRegistry = nullptr;
        std::map<std::string, world::LoadedWorld*, std::less<>> worlds;
        world::LoadedWorld* defaultWorld = nullptr;
        command::CommandDispatcher* commands = nullptr;
        wire::protocol::play::Gamemode defaultGamemode = wire::protocol::play::Gamemode::Survival;
        std::string difficulty = "easy";
        storage::PlayerStore* playerStore = nullptr;
        bool rememberIp = false;
        std::function<config::UiConfigs()> ui;
        const wire::common::placeholder::PlaceholderRegistry* placeholders = nullptr;
        world::RealLootTables* lootTables = nullptr;
        world::RealRecipes* recipes = nullptr;
        world::RealVillagerTrades* villagerTrades = nullptr;
        world::jukeboxes::SongOutputs* jukeboxes = nullptr;
        std::function<bool()> whitelistEnforced;
        world::RealEnchantments* enchantments = nullptr;
        world::PlayerProgress* progress = nullptr;
        world::AdvancementTriggers* advancementTriggers = nullptr;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        const world::realItemTags::TagMembers* itemTags = nullptr;
        /// Which enchantments will not sit beside each other, which only an anvil asks about.
        const world::realEnchantmentTags::TagMembers* enchantmentTags = nullptr;
        const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
        const world::realFluidTags::TagMembers* fluidTags = nullptr;
        const world::realTimelineTags::TagMembers* timelineTags = nullptr;
        const world::realEntityTypeTags::TagMembers* entityTypeTags = nullptr;
        /// The two registries a client is sent that this build loads from the jar.
        world::RealDialogs* dialogs = nullptr;
        world::RealTimelines* timelines = nullptr;
        /// What an older client's translation renames things with, when one is loaded.
        const wire::multiversion::SubstitutionLabels* substitutionLabels = nullptr;
        /// The server's own RSA pair, which only online mode uses.
        const wire::auth::SessionKeyPair* keyPair = nullptr;
        /// The bars `/bossbar` keeps, which a joining player is shown.
        const command::commands::BossBarRegistry* bossBars = nullptr;
        /// Which client versions may in. Defaults to this build's own and nothing else.
        wire::multiversion::ProtocolGate protocolGate{{wire::multiversion::protocolVersions::kNative},
                                                      wire::multiversion::UnknownProtocolPolicy::Warn,
                                                      wire::multiversion::UnknownProtocolPolicy::Refuse};
        /// The rolls this session makes that belong to no world stream: a sound's own seed and the
        /// duration a consumed effect lands with.
        std::function<std::int64_t()> nextSeed;
        std::function<float()> nextFloat;
        std::function<double()> nextDouble;
        std::function<bool()> nextBool;
        /// The block and blast machinery the server owns, shared by every session in it.
        BlockChangeBroadcaster* blocks = nullptr;
        ExplosionSystem* explosions = nullptr;
        PluginHooks plugins;
    };

    ClientSession(wire::io::TcpConnection connection, Options options);

    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    ClientSession(ClientSession&&) = delete;
    ClientSession& operator=(ClientSession&&) = delete;
    ~ClientSession();

    /// Reads this connection until the peer goes away. The whole session lives on this thread.
    void run();

    [[nodiscard]] const wire::common::Logger& logger() const noexcept { return logger_; }

    [[nodiscard]] const Options& options() const noexcept { return options_; }

    [[nodiscard]] wire::protocol::ProtocolState state() const noexcept { return state_; }

    /// The player this connection is carrying, or nothing before they have logged in.
    [[nodiscard]] player::PlayerSession* playerSession() const noexcept { return playerSession_.get(); }

    /// Hands this player over to whoever will let go of them. Called on the way out: the thread
    /// that ends a connection is not the thread that walks the list of players, so the object
    /// cannot simply die here.
    [[nodiscard]] std::unique_ptr<player::PlayerSession> takePlayerSession() noexcept {
        return std::move(playerSession_);
    }

    [[nodiscard]] world::LoadedWorld* currentWorld() const noexcept { return currentWorld_; }

    /// The world's blocks. Only valid once the player is in one.
    [[nodiscard]] world::WorldState& world() const;

    [[nodiscard]] world::ChunkGenerationQueue& generationQueue() const;

    [[nodiscard]] std::string worldName() const;

    [[nodiscard]] std::optional<std::string> remoteAddress() const;

    /// The address to remember them by, or nothing when the operator asked not to keep one.
    [[nodiscard]] std::optional<std::string> rememberAddress() const;

    [[nodiscard]] ui::PlayerUi& playerUi() noexcept { return playerUi_; }

    /// Writes one play packet, dropping it when the connection is already gone.
    void sendPlayPacket(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody);

    /// The same, but a socket error propagates: login and configuration want to know.
    void sendCompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody);

    void disconnect(const wire::text::Component& reason);

    /// The satellites, each of which holds this session and owns one part of what it does.
    [[nodiscard]] ContainerMenuController& menus() noexcept { return *menus_; }

    [[nodiscard]] BlockInteractionHandler& blockInteraction() noexcept { return *blockInteraction_; }

    [[nodiscard]] ChunkStreamer& chunkStreamer() noexcept { return *chunkStreamer_; }

    [[nodiscard]] CombatHandler& combat() noexcept { return *combat_; }

    [[nodiscard]] EntityVisibilityTracker& entityTracker() noexcept { return *entityTracker_; }

    [[nodiscard]] ItemUseTimers& itemUseTimers() noexcept { return *itemUseTimers_; }

    [[nodiscard]] PlayerLifecycle& lifecycle() noexcept { return *lifecycle_; }

    [[nodiscard]] UiStateSync& uiState() noexcept { return *uiState_; }

    [[nodiscard]] VillagerTradeController& trades() noexcept { return *trades_; }

    /// Everyone in the same world as this player, which is who a block change is told to.
    [[nodiscard]] std::vector<player::PlayerSession*> playersInSameWorld() const;

    /// What a respawn and a world change both need: which world, at what difficulty, in what mode.
    [[nodiscard]] wire::protocol::play::SpawnInfo spawnInfo(wire::protocol::play::Gamemode gamemode) const;

    /// Moves the player and everything that follows from where they now are: chunks, blocks under
    /// them, the statistics and whoever can see them.
    void updatePosition(double x, double y, double z, float yaw, float pitch, bool onGround);

    /// parity: `ServerPlayer.tick`'s own pickup pass, which is what makes an item fly to a player.
    void tryPickupNearbyItems();

    /// Damage that goes through the ordinary path: armour, absorption, the death message and the
    /// hurt animation.
    void hurt(float amount, std::string_view deathMessageId, const wire::common::Identifier& damageType,
              const std::optional<entity::DamageSource>& source = std::nullopt);

    void heal(float amount);

    void killPlayer(std::string_view deathMessageId);

    /// Gets this player out of the bed they are in.
    void wakeUp();

    /// A line only this player sees, which is how a plugin explains why it said no.
    void sendPluginNotice(const std::string& message);

    /// The line above the hotbar, which is where a bed and a spawn point answer.
    void sendActionBar(const wire::text::Component& message);

    /// parity: `ServerGamePacketListenerImpl`'s `SWAP_ITEM_WITH_OFFHAND`: the F key.
    void swapWithOffhand();

    /// A chat line everybody on the server sees.
    void broadcastToEveryone(const wire::text::Component& message);

    /// Whether this player can reach that entity to hit it. parity: `Player.canReach`.
    [[nodiscard]] bool withinAttackReach(const player::PlayerSession& session,
                                         const entity::ServerEntity& target) const;

    /// The console's own language table, which is what a death message is written out with.
    [[nodiscard]] const wire::text::LangTable& consoleLang() const;

    /// One chunk again, for a change too large to express as block updates. Delegates.
    void resendChunk(std::int32_t chunkX, std::int32_t chunkZ);

    void sendBiomes(const std::vector<std::pair<std::int32_t, std::int32_t>>& chunks);

    /// parity: `Entity.stopRiding`: off the cart, standing where it left them. Delegates.
    void dismount(player::PlayerSession& session);

    /// What a plugin writes to this player, in chat or on the action bar.
    void sendPluginText(const wire::text::Component& text, bool actionBar);

    /// parity: `LivingEntity.addEffect`, for what a mob puts on somebody rather than a drink.
    void addEffect(const wire::common::Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier);

    void syncSubmersion();

    void syncHeldAttributes();

    void syncExperience();

    void sendStats();

    void sendTime(world::LoadedWorld& world);

    void sendWeather(const world::WorldWeather& weather);

    void sendCommandTree();

    /// A sound everybody near it hears, which is how a bow, a pearl and a step all travel.
    void broadcastSound(std::string_view sound, double x, double y, double z, wire::protocol::play::SoundSource source,
                        float volume = 1.0F, float pitch = 1.0F);

    /// The inventory delegates, which the satellites and the plugin bridge both reach for.
    void sendInventory();
    void sendInventoryAll();
    void sendInventorySlot(std::int32_t slot);
    void syncHealth();

    /// One stack on the ground where it was dropped.
    void spawnDroppedItem(const wire::common::Identifier& item, std::int32_t count, double x, double y, double z,
                          double velocityX = 0.0, double velocityY = 0.0, double velocityZ = 0.0);

    /// parity: `Consumable.onConsume`, which is what makes a golden apple worth eating.
    void applyConsumeEffects(player::PlayerSession& session,
                             const std::vector<server::registry::ConsumeEffect>& effects);

    /// The satellites' delegates, kept because their callers (commands, portals, the plugin
    /// bridge) are too wide to move wholesale.
    void setGamemode(wire::protocol::play::Gamemode mode);

    bool switchWorld(world::LoadedWorld& target, double x, double y, double z);

    void teleportTo(double x, double y, double z, float yaw, float pitch, std::string_view cause = "command");

    [[nodiscard]] std::optional<storage::StoredPlayer> storedSnapshot() const;

    /// What the tick loop and the plugin bridge set: the player, and where they are.
    std::unique_ptr<player::PlayerSession> playerSession_;
    world::LoadedWorld* currentWorld_ = nullptr;

    /// What was last sent about how this player is standing and what level they are, so nothing
    /// goes out twice. On the session rather than on the entity tracker: it is what this connection
    /// last said about its own player, not what it has been told about anybody else.
    std::int32_t lastSharedFlags = -1;
    wire::protocol::play::Pose lastPose = wire::protocol::play::Pose::Standing;
    std::int32_t lastSyncedLevel = -1;

    /// Ticks lived, which is what the UI surfaces are paced by.
    std::int64_t uiTick = 0;
    std::int64_t firstSeenEpochSeconds = 0;
    std::int64_t playTimeTicks = 0;
    /// How far the player moved last tick, for the step sounds and the walked statistic.
    double lastStepSquared = 0.0;
    bool addressRecorded = false;

    /// parity of intent with Folia handing a player to the region they walked into.
    void handOverToRegionOf(world::LoadedWorld& world);

private:
    /// Everything queued has to be on the wire before the socket goes.
    void writeToSocket(const wire::io::ByteBuffer& frame);

    void sendUncompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody);

    void sendTranslated(const wire::multiversion::ProtocolChain& chain, std::int32_t id,
                        std::span<const std::uint8_t> body);

    /// This connection's own outbound loop, and the only thing that writes to it unprompted.
    void startOutboundLoop();

    void drainFrames();

    void handleFrame(wire::io::ByteBuffer& body);

    void handleFrameBody(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handleHandshake(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handleStatus(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handleLogin(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handleConfiguration(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handlePlay(std::int32_t packetId, wire::io::ByteBuffer& body);

    /// Everything the client is owed between the login sequence and its first tick.
    void enterPlay();

    void sendKeepAliveIfDue();

    void flushOutbox();

    /// One frame onto the wire, ciphered in send order. Only ever called with `sendMutex_` held.
    void writeFrame(std::span<const std::uint8_t> frame);

    void handleCustomPayload(wire::io::ByteBuffer& body);

    /// Runs `work` on the owner of the world this player is in, or here when there is none yet.
    void onOwner(std::function<void()> work);

    void broadcastChat(const std::string& message);

    void runCommand(const std::string& command);

    void sendSuggestions(const wire::protocol::play::CommandSuggestionRequestPacket& request);

    /// parity: `ClientboundUpdateRecipesPacket`, of which this sends the stonecutter half.
    void sendStonecutterRecipes();

    /// The callbacks a `PlayerSession` reaches this connection through.
    [[nodiscard]] player::PlayerConnection playerConnection();

    void handleLoginStart(wire::io::ByteBuffer& body);

    void handleKey(wire::io::ByteBuffer& body);

    void handleForwardingAnswer(wire::io::ByteBuffer& body);

    /// Either finishes the login with what a BungeeCord-family proxy put on the handshake, or asks
    /// a Velocity one for it and waits.
    void startForwardedLogin(const wire::protocol::login::LoginStartPacket& loginStart);

    void finishLogin(wire::common::Uuid id, const std::string& name,
                     const std::vector<wire::auth::SessionProfileProperty>& properties);

    /// Why this login may not proceed, or nothing when it may.
    [[nodiscard]] std::optional<wire::text::Component> loginRefusal(wire::common::Uuid id, const std::string& name);

    /// The one gate before a player exists at all. Answers whether it said no.
    [[nodiscard]] bool pluginRefusesLogin(const std::string& name, const std::string& profileId);

    /// Turns a client away while still in login, where a written reason still reaches the screen.
    void disconnectFromLogin(const std::string& miniMessage);

    void refuseVersion(const std::string& name, const wire::multiversion::ProtocolVerdict::Refused& refusal);

    void enterConfiguration();

    void sendRegistriesThenFinish();

    /// The numeric members of one tag, or empty where this build has no real data for it.
    [[nodiscard]] std::vector<std::int32_t> tagMembers(std::string_view registryId, std::string_view tagName) const;

    wire::io::TcpConnection connection_;
    Options options_;
    wire::common::Logger logger_{"Session"};
    net::SessionOutbox outbox_;
    net::ProxyLogin proxy_;
    std::mutex sendMutex_;
    wire::io::ByteBuffer incoming_;
    wire::protocol::ProtocolState state_ = wire::protocol::ProtocolState::Handshake;
    std::optional<wire::protocol::handshake::HandshakePacket> handshake_;
    wire::multiversion::ProtocolVerdict verdict_{wire::multiversion::ProtocolVerdict::Native{}};
    std::unique_ptr<wire::multiversion::java::JavaChain> translation_;
    std::optional<wire::auth::Cfb8Cipher> decryptCipher_;
    std::optional<wire::auth::Cfb8Cipher> encryptCipher_;
    bool compressionEnabled_ = false;
    ui::PlayerUi playerUi_;

    std::unique_ptr<ContainerMenuController> menus_;
    std::unique_ptr<BlockInteractionHandler> blockInteraction_;
    std::unique_ptr<ChunkStreamer> chunkStreamer_;
    std::unique_ptr<CombatHandler> combat_;
    std::unique_ptr<EntityVisibilityTracker> entityTracker_;
    std::unique_ptr<ItemUseTimers> itemUseTimers_;
    std::unique_ptr<PlayerLifecycle> lifecycle_;
    std::unique_ptr<UiStateSync> uiState_;
    std::unique_ptr<VillagerTradeController> trades_;

    std::chrono::steady_clock::time_point lastKeepAlive_ = std::chrono::steady_clock::now();
    std::atomic<bool> streaming_{false};
    /// A turn still running is not asked for a second one.
    std::atomic<bool> tickTaskPending_{false};
    std::atomic<bool> closeWhenDrained_{false};
    std::jthread outbound_;

    /// What every task this session hands another owner carries with it.
    ///
    /// A task queued on the world's owner runs on the tick thread one turn later, and the client
    /// can be gone by then: the socket closes on its own thread and this session is destroyed the
    /// moment `run` returns. The task asks `alive` before touching anything of this session's, and
    /// the destructor waits for whoever is already inside one, which is the half a flag alone
    /// cannot do.
    struct OwnerWork {
        std::atomic<bool> alive{true};
        std::atomic<std::int32_t> running{0};
    };

    std::shared_ptr<OwnerWork> ownerWork_{std::make_shared<OwnerWork>()};

    /// Remembered, not passed around: the only reader is the failure path in `run`, and knowing
    /// which packet a decoder died on is the difference between a one-line fix and a hunt.
    std::int32_t lastPacketId_ = -1;

    /// Which owner this player's packets go to, changed only between turns.
    sched::Owner* regionOwner_ = nullptr;

    /// What the login sequence is holding on to between its packets.
    std::optional<std::string> pendingUsername_;
    std::vector<std::uint8_t> challenge_;
    std::optional<wire::auth::SessionProfile> pendingProfile_;
    std::int8_t pendingSkinParts_ = static_cast<std::int8_t>(0x7F);
    std::optional<wire::auth::forwarding::ForwardingResult> forwardedHandshake_;
};

} // namespace basalt::server::runtime
