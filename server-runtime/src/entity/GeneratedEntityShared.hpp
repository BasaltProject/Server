#pragma once

#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>

#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::runtime::entity::presentation {
/// parity: `LivingEntity.DATA_EFFECT_PARTICLES` and `DATA_EFFECT_AMBIENCE_ID`.
constexpr std::uint8_t kEffectParticlesDataIndex = 10;
constexpr std::uint8_t kEffectAmbienceDataIndex = 11;
/// parity: `LivingEntity.DATA_STINGER_COUNT_ID`, after the arrows stuck in it.
constexpr std::uint8_t kArrowCountDataIndex = 12;
/// parity: `Wolf`'s anger after the tamable's two and its own interest and collar.
constexpr std::uint8_t kWolfInterestedDataIndex = 20;
constexpr std::uint8_t kWolfAngerEndDataIndex = 22;
/// parity: `Enderman`'s creepy and stared-at flags after its carried block at 16.
constexpr std::uint8_t kEndermanCreepyDataIndex = 17;
constexpr std::uint8_t kEndermanStaredAtDataIndex = 18;
constexpr std::uint8_t kStingerCountDataIndex = 13;
/// parity: `Fox.DATA_TYPE_ID`, before the byte-valued `DATA_FLAGS_ID` at 19.
constexpr std::uint8_t kFoxVariantDataIndex = 18;

/// parity: `ExperienceOrb.DATA_VALUE`.
constexpr std::uint8_t kOrbValueDataIndex = 8;

/// parity: `Pufferfish.PUFF_STATE`, the integer right after `AbstractFish`'s moving flag.
constexpr std::uint8_t kPufferfishPuffDataIndex = 17;

using wire::nbt::NbtCompound;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SetEquipmentPacket;
using Entry = SetEntityDataPacket::Entry;
using Entries = std::vector<Entry>;
using RegistryHolder = wire::protocol::play::RegistryHolder;

/// `AbstractCubeMob.ID_SIZE`: two `AgeableMob` fields follow the common index 16.
constexpr std::uint8_t kCubeSizeDataIndex = 18;
constexpr std::uint8_t kAbstractHorseFlagsDataIndex = 18;
constexpr std::uint8_t kTridentLoyaltyDataIndex = 11;
constexpr std::uint8_t kTridentFoilDataIndex = 12;
constexpr std::uint8_t kMobFlagsDataIndex = 15;
/// parity: `Fox.DATA_FLAGS_ID` after the type, and `Panda`'s counters and flags after the genes.
constexpr std::uint8_t kFoxFlagsDataIndex = 20;
constexpr std::uint8_t kPandaUnhappyDataIndex = 18;
constexpr std::uint8_t kPandaSneezeDataIndex = 19;
constexpr std::uint8_t kPandaEatDataIndex = 20;
constexpr std::uint8_t kPandaFlagsDataIndex = 23;
constexpr std::uint8_t kAirSupplyDataIndex = 1;
constexpr std::int32_t kDefaultAirSupply = 300;

/// parity: `AbstractHorse`, whose tame flag every horse-like mob shares.
inline const std::set<std::string, std::less<>>& abstractHorseTypes() {
    static const std::set<std::string, std::less<>> kTypes{
        "horse", "donkey", "mule", "skeleton_horse", "zombie_horse", "llama", "trader_llama", "camel", "camel_husk"};
    return kTypes;
}
} // namespace basalt::server::runtime::entity::presentation
