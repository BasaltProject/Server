#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string_view>

namespace basalt::server::runtime::entity::neutralAnger {

Anger& stateOf(ServerEntity& mob) {
    if (mob.neutral != nullptr) {
        return *mob.neutral;
    }
    mob.neutral = std::make_shared<Anger>();
    Anger& anger = *mob.neutral;
    if (!mob.sourceNbt.has_value()) {
        return anger;
    }
    const wire::nbt::NbtCompound& tag = *mob.sourceNbt;
    if (tag.contains("anger_end_time")) {
        anger.endTime = tag.getLong("anger_end_time");
    } else if (tag.contains("AngerTime")) {
        anger.legacyTime = tag.getInt("AngerTime");
    }
    if (const std::span<const std::int32_t> parts = tag.getIntArray("angry_at"); parts.size() == 4) {
        const auto word = [parts](std::size_t at) {
            return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
        };
        anger.target = wire::common::Uuid{(word(0) << 32) | word(1), (word(2) << 32) | word(3)};
    }
    return anger;
}

void write(const ServerEntity& mob, wire::nbt::NbtCompound& tag) {
    if (mob.neutral == nullptr) {
        return;
    }
    const Anger& anger = *mob.neutral;
    tag.remove("AngerTime");
    tag.putLong("anger_end_time", anger.endTime);
    if (anger.target.has_value()) {
        const wire::common::Uuid& uuid = *anger.target;
        tag.putIntArray("angry_at", {static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high() >> 32)),
                                     static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high())),
                                     static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low() >> 32)),
                                     static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low()))});
    } else {
        tag.remove("angry_at");
    }
}

bool isAngry(const Anger& anger, std::int64_t gameTime) { return anger.endTime > 0 && anger.endTime - gameTime > 0; }

void start(Anger& anger, std::int64_t gameTime, random::RandomSource& random, std::int32_t minTicks,
           std::int32_t maxTicks) {
    anger.endTime = gameTime + minTicks + random.nextInt(maxTicks - minTicks + 1);
}

void stop(ServerEntity& mob, Anger& anger) {
    mob.hurtBy.reset();
    mob.lastHurtByMob.reset();
    mob.lastHurtByMobTimestamp = mob.tickCount;
    anger.target.reset();
    mob.targetId.reset();
    anger.endTime = -1;
}

bool isNeutral(const wire::common::Identifier& type) {
    static constexpr std::array<std::string_view, 6> kNeutral{"bee",        "wolf",     "iron_golem",
                                                              "polar_bear", "enderman", "zombified_piglin"};
    return type.namespaceName() == "minecraft" && std::ranges::find(kNeutral, type.path()) != kNeutral.end();
}

Access accessOf(ServerEntity& mob) {
    if (!isNeutral(mob.type())) {
        return {};
    }
    if (bees::isBee(mob.type())) {
        return mob.bee == nullptr ? Access{} : Access{.endTime = &mob.bee->angerEndTime, .target = &mob.bee->angryAt};
    }
    Anger& anger = stateOf(mob);
    return Access{.endTime = &anger.endTime, .target = &anger.target};
}

namespace {

[[nodiscard]] bool angry(const Access& access, std::int64_t gameTime) {
    return access.endTime != nullptr && *access.endTime > 0 && *access.endTime - gameTime > 0;
}

} // namespace

bool isAngryAtAllPlayers(ServerEntity& mob, std::int64_t gameTime, bool universalAnger) {
    const Access access = accessOf(mob);
    return universalAnger && angry(access, gameTime) && !access.target->has_value();
}

bool isAngryAtPlayer(ServerEntity& mob, const wire::common::Uuid& player, bool valid, std::int64_t gameTime,
                     bool universalAnger) {
    if (!valid) {
        return false;
    }
    if (isAngryAtAllPlayers(mob, gameTime, universalAnger)) {
        return true;
    }
    const Access access = accessOf(mob);
    return access.target != nullptr && *access.target == player;
}

void stopBeingAngry(ServerEntity& mob) {
    if (mob.bee != nullptr) {
        bees::stopBeingAngry(mob, *mob.bee);
        mob.lastHurtByMob.reset();
        mob.lastHurtByMobTimestamp = mob.tickCount;
        return;
    }
    stop(mob, stateOf(mob));
}

void refreshUniversalAnger(ServerEntity& mob, std::int64_t gameTime, random::RandomSource& random) {
    stopBeingAngry(mob);
    if (mob.bee != nullptr) {
        bees::startAngerTimer(*mob.bee, gameTime, random);
        return;
    }
    start(stateOf(mob), gameTime, random, kAngerMinTicks, kAngerMaxTicks);
}

void playerDied(ServerEntity& mob, const wire::common::Uuid& player) {
    const Access access = accessOf(mob);
    if (access.target != nullptr && *access.target == player) {
        stopBeingAngry(mob);
    }
}

} // namespace basalt::server::runtime::entity::neutralAnger
