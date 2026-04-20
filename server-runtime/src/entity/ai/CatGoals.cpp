#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/CatGoals.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/SpecialGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr std::int32_t kGiveUpTicks = 1200;
constexpr std::int32_t kStayTicks = 1200;
constexpr std::int32_t kIntervalTicks = 200;
constexpr std::int32_t kRepathInterval = 40;
constexpr double kAcceptedDistance = 1.0;
constexpr std::int32_t kLieStartTicks = 40;
constexpr std::int32_t kLieVerticalRange = 6;
constexpr std::int32_t kSitSearchRange = 8;

[[nodiscard]] std::optional<registry::BlockState> stateAt(const MobContext& mob, std::int32_t x, std::int32_t y,
                                                          std::int32_t z) {
    const std::optional<std::int32_t> id = mob.world.blockState(x, y, z);
    return id.has_value() ? registry::blocks::byStateId(*id) : std::nullopt;
}

/// parity: `LevelReader.isEmptyBlock`, which is `BlockState.isAir`.
[[nodiscard]] bool isEmpty(const MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<registry::BlockState> state = stateAt(mob, x, y, z);
    if (!state.has_value()) {
        return false;
    }
    const std::string_view path = state->block.path();
    return path == "air" || path == "cave_air" || path == "void_air";
}

[[nodiscard]] bool tagged(MobContext& mob, std::string_view tag, const registry::BlockState& state) {
    return mob.actions.blockTagged(tag, state.block).value_or(false);
}

/// parity: `Mob.isWithinHome`, where no radius means everywhere.
[[nodiscard]] bool withinHome(const ServerEntity& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (!mob.homePosition.has_value() || mob.homeRadius <= 0) {
        return true;
    }
    const auto dx = static_cast<double>(x - (*mob.homePosition)[0]);
    const auto dy = static_cast<double>(y - (*mob.homePosition)[1]);
    const auto dz = static_cast<double>(z - (*mob.homePosition)[2]);
    const auto radius = static_cast<double>(mob.homeRadius);
    return (dx * dx) + (dy * dy) + (dz * dz) < radius * radius;
}

void setSittingPose(MobContext& mob, bool value) {
    if (mob.mob.sittingPose != value) {
        mob.mob.sittingPose = value;
        mob.actions.poseChanged(mob.mob);
    }
}

void setLying(MobContext& mob, bool value) {
    if (mob.mob.lying != value) {
        mob.mob.lying = value;
        mob.actions.poseChanged(mob.mob);
    }
}

[[nodiscard]] bool tame(const MobContext& mob) { return mob.mob.owner.has_value(); }

void setRelaxOne(MobContext& mob, bool value) {
    if (mob.mob.relaxOne != value) {
        mob.mob.relaxOne = value;
        mob.actions.poseChanged(mob.mob);
    }
}

} // namespace

MoveToBlockGoal::MoveToBlockGoal(double speedModifier, std::int32_t searchRange, std::int32_t verticalSearchRange)
    : speedModifier_(speedModifier), searchRange_(searchRange), verticalSearchRange_(verticalSearchRange) {}

bool MoveToBlockGoal::canUse(MobContext& mob) {
    if (nextStartTick_ > 0) {
        --nextStartTick_;
        return false;
    }
    nextStartTick_ = nextStartTick(mob);
    return findNearestBlock(mob);
}

std::int32_t MoveToBlockGoal::nextStartTick(MobContext& mob) {
    return reducedTickDelay(kIntervalTicks + mob.random.nextInt(kIntervalTicks));
}

bool MoveToBlockGoal::canContinueToUse(MobContext& mob) {
    return tryTicks_ >= -maxStayTicks_ && tryTicks_ <= kGiveUpTicks &&
           isValidTarget(mob, blockPos_[0], blockPos_[1], blockPos_[2]);
}

