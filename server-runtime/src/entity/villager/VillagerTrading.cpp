#include "BrainInternals.hpp"

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::entity::villager::villagerBrain {
namespace {

/// parity: `Villager.setUnhappy`'s forty ticks.
constexpr std::int32_t kUnhappyTicks = 40;
/// parity: `VillagerData.NEXT_LEVEL_XP_THRESHOLDS`.
constexpr std::array<std::int32_t, 5> kLevelThresholds{0, 10, 70, 150, 250};
constexpr std::int32_t kLastLevel = 5;
/// parity: `Villager.rewardTradeXp`.
constexpr std::int32_t kPromotionRegeneration = 200;
constexpr std::int32_t kPromotionOrbBonus = 5;
/// parity: `ambientSoundTime > -getAmbientSoundInterval() + 20`.
constexpr std::int32_t kTradeSoundGap = 20;
/// parity: `Mob.getAmbientSoundInterval`, which neither trader overrides.
constexpr std::int32_t kAmbientInterval = 80;

void setUnhappy(ServerEntity& villager, VillagerWorld& world) {
    stateOf(villager).unhappyCounter = kUnhappyTicks;
    world.unhappyChanged(villager, kUnhappyTicks);
    world.sound(villager, "entity.villager.no", 1.0F, 1.0F);
}

/// parity: `Villager.resetSpecialPrices`.
void resetSpecialPrices(ServerEntity& villager) {
    if (villager.offers == nullptr) {
        return;
    }
    for (world::trades::Offer& offer : villager.offers->list) {
        offer.specialPrice = 0;
    }
}

/// parity: `Villager.updateSpecialPrices`.
void updateSpecialPrices(ServerEntity& villager, std::vector<world::trades::Offer>& offers, VillagerWorld& world,
                         std::int32_t playerId, const std::string& playerUuid) {
    resetSpecialPrices(villager);
    const std::int32_t reputation = reputationOf(villager, playerUuid);
    const std::optional<std::int32_t> hero = world.heroLevel(playerId);
    const double heroModifier =
        hero.has_value() ? static_cast<double>(0.3F + (0.0625F * static_cast<float>(*hero))) : 0.0;
    for (world::trades::Offer& offer : offers) {
        if (reputation != 0) {
            offer.specialPrice -=
                static_cast<std::int32_t>(std::floor(static_cast<float>(reputation) * offer.priceMultiplier));
        }
        if (heroModifier > 0.0) {
            const auto reduction = static_cast<std::int32_t>(std::floor(heroModifier * offer.costA.count));
            offer.specialPrice -= std::max(reduction, 1);
        }
    }
}

[[nodiscard]] bool shouldIncreaseLevel(const ServerEntity& villager) {
    const std::int32_t level = villager.villagerLevel;
    return level >= 1 && level < kLastLevel &&
           villager.villagerExperience >= kLevelThresholds[static_cast<std::size_t>(level)];
}

/// parity: `Villager.increaseMerchantCareer`.
void increaseMerchantCareer(ServerEntity& villager, VillagerWorld& world) {
    villager.villagerLevel++;
    if (!villager.sourceNbt.has_value()) {
        villager.sourceNbt.emplace();
    }
    wire::nbt::NbtCompound data;
    if (const wire::nbt::NbtCompound* old = villager.sourceNbt->getCompound("VillagerData"); old != nullptr) {
        data = *old;
    }
    data.putInt("level", villager.villagerLevel);
    villager.sourceNbt->set("VillagerData", wire::nbt::NbtTag::ofCompound(std::move(data)));
    world.villagerDataChanged(villager);
    static_cast<void>(world.offers(villager));
}

} // namespace

bool startTrading(ServerEntity& villager, VillagerWorld& world, std::int32_t playerId, const std::string& playerUuid,
                  bool mainHand, bool* talked) {
    VillagerState& state = stateOf(villager);
    if (state.tradingPlayer.has_value() || villager.sleepingAt.has_value()) {
        return false;
    }
    if (look::isBaby(villager)) {
        setUnhappy(villager, world);
        return false;
    }
    std::vector<world::trades::Offer>* offers = world.offers(villager);
    const bool noOffers = offers == nullptr || offers->empty();
    if (mainHand && noOffers) {
        setUnhappy(villager, world);
    }
    // parity: `TALKED_TO_VILLAGER`.
    if (talked != nullptr) {
        *talked = mainHand;
    }
    if (noOffers) {
        return false;
    }
    updateSpecialPrices(villager, *offers, world, playerId, playerUuid);
    state.tradingPlayer = playerId;
    return true;
}

void tradeUpdated(ServerEntity& villager, VillagerWorld& world, bool validTrade, std::int64_t gameTime) {
    static_cast<void>(gameTime);
    if (villager.ambientSoundTime <= -kAmbientInterval + kTradeSoundGap) {
        return;
    }
    villager.ambientSoundTime = -kAmbientInterval;
    world.sound(villager, validTrade ? "entity.villager.yes" : "entity.villager.no", 1.0F, 1.0F);
}

void noteTrade(ServerEntity& villager, std::int64_t /*gameTime*/) { villager.ambientSoundTime = -kAmbientInterval; }

void stopTrading(ServerEntity& villager) {
    stateOf(villager).tradingPlayer.reset();
    resetSpecialPrices(villager);
}

std::int32_t rewardTradeXp(ServerEntity& villager, VillagerWorld& world, std::int32_t experience,
                           random::RandomSource& random) {
    VillagerState& state = stateOf(villager);
    std::int32_t popXp = world::trades::kTradeOrbBase + random.nextInt(4);
    villager.villagerExperience += experience;
    state.lastTradedPlayer = state.tradingPlayer;
    if (shouldIncreaseLevel(villager)) {
        increaseMerchantCareer(villager, world);
        world.addEffect(villager, "regeneration", kPromotionRegeneration, 0);
        popXp += kPromotionOrbBonus;
    }
    return popXp;
}

} // namespace basalt::server::runtime::entity::villager::villagerBrain
