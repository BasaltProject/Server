#pragma once

#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/ExperienceData.hpp>
#include <basalt/server/runtime/player/FoodData.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerExtras.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerStats.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/WardenSpawnTracker.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::plugins {
class EventSink;
}

namespace basalt::server::runtime::player {

class PlayerSession {
public:
    static constexpr float kMaxHealth = 20.0F;

    static constexpr std::int8_t kAllSkinParts = 0x7F;

    PlayerSession(wire::common::Uuid uuid, std::string name, std::int32_t entityId, PlayerConnection connection,
                  wire::protocol::play::Gamemode gamemode = wire::protocol::play::Gamemode::Survival);

    [[nodiscard]] const wire::common::Uuid& uuid() const noexcept { return uuid_; }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] std::int32_t entityId() const noexcept { return entityId_; }

    [[nodiscard]] const PlayerConnection& connection() const noexcept { return connection_; }

    [[nodiscard]] std::int32_t protocolVersion() const noexcept { return connection_.protocolVersion; }

    [[nodiscard]] const std::string& connectedHost() const noexcept { return connection_.connectedHost; }

    [[nodiscard]] const std::optional<std::string>& address() const noexcept { return connection_.address; }

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    bool onGround = false;
    bool fallFlying = false;
    std::int32_t fallFlyTicks = 0;

    std::optional<std::string> group;

    /// What each plugin grants or denies this session over the group; one answer wins by specificity.
    std::map<std::string, std::map<std::string, bool, std::less<>>, std::less<>> permissionGrants;

    [[nodiscard]] std::optional<bool> permissionGrant(std::string_view node) const;

    std::string worldName;

    wire::protocol::play::Gamemode gamemode = wire::protocol::play::Gamemode::Survival;
    bool sprinting = false;

    bool sneaking = false;

    bool usingItem = false;
    std::optional<std::int32_t> blockingSlot;
    std::int32_t blockingTicks = 0;
    bool usingOffhand = false;
    std::int32_t autoSpinTicks = 0;
    float autoSpinDamage = 0.0F;
    std::optional<storage::StoredItemStack> autoSpinItem;

    bool swimming = false;
    wire::protocol::play::Pose pose = wire::protocol::play::Pose::Standing;
    std::int32_t mainArm = 1;
    std::int32_t arrowCount = 0;
    std::int32_t removeArrowTime = 0;
    std::int32_t stingerCount = 0;
    std::int32_t removeStingerTime = 0;
    std::optional<wire::nbt::NbtCompound> shoulderLeft;
    std::optional<wire::nbt::NbtCompound> shoulderRight;
    std::int64_t timeEntitySatOnShoulder = 0;
    bool shoulderDropPending = false;
    double fallDistance = 0.0;
    /// parity: `Entity.moveDist` and `nextStep` for the footsteps everyone else hears.
    std::optional<std::array<double, 3>> emittedFrom;
    std::array<double, 3> knownMovement{};
    std::array<double, 3> deltaMovement{};
    float moveDist = 0.0F;
    float nextStep = 1.0F;
    std::int64_t lastCrystalSoundTime = 0;
    float crystalSoundIntensity = 0.0F;
    std::optional<std::array<double, 3>> impulseImpact;
    std::int32_t impulseGraceTicks = 0;

    std::optional<std::int32_t> vehicleId;
    std::array<double, 2> seenAt{};
    bool movedHorizontally = false;
    std::optional<std::int32_t> fishingHook;

    bool inputForward = false;
    bool inputBack = false;
    bool inputLeft = false;
    bool inputRight = false;

    std::vector<wire::protocol::login::GameProfileProperty> profileProperties;

    std::int8_t skinParts = kAllSkinParts;

    /// parity: `ChatVisiblity`.
    enum class ChatVisibility : std::uint8_t { Full, System, Hidden };
    std::atomic<ChatVisibility> chatVisibility{ChatVisibility::Full};

    [[nodiscard]] std::optional<wire::protocol::play::RemoteChatSessionData> chatSession() const;
    void setChatSession(std::optional<wire::protocol::play::RemoteChatSessionData> session);

    [[nodiscard]] float health() const noexcept { return health_; }

    void setHealth(float value) noexcept { health_ = value; }

    [[nodiscard]] float maxHealth() const noexcept { return maxHealth_; }

    void setMaxHealth(float value);

    [[nodiscard]] float luck() const;

    std::map<std::string, double, std::less<>> attributeBases;

    /// parity: `LivingEntity.getAbsorptionAmount`, the pool damage eats before health does.
    float absorption = 0.0F;

    std::optional<std::array<std::int32_t, 3>> sleepingBed;

