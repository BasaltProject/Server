#pragma once

#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

[[nodiscard]] std::vector<wire::protocol::play::SetEntityDataPacket::Entry>
generatedEntityMetadata(std::string_view type, const wire::nbt::NbtCompound& data);

struct GeneratedEquipment {
    wire::protocol::play::SetEquipmentPacket::SlotType slot =
        wire::protocol::play::SetEquipmentPacket::SlotType::MainHand;
    std::string item;
    std::int32_t count = 0;
    wire::nbt::NbtCompound components;
};

[[nodiscard]] std::vector<GeneratedEquipment> generatedEntityEquipment(const wire::nbt::NbtCompound& data);

/// parity: `VehicleEntity`'s hurt clock.
struct VehicleData {
    std::int32_t hurtTime = 0;
    std::int32_t hurtDir = 1;
    float damage = 0.0F;
    /// parity: `MinecartFurnace.DATA_ID_FUEL`.
    std::optional<bool> furnaceFuel;
    /// parity: `AbstractBoat.DATA_ID_BUBBLE_TIME`.
    std::optional<std::int32_t> bubbleTime;
    /// parity: `MinecartCommandBlock.DATA_ID_COMMAND_NAME`.
    std::optional<std::string> command;

    friend bool operator==(const VehicleData&, const VehicleData&) = default;
};

[[nodiscard]] std::optional<VehicleData> vehicleDataOf(const ServerEntity& entity);

void appendVehicleChanges(std::vector<wire::protocol::play::SetEntityDataPacket::Entry>& entries,
                          const VehicleData& now, const VehicleData* before);

