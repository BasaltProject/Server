#pragma once

#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/chat/SecureChat.hpp>
#include <basalt/server/runtime/config/UiConfigs.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/net/ProxyLogin.hpp>
#include <basalt/server/runtime/net/SessionOutbox.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/ui/PlayerUi.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/server/runtime/world/RealBannerPatterns.hpp>
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
#include <basalt/wire/protocol/EnvelopeEndpoint.hpp>
#include <basalt/wire/protocol/ProtocolState.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <array>
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
class RealBlockTransformers;
class RealEnchantments;
class PlayerProgress;
class AdvancementTriggers;
class PlayerAdvancements;
class WorldWeather;
class RealDialogs;
class RealTimelines;
} // namespace basalt::server::runtime::world

namespace basalt::server::storage {
class PlayerStore;
}

namespace basalt::server::runtime::plugins {
class EventSink;
}

namespace basalt::server::runtime {

class ChunkStreamer;
class CombatHandler;
class BlockChangeBroadcaster;
class ContainerMenuController;
class CombatSystem;
class VehicleSystem;
class EndFightSystem;
class FishingSystem;
class MapSystem;
class FireworkSystem;
class WitherSystem;
class MobActionSystem;
class StructureBlockSystem;
class ExplosionSystem;
class BlockInteractionHandler;
class ItemUseTimers;
class PlayerLifecycle;
class UiStateSync;
class VillagerTradeController;

class ClientSession {
public:
    /// parity: vanilla's own fifteen seconds between keep-alives.
    static constexpr std::int64_t kKeepAliveIntervalSeconds = 15;

    static constexpr std::int32_t kFirstBlockContainerId = 1;
    static constexpr std::int32_t kMaxContainerId = 100;

    static constexpr float kInitialChunksPerTick = 4.0F;

    struct PluginHooks {
        std::function<std::optional<std::string>(const std::string& name, const std::string& uuid,
                                                 const std::string& address)>
            loginRefused;
        std::function<std::optional<std::vector<std::uint8_t>>(wire::protocol::ProtocolState state, std::int32_t id,
                                                               std::span<const std::uint8_t> packet)>
            outgoing;
        std::function<std::optional<std::vector<std::uint8_t>>(wire::protocol::ProtocolState state, std::int32_t id,
                                                               std::span<const std::uint8_t> packet)>
            incoming;
        std::function<bool(const std::string& channel)> listensOn;
        std::function<void(const std::string& uuid, const std::string& channel, std::span<const std::uint8_t> payload)>
            channelMessage;

        struct Surfaces {
            std::function<bool()> holdsSidebar;
            std::function<std::optional<ui::SurfaceContent>(const std::string& playerUuid)> sidebar;
            std::function<bool()> holdsBossBar;
            std::function<std::optional<ui::SurfaceContent>(const std::string& playerUuid)> bossBar;
        };

        Surfaces surfaces;
        std::function<std::vector<NpcView>()> npcs;
        std::function<bool(std::int32_t entityId, const std::string& playerUuid)> npcClicked;
        std::function<bool(const player::PlayerSession& player, const std::string& item, std::int32_t count,
                           std::int32_t entityId)>
            pickupAllowed;
        std::function<bool(const player::PlayerSession& player, double fromX, double fromY, double fromZ, double toX,
                           double toY, double toZ)>
            moveAllowed;
        std::function<bool(const player::PlayerSession& player, std::string& message)> chatAllowed;
        std::function<bool(const player::PlayerSession& player, std::string& command)> commandAllowed;
        std::function<bool(const player::PlayerSession& player)> swapHandsAllowed;
        std::function<bool(std::string& message)> broadcastAllowed;

        std::function<void(const player::PlayerSession& player)> joined;
        std::function<void(const player::PlayerSession& player, bool sprinting)> sprintChanged;
        std::function<void(const player::PlayerSession& player, bool sneaking)> sneakChanged;
        std::function<void(const player::PlayerSession& player, std::int32_t slot, std::int32_t fromSlot)>
            heldSlotChanged;
        std::function<void(const player::PlayerSession& player, std::string_view kind)> menuClosed;
    };

