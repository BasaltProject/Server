#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `AbstractHorse` and `AbstractChestedHorse`.
namespace horses {

/// parity: `AbstractHorse.DATA_ID_FLAGS` and `AbstractChestedHorse.DATA_ID_CHEST`.
inline constexpr std::uint8_t kFlagsDataIndex = 18;
inline constexpr std::uint8_t kChestDataIndex = 19;
inline constexpr std::int8_t kFlagTame = 2;
inline constexpr std::int8_t kFlagBred = 8;
inline constexpr std::int8_t kFlagEating = 16;
inline constexpr std::int8_t kFlagStanding = 32;
inline constexpr std::int8_t kFlagOpenMouth = 64;

/// parity: `spawnTamingParticles`, smoke for a rider thrown and hearts for a horse tamed.
inline constexpr std::int8_t kEventTamingFailed = 6;
inline constexpr std::int8_t kEventTamingSucceeded = 7;

/// parity: `AbstractMountInventoryMenu.INVENTORY_ROWS`.
inline constexpr std::int32_t kInventoryRows = 3;
/// parity: the saddle and body armour slots ahead of a mount's own inventory.
inline constexpr std::int32_t kEquipmentSlots = 2;

/// parity: `Camel.DASH_COOLDOWN_TICKS`.
inline constexpr std::int32_t kDashCooldownTicks = 55;

/// parity: `AbstractHorse`'s counters.
struct HorseState {
    std::int32_t eatingCounter = 0;
    std::int32_t mouthCounter = 0;
    std::int32_t standCounter = 0;
    bool eating = false;
    bool standing = false;
    bool mouthOpen = false;
    /// parity: `Camel.dashCooldown` and `DASH`.
    std::int32_t dashCooldown = 0;
    bool dashing = false;
    bool hurt = false;
};

[[nodiscard]] bool isHorse(const wire::common::Identifier& type);

/// parity: `AbstractChestedHorse`: donkeys, mules and llamas.
[[nodiscard]] bool isChested(const wire::common::Identifier& type);

[[nodiscard]] bool isLlama(const wire::common::Identifier& type);

[[nodiscard]] bool isCamel(const wire::common::Identifier& type);

[[nodiscard]] HorseState& stateOf(ServerEntity& horse);

/// parity: `AbstractHorse.DATA_ID_FLAGS` as the client reads it.
[[nodiscard]] std::int8_t flagsOf(const ServerEntity& horse);

/// parity: `isTamed`, which a camel always is.
[[nodiscard]] bool tamed(const ServerEntity& horse);

/// parity: `tameWithName`'s owner and flag.
void tame(ServerEntity& horse, const wire::common::Uuid& owner);

/// parity: `getMaxTemper`, thirty for a llama.
[[nodiscard]] std::int32_t maxTemper(const wire::common::Identifier& type);

[[nodiscard]] std::int32_t temper(const ServerEntity& horse);

/// parity: `modifyTemper`, clamped to what the horse can hold.
std::int32_t modifyTemper(ServerEntity& horse, std::int32_t amount);

[[nodiscard]] bool hasChest(const ServerEntity& horse);

void setChest(ServerEntity& horse, bool chest);

/// parity: `getInventoryColumns`.
[[nodiscard]] std::int32_t inventoryColumns(const ServerEntity& horse);

/// parity: `Llama.getStrength`, one to five whatever was saved.
[[nodiscard]] std::int32_t strength(const ServerEntity& llama);

/// parity: `Mob.isSaddled`.
[[nodiscard]] bool saddled(const ServerEntity& horse);

[[nodiscard]] bool baby(const ServerEntity& horse);

/// parity: `Entity.isSilent`, its saved `Silent`.
[[nodiscard]] bool silent(const ServerEntity& horse);

/// parity: `isEquippableInSlot`.
enum class Wearable : std::uint8_t { None, Saddle, Body };

[[nodiscard]] Wearable wearableFor(const ServerEntity& horse, const wire::common::Identifier& item);

/// parity: `HorseInventoryMenu`'s two `isActive` checks.
[[nodiscard]] bool saddleSlotActive(const ServerEntity& horse);
[[nodiscard]] bool bodySlotActive(const ServerEntity& horse);

/// parity: `isFood`.
[[nodiscard]] bool isFood(const wire::common::Identifier& type, const wire::common::Identifier& item);

/// parity: `handleEating`'s table.
struct Feeding {
    float heal = 0.0F;
    std::int32_t ageUp = 0;
    std::int32_t temper = 0;
    bool love = false;
};

[[nodiscard]] Feeding feedingOf(const wire::common::Identifier& type, const wire::common::Identifier& item);

/// parity: `canPerformRearing`, which a camel and a llama cannot.
[[nodiscard]] bool canRear(const wire::common::Identifier& type);

/// parity: `canEatGrass`, which a llama cannot.
[[nodiscard]] bool canEatGrass(const wire::common::Identifier& type);

/// parity: `setStanding`.
void stand(ServerEntity& horse, std::int32_t ticks);

/// parity: `standIfPossible`.
void standIfPossible(ServerEntity& horse);

/// parity: `openMouth`.
void openMouth(ServerEntity& horse);

[[nodiscard]] std::string_view angrySound(const ServerEntity& horse);
[[nodiscard]] std::string_view eatingSound(const ServerEntity& horse);
[[nodiscard]] std::string_view ambientSound(const ServerEntity& horse);
[[nodiscard]] std::string_view jumpSound(const ServerEntity& horse);
[[nodiscard]] std::string_view chestSound(const ServerEntity& horse);
[[nodiscard]] std::string_view saddleSound(const ServerEntity& horse);

/// parity: `Mob.getVoicePitch` over its roll, higher for a foal.
[[nodiscard]] float voicePitch(const ServerEntity& horse, float roll);

/// parity: `AbstractHorse.canParent`: tame, grown, unhurt, in love and nobody aboard.
[[nodiscard]] bool canParent(const ServerEntity& horse);

[[nodiscard]] bool canMate(const ServerEntity& horse, const ServerEntity& partner);

/// parity: `getBreedOffspring`'s kind, a mule from a horse and a donkey.
[[nodiscard]] wire::common::Identifier offspringType(const ServerEntity& horse, const ServerEntity& partner);

/// parity: `getBreedOffspring`'s coat, markings, strength and `setOffspringAttributes`.
void inherit(ServerEntity& baby, const ServerEntity& horse, const ServerEntity& partner, random::RandomSource& random);

/// parity: `AbstractHorse.createOffspringAttribute`.
[[nodiscard]] double offspringAttribute(double parentA, double parentB, double min, double max,
                                        random::RandomSource& random);

} // namespace horses

} // namespace basalt::server::runtime::entity