struct RuntimeEntityMetadata {
    std::int32_t airSupply = 0;
    std::optional<bool> baby;
    std::optional<std::int32_t> cubeSize;
    std::optional<std::int8_t> horseFlags;
    std::optional<std::int8_t> livingFlags;
    /// parity: `AbstractChestedHorse.DATA_ID_CHEST` and `Camel.DASH`.
    std::optional<bool> horseChest;
    std::optional<bool> camelDash;
    /// parity: `ThrownTrident.ID_LOYALTY` and `ID_FOIL`, after `AbstractArrow`'s three.
    std::optional<std::int8_t> tridentLoyalty;
    std::optional<bool> tridentFoil;
    /// parity: `Creaking.CAN_MOVE`, `IS_ACTIVE`, `IS_TEARING_DOWN` and `HOME_POS`.
    std::optional<bool> creakingCanMove;
    std::optional<bool> creakingActive;
    std::optional<bool> creakingTearingDown;
    std::optional<std::optional<wire::protocol::play::BlockPos>> creakingHome;
    /// parity: `EndCrystal`'s beam and base.
    std::optional<std::optional<wire::protocol::play::BlockPos>> crystalBeam;
    std::optional<bool> crystalBottom;
    /// parity: `EnderDragon.DATA_PHASE`, which the client animates by.
    std::optional<std::int32_t> dragonPhase;
    /// parity: `ExperienceOrb.DATA_VALUE`, which picks the orb's size.
    std::optional<std::int32_t> orbValue;
    /// parity: `AreaEffectCloud`'s radius, waiting flag and particle.
    std::optional<float> cloudRadius;
    std::optional<bool> cloudWaiting;
    std::optional<wire::protocol::play::SetEntityDataPacket::ParticleValue> cloudParticle;
    /// parity: `FishingHook`'s hooked entity.
    std::optional<std::int32_t> hookedEntity;
    std::optional<bool> hookBiting;
    /// parity: `LivingEntity.DATA_EFFECT_PARTICLES` and `DATA_EFFECT_AMBIENCE_ID`.
    std::optional<wire::protocol::play::SetEntityDataPacket::ParticlesValue> effectParticles;
    std::optional<bool> effectsAmbient;
    /// parity: `Entity.DATA_POSE`.
    std::optional<wire::protocol::play::Pose> pose;
    std::optional<std::int32_t> wardenAnger;
    /// parity: `CopperGolem`'s weathering and the pose it works a chest in.
    std::optional<bool> dangerousSkull;
    std::optional<std::int32_t> copperWeather;
    std::optional<std::int32_t> copperPose;
    /// parity: `Shulker`'s attach face, raw peek and colour.
    std::optional<std::int32_t> shulkerFace;
    std::optional<std::int32_t> shulkerPeek;
    std::optional<std::int32_t> shulkerColor;
    /// parity: `FireworkRocketEntity`'s item.
    std::optional<wire::protocol::play::WireItemStack> fireworkItem;
    std::optional<std::optional<std::int32_t>> fireworkAttached;
    /// parity: `LivingEntity.DATA_STINGER_COUNT_ID`.
    std::optional<std::int32_t> stingers;
    std::optional<std::int32_t> arrows;
    /// parity: `Armadillo.ARMADILLO_STATE`.
    std::optional<std::int32_t> armadilloState;
    /// parity: `Sniffer.DATA_STATE` and `DATA_DROP_SEED_AT_TICK`.
    std::optional<std::int32_t> snifferState;
    std::optional<std::int32_t> snifferDropSeed;
    /// parity: `Pufferfish`'s PUFF_STATE.
    std::optional<std::int32_t> pufferfishPuff;
    /// parity: `Camel.LAST_POSE_CHANGE_TICK`.
    std::optional<std::int64_t> camelPoseTick;
    /// parity: `Frog.DATA_TONGUE_TARGET_ID`.
    std::optional<std::optional<std::int32_t>> frogTongue;
    std::optional<bool> fireworkShotAtAngle;
    /// parity: `Bee.DATA_FLAGS_ID` and `DATA_ANGER_END_TIME`.
    std::optional<std::int8_t> beeFlags;
    std::optional<std::int64_t> beeAngerEnd;
    /// parity: `Wolf.DATA_ANGER_END_TIME`, and `Enderman.DATA_CREEPY` and `DATA_STARED_AT`.
    std::optional<std::int64_t> wolfAngerEnd;
    std::optional<bool> wolfInterested;
    std::optional<bool> endermanCreepy;
    std::optional<bool> endermanStaredAt;
    /// parity: `Allay.DATA_DANCING` and `DATA_CAN_DUPLICATE`.
    std::optional<bool> allayDancing;
    std::optional<bool> allayCanDuplicate;
    /// parity: `Piglin`'s baby, crossbow and dance flags, one after `AbstractPiglin`'s own.
    std::optional<bool> piglinBaby;
    std::optional<bool> piglinCharging;
    std::optional<bool> piglinDancing;
    /// parity: `PolarBear.DATA_STANDING_ID`, `Strider.DATA_SUFFOCATING`, `Turtle.LAYING_EGG` and
    /// `Witch.DATA_USING_ITEM`.
    std::optional<bool> polarStanding;
    std::optional<bool> striderCold;
    std::optional<bool> turtleLaying;
    std::optional<bool> turtleHasEgg;
    std::optional<std::int32_t> sulfurMaxFuse;
    std::optional<bool> witchUsing;
    /// parity: `Pig.DATA_BOOST_TIME` and `Strider.DATA_BOOST_TIME`.
    std::optional<std::int32_t> boostTotal;
    /// parity: `Axolotl.DATA_PLAYING_DEAD`.
    std::optional<bool> axolotlPlayingDead;
    /// parity: `Goat`'s horns, which a ram into stone can knock off.
    std::optional<bool> goatLeftHorn;
    std::optional<bool> goatRightHorn;
    /// parity: `Mob.DATA_MOB_FLAGS_ID`, no AI, left handed and aggressive.
    std::optional<std::int8_t> mobFlags;
    /// parity: `Fox.DATA_FLAGS_ID` and `Panda.DATA_ID_FLAGS` with the panda's three counters.
    std::optional<std::int8_t> foxFlags;
    std::optional<std::int8_t> pandaFlags;
    std::optional<std::int32_t> pandaUnhappy;
    std::optional<std::int32_t> pandaSneeze;
    std::optional<std::int32_t> pandaEat;
    std::optional<VehicleData> vehicle;
    /// parity: `Entity`'s shared flags, custom name, name visibility, silence and gravity.
    std::int8_t sharedFlags = 0;
    std::optional<std::string> customName;
    bool nameVisible = false;
    bool silent = false;
    bool noGravity = false;

    [[nodiscard]] std::vector<wire::protocol::play::SetEntityDataPacket::Entry>
    changesFrom(const RuntimeEntityMetadata* previous) const;

    friend bool operator==(const RuntimeEntityMetadata&, const RuntimeEntityMetadata&) = default;
};

[[nodiscard]] RuntimeEntityMetadata runtimeEntityMetadata(const ServerEntity& entity);

/// parity: `Entity.DATA_SHARED_FLAGS_ID` as far as a non-player entity sets it: on fire, invisible and glowing.
[[nodiscard]] std::int8_t sharedFlagsOf(const ServerEntity& entity);

} // namespace basalt::server::runtime::entity