void MoveToBlockGoal::start(MobContext& mob) {
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, static_cast<double>(blockPos_[0]) + 0.5,
                                         static_cast<double>(blockPos_[1] + 1), static_cast<double>(blockPos_[2]) + 0.5,
                                         speedModifier_));
    tryTicks_ = 0;
    const std::int32_t bound = mob.random.nextInt(kStayTicks) + kStayTicks;
    maxStayTicks_ = mob.random.nextInt(bound) + kStayTicks;
}

void MoveToBlockGoal::tick(MobContext& mob) {
    // parity: `Vec3i.closerToCenterThan`.
    const double x = static_cast<double>(blockPos_[0]) + 0.5;
    const double y = static_cast<double>(blockPos_[1] + 1) + 0.5;
    const double z = static_cast<double>(blockPos_[2]) + 0.5;
    if (mob.distanceSquaredTo(x, y, z) >= kAcceptedDistance * kAcceptedDistance) {
        reachedTarget_ = false;
        ++tryTicks_;
        if (tryTicks_ % kRepathInterval == 0) {
            static_cast<void>(
                navigation::moveTo(mob.world, mob.mob, x, static_cast<double>(blockPos_[1] + 1), z, speedModifier_));
        }
    } else {
        reachedTarget_ = true;
        --tryTicks_;
    }
}