    std::optional<std::array<std::int32_t, 3>> raidOmenPosition;
    std::int32_t sleepTicks = 0;

    /// parity: `LivingEntity.invulnerableTime`.
    std::int32_t invulnerableTicks = 0;

    bool lastHurtLanded = false;

    /// parity: `CombatEntry`, kept raw; the tracker's own status rules are read on top of them.
    struct CombatRecord {
        std::string damageType;
        std::string messageId;
        std::int32_t directId = -1;
        std::int32_t causeId = -1;
        float damage = 0.0F;
        float fallDistance = 0.0F;
        std::string fallLocation;
        std::int64_t tick = 0;
    };

    static constexpr std::size_t kMaxCombatRecords = 256;
    std::vector<CombatRecord> combatRecords;
    /// parity: `Entity.DATA_TICKS_FROZEN`.
    std::int32_t ticksFrozen = 0;
    /// parity: `Entity.isInPowderSnow`.
    bool inPowderSnow = false;
    /// parity: the `powder_snow` movement speed modifier `LivingEntity.tryAddFrost` adds.
    float frostSlowdown = 0.0F;
    std::int64_t aliveTicks = 0;
    /// parity: `CombatTracker.inCombat`.
    bool inCombat = false;

    /// parity: `LivingEntity.lastHurt`.
    float lastHurt = 0.0F;

    /// parity: `Player.attackStrengthTicker`.
    std::int32_t ticksSinceSwing = 0;
    std::array<std::string, 4> armorSeen{};
    bool armorKnown = false;

    FoodData food;
    ExperienceData experience;

    std::int32_t enchantmentSeed = 0;

    std::set<std::string, std::less<>> tags;
    std::vector<std::string> postEffects;

    PlayerStats stats;

    /// Where this player's own state tells the plugins what happens; null until the session is wired.
    const plugins::EventSink* events = nullptr;

    PlayerInventory inventory;

    std::array<std::optional<storage::StoredItemStack>, storage::StoredPlayer::kEnderChestSlots> enderChest;

    std::int32_t airSupply = world::fluids::kMaxAir;

    std::int32_t fireTicks = 0;

    /// parity: `Player.lastDeathLocation`, what a recovery compass points at.
    std::optional<wire::protocol::play::GlobalPos> lastDeath;

    struct RecordedScores {
        float health = std::numeric_limits<float>::denorm_min();
        std::int32_t food = std::numeric_limits<std::int32_t>::min();
        std::int32_t air = std::numeric_limits<std::int32_t>::min();
        std::int32_t armor = std::numeric_limits<std::int32_t>::min();
        std::int32_t level = std::numeric_limits<std::int32_t>::min();
        std::int32_t experience = std::numeric_limits<std::int32_t>::min();
    };

    RecordedScores recordedScores;

    std::atomic<std::int32_t> deathsToScore{0};
    std::optional<std::int32_t> killerEntityId;
    std::optional<std::int32_t> lastHurtByPlayerId;
    std::atomic<std::int32_t> lastHurtByPlayerTime{0};
    std::optional<wire::common::Identifier> lastHurtByMobType;
    std::atomic<std::int32_t> lastHurtByMobTime{0};
    std::optional<std::array<double, 3>> explosionImpact;
    std::optional<std::int32_t> explosionCause;

    /// parity: `ItemCooldowns`.
    std::map<std::string, std::int64_t, std::less<>> itemCooldowns;

    std::map<wire::common::Identifier, world::ActiveEffect> effects;

    std::vector<std::optional<std::int32_t>> effectSources;

    void noteEffectSource(std::optional<std::int32_t> sourceId) { effectSources.push_back(sourceId); }

    world::wardenSpawnTracker::Tracker wardenTracker;

    std::int32_t portalTicks = 0;

    std::int32_t portalCooldown = 0;

    bool dead = false;

    bool flying = false;

    PlayerExtras extras;

    [[nodiscard]] bool mayFly() const noexcept;

    [[nodiscard]] bool invulnerable() const noexcept;

    [[nodiscard]] bool instabuild() const noexcept;

    void sendSystemMessage(const wire::text::Component& message) const;

    /// parity: `ServerPlayer.sendChatMessage`, which only a full chat shows.
    void sendChatMessage(const chat::PlayerChatMessage& message, const chat::BoundChatType& chatType) const;

    void resetForRespawn();

private:
    wire::common::Uuid uuid_;
    std::string name_;
    std::int32_t entityId_ = 0;
    PlayerConnection connection_;
    float health_ = kMaxHealth;
    float maxHealth_ = kMaxHealth;
    mutable std::mutex chatSessionMutex_;
    std::optional<wire::protocol::play::RemoteChatSessionData> chatSession_;
};

} // namespace basalt::server::runtime::player