    struct Options {
        std::function<wire::protocol::status::ServerStatusPayload(std::int32_t protocolVersion, const std::string& host,
                                                                  const std::optional<std::string>& address)>
            statusProvider;
        std::int32_t compressionThreshold = 256;
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
        /// parity: `WorldDataConfiguration.enabledFeatures`.
        std::function<std::vector<std::string>()> enabledFeatures;
        /// parity: `DedicatedServer.createServerLinks`; empty when unset or not http(s).
        std::string bugReportLink;

        struct ResourcePack {
            wire::common::Uuid id;
            std::string url;
            std::string hash;
            bool required = false;
            std::optional<wire::text::Component> prompt;
        };

        /// parity: `MinecraftServer.getServerResourcePack`.
        std::optional<ResourcePack> resourcePack;
        std::function<config::UiConfigs()> ui;
        const wire::common::placeholder::PlaceholderRegistry* placeholders = nullptr;
        world::RealLootTables* lootTables = nullptr;
        world::RealRecipes* recipes = nullptr;
        world::RealVillagerTrades* villagerTrades = nullptr;
        world::RealBlockTransformers* blockTransformers = nullptr;
        world::jukeboxes::SongOutputs* jukeboxes = nullptr;
        std::function<bool()> whitelistEnforced;
        world::RealEnchantments* enchantments = nullptr;
        world::PlayerProgress* progress = nullptr;
        world::PlayerAdvancements* advancements = nullptr;
        world::AdvancementTriggers* advancementTriggers = nullptr;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        const world::realItemTags::TagMembers* itemTags = nullptr;
        const world::realEnchantmentTags::TagMembers* enchantmentTags = nullptr;
        const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
        const world::realFluidTags::TagMembers* fluidTags = nullptr;
        const world::realTimelineTags::TagMembers* timelineTags = nullptr;
        const world::realEntityTypeTags::TagMembers* entityTypeTags = nullptr;
        const world::realBannerPatterns::TagOrder* bannerPatterns = nullptr;
        world::RealDialogs* dialogs = nullptr;
        world::RealTimelines* timelines = nullptr;
        const wire::multiversion::SubstitutionLabels* substitutionLabels = nullptr;
        const wire::auth::SessionKeyPair* keyPair = nullptr;
        const wire::auth::ServicesKeySet* servicesKeys = nullptr;
        bool enforceSecureProfile = false;
        const command::commands::BossBarRegistry* bossBars = nullptr;
        wire::multiversion::ProtocolGate protocolGate{{wire::multiversion::protocolVersions::kNative},
                                                      wire::multiversion::UnknownProtocolPolicy::Warn,
                                                      wire::multiversion::UnknownProtocolPolicy::Refuse};
        std::function<std::int64_t()> nextSeed;
        std::function<float()> nextFloat;
        std::function<double()> nextDouble;
        std::function<bool()> nextBool;
        BlockChangeBroadcaster* blocks = nullptr;
        ExplosionSystem* explosions = nullptr;
        CombatSystem* combat = nullptr;
        VehicleSystem* vehicles = nullptr;
        EndFightSystem* endFight = nullptr;
        FishingSystem* fishing = nullptr;
        MapSystem* maps = nullptr;
        WitherSystem* withers = nullptr;
        FireworkSystem* fireworks = nullptr;
        MobActionSystem* mobs = nullptr;
        StructureBlockSystem* structureBlocks = nullptr;
        std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t amount)>
            awardExperience;
        /// parity: `new ExperienceOrb`.
        std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t value)> spawnOrb;
        PluginHooks plugins;
        /// Where the session tells the plugins what happens; null in a test, and then nobody hears.
        plugins::EventSink* events = nullptr;
    };

    ClientSession(wire::io::TcpConnection connection, Options options);

    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    ClientSession(ClientSession&&) = delete;
    ClientSession& operator=(ClientSession&&) = delete;
    ~ClientSession();

    /// The whole session lives on this thread.
    void run();

    [[nodiscard]] const wire::common::Logger& logger() const noexcept { return logger_; }

    [[nodiscard]] const Options& options() const noexcept { return options_; }

    [[nodiscard]] wire::protocol::ProtocolState state() const noexcept { return state_; }

    [[nodiscard]] player::PlayerSession* playerSession() const noexcept { return playerSession_.get(); }

    [[nodiscard]] std::unique_ptr<player::PlayerSession> takePlayerSession() noexcept {
        return std::move(playerSession_);
    }

    [[nodiscard]] world::LoadedWorld* currentWorld() const noexcept { return currentWorld_; }

    [[nodiscard]] world::WorldState& world() const;

    [[nodiscard]] world::ChunkGenerationQueue& generationQueue() const;

    [[nodiscard]] std::string worldName() const;

    [[nodiscard]] std::optional<std::string> remoteAddress() const;

    [[nodiscard]] std::optional<std::string> rememberAddress() const;

    [[nodiscard]] ui::PlayerUi& playerUi() noexcept { return playerUi_; }

    void sendPlayPacket(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody);

    void sendCompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody);

    void disconnect(const wire::text::Component& reason);

    [[nodiscard]] ContainerMenuController& menus() noexcept { return *menus_; }

    [[nodiscard]] BlockInteractionHandler& blockInteraction() noexcept { return *blockInteraction_; }

    [[nodiscard]] ChunkStreamer& chunkStreamer() noexcept { return *chunkStreamer_; }

    [[nodiscard]] CombatHandler& combat() noexcept { return *combat_; }

    [[nodiscard]] EntityVisibilityTracker& entityTracker() noexcept { return *entityTracker_; }

    [[nodiscard]] ItemUseTimers& itemUseTimers() noexcept { return *itemUseTimers_; }

    [[nodiscard]] PlayerLifecycle& lifecycle() noexcept { return *lifecycle_; }

    [[nodiscard]] UiStateSync& uiState() noexcept { return *uiState_; }

    [[nodiscard]] VillagerTradeController& trades() noexcept { return *trades_; }

    [[nodiscard]] std::vector<player::PlayerSession*> playersInSameWorld() const;

    [[nodiscard]] wire::protocol::play::SpawnInfo spawnInfo(wire::protocol::play::Gamemode gamemode) const;

    void updatePosition(double x, double y, double z, float yaw, float pitch, bool onGround);

    /// parity: `ServerGamePacketListenerImpl.teleport`.
    void sendPlayerPosition(double x, double y, double z, float yaw, float pitch);

    /// parity: 26.3 `ServerPlayer.sendPostEffects`, the whole list, on join and on respawn.
    void sendPostEffects();

    /// parity: 26.3 `handleAcceptTeleportPacket`.
    void acceptTeleport(const wire::protocol::play::AcceptTeleportationPacket& accept);

    /// parity: `updateAwaitingTeleport`.
    [[nodiscard]] bool updateAwaitingTeleport();

    /// parity: `ServerPlayer.tick`'s own pickup pass.
    void tryPickupNearbyItems();

    void hurt(float amount, std::string_view deathMessageId, const wire::common::Identifier& damageType,
              const std::optional<entity::DamageSource>& source = std::nullopt);

    void heal(float amount);

    void killPlayer(std::string_view deathMessageId);

    void wakeUp();

    void sendPluginNotice(const std::string& message);

    void sendActionBar(const wire::text::Component& message);

    /// parity: `ServerGamePacketListenerImpl`'s `SWAP_ITEM_WITH_OFFHAND`: the F key.
    void swapWithOffhand();

    void broadcastToEveryone(const wire::text::Component& message);

    /// parity: `Player.canReach`.
    [[nodiscard]] bool withinAttackReach(const player::PlayerSession& session,
                                         const entity::ServerEntity& target) const;

    [[nodiscard]] const wire::text::LangTable& consoleLang() const;

    void resendChunk(std::int32_t chunkX, std::int32_t chunkZ);

    void sendBiomes(const std::vector<std::pair<std::int32_t, std::int32_t>>& chunks);

    /// parity: `Entity.stopRiding`: off the cart, standing where it left them.
    void dismount(player::PlayerSession& session);

    void sendPluginText(const wire::text::Component& text, bool actionBar);

    /// parity: `LivingEntity.addEffect`, for what a mob puts on somebody rather than a drink.
    void addEffect(const wire::common::Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier);

    void syncHeldAttributes();

    void syncExperience();

    void sendStats();

    void sendTime(world::LoadedWorld& world);

    void sendWeather(const world::WorldWeather& weather);

    void sendCommandTree();

    void broadcastSound(std::string_view sound, double x, double y, double z, wire::protocol::play::SoundSource source,
                        float volume = 1.0F, float pitch = 1.0F);

    void sendInventory();
    void sendInventoryAll();
    void sendInventorySlot(std::int32_t slot);
    void syncHealth();

    void spawnDroppedItem(const wire::common::Identifier& item, std::int32_t count, double x, double y, double z,
                          double velocityX = 0.0, double velocityY = 0.0, double velocityZ = 0.0);

    /// parity: `Consumable.onConsume`, which is what makes a golden apple worth eating.
    void applyConsumeEffects(player::PlayerSession& session,
                             const std::vector<server::registry::ConsumeEffect>& effects);

    void setGamemode(wire::protocol::play::Gamemode mode);

    bool switchWorld(world::LoadedWorld& target, double x, double y, double z);

    void teleportTo(double x, double y, double z, float yaw, float pitch, std::string_view cause = "command");

    [[nodiscard]] std::optional<storage::StoredPlayer> storedSnapshot() const;

    std::unique_ptr<player::PlayerSession> playerSession_;
    world::LoadedWorld* currentWorld_ = nullptr;

    std::int32_t lastSyncedLevel = -1;

    std::int64_t uiTick = 0;
    std::int64_t firstSeenEpochSeconds = 0;
    std::int64_t playTimeTicks = 0;
    double lastStepSquared = 0.0;
    bool addressRecorded = false;

    void handOverToRegionOf(world::LoadedWorld& world);

