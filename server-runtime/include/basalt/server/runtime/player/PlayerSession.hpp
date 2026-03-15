#pragma once

#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/ExperienceData.hpp>
#include <basalt/server/runtime/player/FoodData.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerStats.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/WardenSpawnTracker.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {

/// Server-side state for one logged-in player: identity, live position, and the survival state the
/// client draws in its HUD.
///
/// Health, hunger and experience live here rather than on the connection because they outlive any
/// single packet exchange and are what a respawn resets; the connection is only how they reach the
/// client, and `players.db` is where it survives a rejoin.
class PlayerSession {
public:
    static constexpr float kMaxHealth = 20.0F;

    /// Every layer on, which is what a client that has not told us otherwise almost certainly has.
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

    /// Which permission group they belong to, loaded from `players.db` at join. Unset means the
    /// group currently marked as the default, which is resolved when a permission is checked rather
    /// than written into the row.
    std::optional<std::string> group;

    /// Which world this player is in. Sweeps use it so one world's chunks are not held by another's
    /// players.
    std::string worldName;

    wire::protocol::play::Gamemode gamemode = wire::protocol::play::Gamemode::Survival;
    bool sprinting = false;

    /// Whether they are crouching, from `player_input`'s own bitmask. Read because sneaking is what
    /// tells "place a block against this one" apart from "open this one's screen".
    bool sneaking = false;

    /// Whether they are in the middle of using something: eating, drinking, drawing a bow, holding
    /// a shield up. parity: `LivingEntity.isUsingItem`, which is what draws the animation on
    /// everybody else's screen.
    bool usingItem = false;
    /// The slot of the shield held up, and for how many ticks. parity: `useItem` with
    /// `blocks_attacks`, which only blocks once its delay has passed.
    std::optional<std::int32_t> blockingSlot;
    std::int32_t blockingTicks = 0;

    /// What they are riding, by entity id. parity: `Entity.vehicle`.
    std::optional<std::int32_t> vehicleId;
    /// The bobber this player has out, by entity id. parity: `Player.fishing`.
    std::optional<std::int32_t> fishingHook;

    /// The movement keys held, as `player_input` sends them.
    ///
    /// parity: `ServerPlayer.getLastClientMoveIntent`, which is what nudges a minecart along when
    /// its rider leans on a key.
    bool inputForward = false;
    bool inputBack = false;
    bool inputLeft = false;
    bool inputRight = false;

    /// The signed profile properties from the session server, carrying this player's skin and cape.
    /// Empty in offline mode, where nothing signs them and every client draws the default skin.
    std::vector<wire::protocol::login::GameProfileProperty> profileProperties;

    /// Which skin layers this player has switched on in their own video settings (hat, jacket,
    /// sleeves, trouser legs). The client owns this and reports it; the server only relays it to
    /// everyone else, or nobody would see the outer layer of anyone's skin.
    std::int8_t skinParts = kAllSkinParts;

    [[nodiscard]] float health() const noexcept { return health_; }

    void setHealth(float value) noexcept { health_ = value; }

    /// What this player's health tops out at, which vanilla keeps as the `max_health` attribute.
    ///
    /// Settable because a plugin can move it, and moving it has to mean something: every heal and
    /// every clamp reads this, so twenty hearts on the screen and twenty hearts in the damage
    /// arithmetic stay the same number.
    [[nodiscard]] float maxHealth() const noexcept { return maxHealth_; }

    void setMaxHealth(float value);

    /// Attribute bases a plugin set, by their registry name. Empty for almost every player.
    std::map<std::string, double, std::less<>> attributeBases;

    /// parity: `LivingEntity.getAbsorptionAmount`, the pool damage eats before health does.
    float absorption = 0.0F;

    /// Where this player is asleep. parity: `LivingEntity.sleepingPos`.
    std::optional<std::array<std::int32_t, 3>> sleepingBed;

    /// parity: `ServerPlayer.raidOmenPosition`, where the raid starts when the omen runs out.
    std::optional<std::array<std::int32_t, 3>> raidOmenPosition;
    std::int32_t sleepTicks = 0;

    /// parity: `LivingEntity.invulnerableTime`, the half second after a hit when most others miss.
    std::int32_t invulnerableTicks = 0;

    /// parity: `LivingEntity.lastHurt`: inside that window only a bigger hit lands, and only the
    /// rest of it.
    float lastHurt = 0.0F;

    /// Ticks since the last swing, which is what a hit is charged from.
    ///
    /// parity: `Player.attackStrengthTicker`. A swing before the bar refills lands a fifth of the
    /// damage, which is the whole reason a player waits between hits.
    std::int32_t ticksSinceSwing = 0;

