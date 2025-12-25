#pragma once

#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/runtime/config/UiConfigs.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/net/ProxyLogin.hpp>
#include <basalt/server/runtime/net/SessionOutbox.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/ui/PlayerUi.hpp>
#include <basalt/wire/auth/Cfb8Cipher.hpp>
#include <basalt/wire/auth/SessionProfile.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/io/TcpSocket.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/text/LangTable.hpp>
#include <basalt/wire/multiversion/ProtocolGate.hpp>
#include <basalt/wire/protocol/ProtocolState.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {
class CommandDispatcher;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
class WorldState;
class ChunkGenerationQueue;
class RealLootTables;
class RealRecipes;
class RealVillagerTrades;
class RealEnchantments;
class Jukeboxes;
class PlayerProgress;
class AdvancementTriggers;
} // namespace basalt::server::runtime::world

namespace basalt::server::storage {
class PlayerStore;
}

namespace basalt::server::runtime {

class ChunkStreamer;
class CombatHandler;
class ContainerMenuController;
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
        std::function<std::optional<std::vector<std::uint8_t>>(std::int32_t id, std::span<const std::uint8_t> packet)>
            outgoing;
        /// The same on the way in.
        std::function<std::optional<std::vector<std::uint8_t>>(std::int32_t id, std::span<const std::uint8_t> packet)>
            incoming;
        /// Whether anybody is listening on a plugin channel, and what to do with what arrives.
        std::function<bool(const std::string& channel)> listensOn;
        std::function<void(const std::string& uuid, const std::string& channel, std::span<const std::uint8_t> payload)>
            channelMessage;
        /// The surfaces a plugin may take over for this player.
        ui::PlayerUi::Plugin surfaces;
        /// The NPCs a plugin is drawing in this world, and what a click on one does.
        std::function<std::vector<NpcView>()> npcs;
        std::function<bool(std::int32_t entityId, const std::string& playerUuid)> npcClicked;
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
        world::Jukeboxes* jukeboxes = nullptr;
        std::function<bool()> whitelistEnforced;
        world::RealEnchantments* enchantments = nullptr;
        world::PlayerProgress* progress = nullptr;
        world::AdvancementTriggers* advancementTriggers = nullptr;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        const world::realItemTags::TagMembers* itemTags = nullptr;
        const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
        /// Which client versions may in. Defaults to this build's own and nothing else.
        wire::multiversion::ProtocolGate protocolGate{{wire::multiversion::protocolVersions::kNative},
                                                      wire::multiversion::UnknownProtocolPolicy::Warn,
                                                      wire::multiversion::UnknownProtocolPolicy::Refuse};
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

    /// A chat line everybody on the server sees.
    void broadcastToEveryone(const wire::text::Component& message);

    /// Whether this player can reach that entity to hit it. parity: `Player.canReach`.
    [[nodiscard]] bool withinAttackReach(const player::PlayerSession& session,
                                         const entity::ServerEntity& target) const;

    /// The console's own language table, which is what a death message is written out with.
    [[nodiscard]] const wire::text::LangTable& consoleLang() const;

    /// A sound everybody near it hears, which is how a bow, a pearl and a step all travel.
    void broadcastSound(std::string_view sound, double x, double y, double z,
                        wire::protocol::play::SoundSource source, float volume = 1.0F, float pitch = 1.0F);

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

private:
    /// Everything queued has to be on the wire before the socket goes.
    void writeToSocket(const wire::io::ByteBuffer& frame);

    void sendUncompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody);

    void sendTranslated(const wire::multiversion::ProtocolChain& chain, std::int32_t id,
                        std::span<const std::uint8_t> body);

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
    std::unique_ptr<wire::multiversion::ProtocolChain> translation_;
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
    std::atomic<bool> closeWhenDrained_{false};
};

} // namespace basalt::server::runtime