private:
    void writeToSocket(const wire::io::ByteBuffer& frame);

    void sendUncompressed(std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& writeBody);

    void sendTranslated(const wire::multiversion::ProtocolChain& chain, std::int32_t id,
                        std::span<const std::uint8_t> body);

    void startOutboundLoop();

    void drainFrames();

    void handleFrame(wire::io::ByteBuffer& body);

    /// One packet of the client as it speaks it, once any envelope around it is gone.
    void handleOriginalPacket(wire::io::ByteBuffer& body);

    /// A frame of the session envelope: the Open that starts it, or a packet of the client carried inside.
    void handleEnvelopeFrame(wire::io::ByteBuffer& body);

    /// Takes on the protocol the proxy says the client speaks, as if that client had connected here itself.
    void activateEnvelope(const wire::protocol::sessionEnvelope::Open& open);

    void sendEnvelopeControl(std::span<const std::uint8_t> control);

    /// Frames one packet for the wire: inside a data frame once the envelope is active.
    void frameOutgoing(std::span<const std::uint8_t> whole);

    void handleFrameBody(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handleHandshake(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handleStatus(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handleLogin(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handleConfiguration(std::int32_t packetId, wire::io::ByteBuffer& body);

    void handlePlay(std::int32_t packetId, wire::io::ByteBuffer& body);

    void answerForm(const wire::protocol::play::ServerboundCustomClickActionPacket& packet);

    void enterPlay();

    void sendKeepAliveIfDue();

    void flushOutbox();

    void writeFrame(std::span<const std::uint8_t> frame);

    void handleCustomPayload(wire::io::ByteBuffer& body);

    void onOwner(std::function<void()> work);

    bool handleChatPacket(std::int32_t packetId, wire::io::ByteBuffer& body);

    /// parity: `tryHandleChat`.
    [[nodiscard]] bool tryHandleChat(const std::string& message, bool isCommand);

    void broadcastChat(const chat::PlayerChatMessage& message);

    void runCommand(const std::string& command, const chat::SignedArguments* signedArguments = nullptr);

    void runSignedCommand(const wire::protocol::play::SignedChatCommandPacket& packet,
                          const std::vector<chat::MessageSignature>& lastSeen);

    /// parity: `sendPlayerChatMessage` and `sendDisguisedChatMessage`.
    void deliverChat(const chat::PlayerChatMessage& message, const chat::BoundChatType& chatType);

    /// parity: `detectRateSpam`, which spares operators.
    void detectSpam(chat::ChatThrottle& throttle);

    [[nodiscard]] bool translatedClient() const;

    void sendSuggestions(const wire::protocol::play::CommandSuggestionRequestPacket& request);

    /// parity: `ClientboundUpdateRecipesPacket`, of which this sends the stonecutter half.
    void sendStonecutterRecipes();

    void seenAdvancements(wire::io::ByteBuffer& body);

    [[nodiscard]] player::PlayerConnection playerConnection();

    void handleLoginStart(wire::io::ByteBuffer& body);

    void handleKey(wire::io::ByteBuffer& body);

    void handleForwardingAnswer(wire::io::ByteBuffer& body);

    void startForwardedLogin(const wire::protocol::login::LoginStartPacket& loginStart);

    void finishLogin(wire::common::Uuid id, const std::string& name,
                     const std::vector<wire::auth::SessionProfileProperty>& properties);

    [[nodiscard]] std::optional<wire::text::Component> loginRefusal(wire::common::Uuid id, const std::string& name);

    [[nodiscard]] bool pluginRefusesLogin(const std::string& name, const std::string& profileId);

    void disconnectFromLogin(const std::string& miniMessage);

    void refuseVersion(const std::string& name, const wire::multiversion::ProtocolVerdict::Refused& refusal);

    void enterConfiguration();

    void sendRegistriesThenFinish();

    void finishConfiguration();

    void handleResourcePackResponse(const wire::protocol::configuration::ResourcePackResponsePacket& response,
                                    bool configuring);

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
    /// Present from the login the proxy selected the envelope for: the next frame it sends is the Open.
    std::optional<wire::protocol::sessionEnvelope::EnvelopeEndpoint> envelope_;
    std::optional<wire::auth::Cfb8Cipher> decryptCipher_;
    std::optional<wire::auth::Cfb8Cipher> encryptCipher_;
    bool compressionEnabled_ = false;
    ui::PlayerUi playerUi_;

    std::mutex formsLock_;
    std::map<std::int32_t, std::shared_ptr<const world::dialogs::Dialog>> forms_;
    std::int32_t nextFormId_ = 1;
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
    std::atomic<bool> tickTaskPending_{false};
    std::atomic<bool> closeWhenDrained_{false};
    std::jthread outbound_;

    struct OwnerWork {
        std::atomic<bool> alive{true};
        std::atomic<std::int32_t> running{0};
    };

    std::shared_ptr<OwnerWork> ownerWork_{std::make_shared<OwnerWork>()};

    std::int32_t lastPacketId_ = -1;

    sched::Owner* regionOwner_ = nullptr;

    std::optional<std::string> pendingUsername_;
    std::vector<std::uint8_t> challenge_;
    std::optional<wire::auth::SessionProfile> pendingProfile_;
    std::int8_t pendingSkinParts_ = static_cast<std::int8_t>(0x7F);
    std::int32_t pendingMainHand_ = 1;
    std::int32_t pendingChatVisibility_ = 0;
    /// parity: `ClientInformation.viewDistance`, kept until the chunk view exists to take it.
    std::int32_t pendingViewDistance_ = 2;
    bool awaitingResourcePack_ = false;
    std::unique_ptr<chat::SecureChat> secureChat_;
    chat::ChatThrottle chatSpam_;
    chat::ChatThrottle commandSpam_;
    std::optional<wire::auth::forwarding::ForwardingResult> forwardedHandshake_;
    /// Sent from the tick thread, accepted on the network one.
    std::mutex teleportMutex_;
    std::int32_t awaitingTeleport_ = 0;
    std::optional<std::array<double, 3>> awaitingPosition_;
    std::int64_t awaitingTeleportTime_ = 0;
};

} // namespace basalt::server::runtime