bool MoveToBlockGoal::findNearestBlock(MobContext& mob) {
    const auto originX = static_cast<std::int32_t>(std::floor(mob.mob.x));
    const auto originY = static_cast<std::int32_t>(std::floor(mob.mob.y));
    const auto originZ = static_cast<std::int32_t>(std::floor(mob.mob.z));
    // parity: the search order is outwards in rings, alternating sides, which decides the tie.
    for (std::int32_t y = verticalSearchStart_; y <= verticalSearchRange_; y = y > 0 ? -y : 1 - y) {
        for (std::int32_t r = 0; r < searchRange_; ++r) {
            for (std::int32_t x = 0; x <= r; x = x > 0 ? -x : 1 - x) {
                for (std::int32_t z = x < r && x > -r ? r : 0; z <= r; z = z > 0 ? -z : 1 - z) {
                    const std::int32_t atX = originX + x;
                    const std::int32_t atY = originY + y - 1;
                    const std::int32_t atZ = originZ + z;
                    if (withinHome(mob.mob, atX, atY, atZ) && isValidTarget(mob, atX, atY, atZ)) {
                        blockPos_ = {atX, atY, atZ};
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

CatLieOnBlockGoal::CatLieOnBlockGoal(double speedModifier, std::int32_t searchRange)
    : MoveToBlockGoal(speedModifier, searchRange, kLieVerticalRange) {
    verticalSearchStart_ = -2;
}

bool CatLieOnBlockGoal::canUse(MobContext& mob) {
    return tame(mob) && !mob.mob.sitting && !mob.mob.lying && MoveToBlockGoal::canUse(mob);
}

void CatLieOnBlockGoal::start(MobContext& mob) {
    MoveToBlockGoal::start(mob);
    setSittingPose(mob, false);
}

std::int32_t CatLieOnBlockGoal::nextStartTick(MobContext& /*mob*/) { return kLieStartTicks; }

void CatLieOnBlockGoal::stop(MobContext& mob) { setLying(mob, false); }

void CatLieOnBlockGoal::tick(MobContext& mob) {
    MoveToBlockGoal::tick(mob);
    setSittingPose(mob, false);
    setLying(mob, reachedTarget());
}

bool CatLieOnBlockGoal::isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (!isEmpty(mob, x, y + 1, z)) {
        return false;
    }
    const std::optional<registry::BlockState> state = stateAt(mob, x, y, z);
    return state.has_value() && tagged(mob, "minecraft:cats_can_lie_on", *state);
}

CatSitOnBlockGoal::CatSitOnBlockGoal(double speedModifier) : MoveToBlockGoal(speedModifier, kSitSearchRange) {}

bool CatSitOnBlockGoal::canUse(MobContext& mob) {
    return tame(mob) && !mob.mob.sitting && MoveToBlockGoal::canUse(mob);
}

void CatSitOnBlockGoal::start(MobContext& mob) {
    MoveToBlockGoal::start(mob);
    setSittingPose(mob, false);
}

void CatSitOnBlockGoal::stop(MobContext& mob) { setSittingPose(mob, false); }

void CatSitOnBlockGoal::tick(MobContext& mob) {
    MoveToBlockGoal::tick(mob);
    setSittingPose(mob, reachedTarget());
}

bool CatSitOnBlockGoal::isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (!isEmpty(mob, x, y + 1, z)) {
        return false;
    }
    const std::optional<registry::BlockState> state = stateAt(mob, x, y, z);
    if (!state.has_value() || !tagged(mob, "minecraft:cats_can_sit_on", *state)) {
        return false;
    }
    // parity: the first of chest, furnace and bed that the block is decides, and anything else passes.
    const std::string_view path = state->block.path();
    if (path == "chest") {
        return !mob.actions.chestInUse(mob.mob, x, y, z);
    }
    if (path == "furnace") {
        const auto lit = state->properties.find("lit");
        return lit != state->properties.end() && lit->second == "true";
    }
    if (tagged(mob, "minecraft:beds", *state)) {
        const auto part = state->properties.find("part");
        return part == state->properties.end() || part->second != "head";
    }
    return true;
}

namespace {

/// parity: `EnvironmentAttributes.CAT_WAKING_UP_GIFT_CHANCE`.
[[nodiscard]] float giftChance(const MobContext& mob) {
    constexpr std::int64_t kDay = 24000;
    constexpr std::int64_t kFrom = 23667;
    constexpr std::int64_t kUntil = 362;
    if (!mob.hasSkylight) {
        return 0.0F;
    }
    const std::int64_t time = ((mob.dayTime % kDay) + kDay) % kDay;
    return time >= kFrom || time < kUntil ? 0.7F : 0.0F;
}

[[nodiscard]] const PlayerView* ownerOf(const MobContext& mob) {
    if (!mob.mob.owner.has_value()) {
        return nullptr;
    }
    for (const PlayerView& player : mob.players) {
        if (player.uuid == *mob.mob.owner) {
            return &player;
        }
    }
    return nullptr;
}

} // namespace

bool CatRelaxOnOwnerGoal::canUse(MobContext& mob) {
    if (!tame(mob) || mob.mob.sitting) {
        return false;
    }
    const PlayerView* owner = ownerOf(mob);
    if (owner == nullptr) {
        return false;
    }
    owner_ = owner->entityId;
    if (!owner->sleeping || mob.distanceSquaredTo(owner->x, owner->y, owner->z) > 100.0) {
        return false;
    }
    const std::array<std::int32_t, 3> at{static_cast<std::int32_t>(std::floor(owner->x)),
                                         static_cast<std::int32_t>(std::floor(owner->y)),
                                         static_cast<std::int32_t>(std::floor(owner->z))};
    const std::optional<registry::BlockState> state = stateAt(mob, at[0], at[1], at[2]);
    if (!state.has_value() || !tagged(mob, "minecraft:cats_can_lie_on", *state)) {
        return false;
    }
    // parity: the foot of the bed, one step back from where it faces, or the square itself.
    goalPos_ = at;
    if (const auto facing = state->properties.find("facing"); facing != state->properties.end()) {
        const std::string_view face = facing->second;
        (*goalPos_)[0] += face == "east" ? -1 : (face == "west" ? 1 : 0);
        (*goalPos_)[2] += face == "south" ? -1 : (face == "north" ? 1 : 0);
    }
    return !spaceIsOccupied(mob);
}

bool CatRelaxOnOwnerGoal::spaceIsOccupied(MobContext& mob) const {
    if (!goalPos_.has_value()) {
        return false;
    }
    for (const ServerEntity* other : mob.entities) {
        if (other == &mob.mob || other->removed || other->type().path() != "cat" ||
            (!other->lying && !other->relaxOne)) {
            continue;
        }
        const double half = other->width() / 2.0;
        const bool touches = other->x + half > (*goalPos_)[0] - 2.0 && other->x - half < (*goalPos_)[0] + 3.0 &&
                             other->y + other->height() > (*goalPos_)[1] - 2.0 && other->y < (*goalPos_)[1] + 3.0 &&
                             other->z + half > (*goalPos_)[2] - 2.0 && other->z - half < (*goalPos_)[2] + 3.0;
        if (touches) {
            return true;
        }
    }
    return false;
}

bool CatRelaxOnOwnerGoal::canContinueToUse(MobContext& mob) {
    const PlayerView* owner = owner_.has_value() ? mob.playerById(*owner_) : nullptr;
    return tame(mob) && !mob.mob.sitting && owner != nullptr && owner->sleeping && goalPos_.has_value() &&
           !spaceIsOccupied(mob);
}

void CatRelaxOnOwnerGoal::start(MobContext& mob) {
    if (goalPos_.has_value()) {
        setSittingPose(mob, false);
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, (*goalPos_)[0], (*goalPos_)[1], (*goalPos_)[2],
                                             static_cast<double>(1.1F)));
    }
}

void CatRelaxOnOwnerGoal::stop(MobContext& mob) {
    setLying(mob, false);
    const PlayerView* owner = owner_.has_value() ? mob.playerById(*owner_) : nullptr;
    if (owner != nullptr && owner->sleepTicks >= 100 && mob.random.nextFloat() < giftChance(mob)) {
        // parity: `giveMorningGift`: a hop within five blocks, then the gift where it lands.
        std::array<double, 3> from{std::floor(mob.mob.x), std::floor(mob.mob.y), std::floor(mob.mob.z)};
        if (mob.mob.leashHolderId.has_value()) {
            if (const ServerEntity* holder = mob.entityById(*mob.mob.leashHolderId); holder != nullptr) {
                from = {std::floor(holder->x), std::floor(holder->y), std::floor(holder->z)};
            }
        }
        const double x = from[0] + mob.random.nextInt(11) - 5;
        const double y = from[1] + mob.random.nextInt(5) - 2;
        const double z = from[2] + mob.random.nextInt(11) - 5;
        if (const std::optional<double> landing = landingFor(mob, x, y, z, "minecraft:cat_does_not_teleport_to");
            landing.has_value() && !mob.mob.vehicleId.has_value()) {
            mob.mob.x = x;
            mob.mob.y = *landing;
            mob.mob.z = z;
            mob.mob.velocityX = 0.0;
            mob.mob.velocityY = 0.0;
            mob.mob.velocityZ = 0.0;
        }
        mob.actions.dropGift(mob.mob, "gameplay/cat_morning_gift");
    }
    onBedTicks_ = 0;
    setRelaxOne(mob, false);
    navigation::stop(mob.mob);
}

void CatRelaxOnOwnerGoal::tick(MobContext& mob) {
    const PlayerView* owner = owner_.has_value() ? mob.playerById(*owner_) : nullptr;
    if (owner == nullptr || !goalPos_.has_value()) {
        return;
    }
    setSittingPose(mob, false);
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, (*goalPos_)[0], (*goalPos_)[1], (*goalPos_)[2],
                                         static_cast<double>(1.1F)));
    if (mob.distanceSquaredTo(owner->x, owner->y, owner->z) < 2.5) {
        ++onBedTicks_;
        if (onBedTicks_ > adjustedTickDelay(16)) {
            setLying(mob, true);
            setRelaxOne(mob, false);
        } else {
            lookControl::lookAt(mob.mob, owner->x, owner->y + owner->height * 0.9, owner->z, 45.0F, 45.0F);
            setRelaxOne(mob, true);
        }
    } else {
        setLying(mob, false);
    }
}

} // namespace basalt::server::runtime::entity::ai