    FoodData food;
    ExperienceData experience;

    /// parity: `Player.enchantmentSeed`, which is what fixes an enchanting table's three offers
    /// until one of them is taken. On the live session only: vanilla writes it as `XpSeed`, which
    /// here needs a column in `players.db`, so the offers re-roll on a rejoin.
    std::int32_t enchantmentSeed = 0;

    /// Scoreboard tags, as `/tag` sets them. On the live session only: vanilla keeps them with the
    /// saved entity, which here needs a column in `players.db`.
    std::set<std::string, std::less<>> tags;

    /// Counters, incremented as this server notices things happening.
    PlayerStats stats;

    /// The 41 slots this player carries.
    PlayerInventory inventory;

    /// The twenty-seven slots that follow this player rather than the block.
    ///
    /// parity: `PlayerEnderChestContainer`, which is why every ender chest in the world shows the
    /// same things to one player and different things to the next. Persisted in `players.db`, not
    /// in any world.
    std::array<std::optional<storage::StoredItemStack>, storage::StoredPlayer::kEnderChestSlots> enderChest;

    /// Air left and how long this player keeps burning.
    ///
    /// parity: `Entity`'s own two synced values. The client draws the bubbles and the flames from
    /// what the server sends, and moves neither on its own.
    std::int32_t airSupply = world::fluids::kMaxAir;

    std::int32_t fireTicks = 0;

    /// What the criteria objectives were last told. parity: `ServerPlayer.lastRecorded*`, which start
    /// where no real value is so the first tick reports every one.
    struct RecordedScores {
        float health = std::numeric_limits<float>::denorm_min();
        std::int32_t food = std::numeric_limits<std::int32_t>::min();
        std::int32_t air = std::numeric_limits<std::int32_t>::min();
        std::int32_t armor = std::numeric_limits<std::int32_t>::min();
        std::int32_t level = std::numeric_limits<std::int32_t>::min();
        std::int32_t experience = std::numeric_limits<std::int32_t>::min();
    };

    RecordedScores recordedScores;

    /// Deaths the scoreboard has not counted yet, and whose hit caused the last, by entity id. A
    /// counter rather than the dead flag: a player can respawn before the scoring pass runs.
    std::atomic<std::int32_t> deathsToScore{0};
    std::optional<std::int32_t> killerEntityId;
    /// parity: `LivingEntity.lastHurtByPlayer` and its hundred-tick memory, by entity id, which is
    /// who gets the kill credit.
    std::optional<std::int32_t> lastHurtByPlayerId;
    std::atomic<std::int32_t> lastHurtByPlayerTime{0};

    /// What this player may not use yet, and the turn of the world each one frees up on.
    ///
    /// parity: `ItemCooldowns`, which belongs to the player and not to the connection: both doors
    /// throw the same pearl, so both wait the same.
    std::map<std::string, std::int64_t, std::less<>> itemCooldowns;

    /// Status effects this player carries, by effect id. parity: `LivingEntity.activeEffects`.
    std::map<wire::common::Identifier, world::ActiveEffect> effects;

    /// How close this player is to summoning a warden. parity: `ServerPlayer.wardenSpawnTracker`,
    /// kept for the session only.
    world::wardenSpawnTracker::Tracker wardenTracker;

    /// How long this player has been standing in a portal, and how long until one works again.
    ///
    /// parity: `Entity.portalProcess`/`portalCooldown`, counted on the player rather than on the
    /// connection: a portal half entered survives a chunk reload the way vanilla's does.
    std::int32_t portalTicks = 0;

    std::int32_t portalCooldown = 0;

    /// Set the moment health hits zero, cleared on respawn. A dead player stops being ticked.
    bool dead = false;

    /// Client-driven, and only meaningful when the game mode allows it.
    bool flying = false;

    [[nodiscard]] bool mayFly() const noexcept;

    [[nodiscard]] bool invulnerable() const noexcept;

    [[nodiscard]] bool instabuild() const noexcept;

    void sendSystemMessage(const wire::text::Component& message) const;

    /// Back to a freshly spawned player: full health, full food, no experience kept.
    void resetForRespawn();

private:
    wire::common::Uuid uuid_;
    std::string name_;
    std::int32_t entityId_ = 0;
    PlayerConnection connection_;
    float health_ = kMaxHealth;
    float maxHealth_ = kMaxHealth;
};

} // namespace basalt::server::runtime::player
