#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <string_view>

namespace basalt::server::runtime::entity::bees {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

/// parity: `BeehiveBlockEntity.IGNORED_BEE_TAGS`, in its own order.
constexpr std::array<std::string_view, 25> kIgnoredInHive{"Air",
                                                          "drop_chances",
                                                          "equipment",
                                                          "Brain",
                                                          "CanPickUpLoot",
                                                          "DeathTime",
                                                          "fall_distance",
                                                          "FallFlying",
                                                          "Fire",
                                                          "HurtTime",
                                                          "LeftHanded",
                                                          "Motion",
                                                          "NoGravity",
                                                          "OnGround",
                                                          "PortalCooldown",
                                                          "Pos",
                                                          "Rotation",
                                                          "sleeping_pos",
                                                          "CannotEnterHiveTicks",
                                                          "TicksSincePollination",
                                                          "CropsGrownSincePollination",
                                                          "hive_pos",
                                                          "Passengers",
                                                          "leash",
                                                          "UUID"};

[[nodiscard]] std::optional<Pos> posIn(const NbtCompound& tag, std::string_view key) {
    const std::span<const std::int32_t> parts = tag.getIntArray(key);
    return parts.size() == 3 ? std::optional{Pos{parts[0], parts[1], parts[2]}} : std::nullopt;
}

void putPos(NbtCompound& tag, std::string_view key, const std::optional<Pos>& pos) {
    if (pos.has_value()) {
        tag.putIntArray(key, {(*pos)[0], (*pos)[1], (*pos)[2]});
    } else {
        tag.remove(key);
    }
}

/// parity: `UUIDUtil.CODEC`, four ints high word first.
[[nodiscard]] NbtTag uuidTag(const wire::common::Uuid& uuid) {
    return NbtTag::ofIntArray({static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high() >> 32)),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high())),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low() >> 32)),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low()))});
}

[[nodiscard]] std::optional<wire::common::Uuid> uuidIn(const NbtCompound& tag, std::string_view key) {
    const std::span<const std::int32_t> parts = tag.getIntArray(key);
    if (parts.size() != 4) {
        return std::nullopt;
    }
    const auto word = [parts](std::size_t at) {
        return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
    };
    return wire::common::Uuid{(word(0) << 32) | word(1), (word(2) << 32) | word(3)};
}

} // namespace

bool isBee(const wire::common::Identifier& type) { return type == wire::common::Identifier::vanilla("bee"); }

namespace {
void readSaved(BeeState& state, const NbtCompound& tag) {
    state.hasNectar = tag.getBoolean("HasNectar");
    state.hasStung = tag.getBoolean("HasStung");
    state.ticksWithoutNectar = tag.getInt("TicksSincePollination");
    state.stayOutOfHive = tag.getInt("CannotEnterHiveTicks");
    state.cropsGrown = tag.getInt("CropsGrownSincePollination");
    state.hive = posIn(tag, "hive_pos");
    state.flower = posIn(tag, "flower_pos");
    // parity: `readPersistentAngerSaveData`, with the old `AngerTime` read as ticks from now.
    if (tag.contains("anger_end_time")) {
        state.angerEndTime = tag.getLong("anger_end_time");
    } else if (tag.contains("AngerTime")) {
        state.legacyAngerTime = tag.getInt("AngerTime");
    }
    state.angryAt = uuidIn(tag, "angry_at");
}
} // namespace

BeeState& stateOf(ServerEntity& bee, random::RandomSource& random) {
    if (bee.bee != nullptr) {
        return *bee.bee;
    }
    bee.bee = std::make_shared<BeeState>();
    BeeState& state = *bee.bee;
    // parity: the constructor's `Mth.nextInt(random, 20, 60)`.
    state.flowerCooldown = kFlowerRetryMin + random.nextInt(kFlowerRetryMax - kFlowerRetryMin + 1);
    if (bee.sourceNbt.has_value()) {
        readSaved(state, *bee.sourceNbt);
    }
    return state;
}

void reload(ServerEntity& bee) {
    if (bee.bee != nullptr && bee.sourceNbt.has_value()) {
        readSaved(*bee.bee, *bee.sourceNbt);
    }
}

void write(const ServerEntity& bee, NbtCompound& tag) {
    if (bee.bee == nullptr) {
        return;
    }
    const BeeState& state = *bee.bee;
    putPos(tag, "hive_pos", state.hive);
    putPos(tag, "flower_pos", state.flower);
    tag.putBoolean("HasNectar", state.hasNectar);
    tag.putBoolean("HasStung", state.hasStung);
    tag.putInt("TicksSincePollination", state.ticksWithoutNectar);
    tag.putInt("CannotEnterHiveTicks", state.stayOutOfHive);
    tag.putInt("CropsGrownSincePollination", state.cropsGrown);
    tag.remove("AngerTime");
    tag.putLong("anger_end_time", state.angerEndTime);
    if (state.angryAt.has_value()) {
        tag.set("angry_at", uuidTag(*state.angryAt));
    } else {
        tag.remove("angry_at");
    }
}

std::int8_t flagsOf(const BeeState& state) {
    return static_cast<std::int8_t>((state.rolling ? kFlagRoll : 0) | (state.hasStung ? kFlagHasStung : 0) |
                                    (state.hasNectar ? kFlagHasNectar : 0));
}

bool isAngry(const BeeState& state, std::int64_t gameTime) {
    return state.angerEndTime > 0 && state.angerEndTime - gameTime > 0;
}

void startAngerTimer(BeeState& state, std::int64_t gameTime, random::RandomSource& random) {
    state.angerEndTime = gameTime + kAngerMinTicks + random.nextInt(kAngerMaxTicks - kAngerMinTicks + 1);
}

void stopBeingAngry(ServerEntity& bee, BeeState& state) {
    bee.hurtBy.reset();
    state.angryAt.reset();
    bee.targetId.reset();
    state.angerEndTime = -1;
}

void setHasNectar(BeeState& state, bool hasNectar) {
    if (hasNectar) {
        state.ticksWithoutNectar = 0;
    }
    state.hasNectar = hasNectar;
}

void dropHive(BeeState& state) {
    state.hive.reset();
    state.hiveCooldown = kHiveCooldown;
}

void dropFlower(BeeState& state, random::RandomSource& random) {
    state.flower.reset();
    state.flowerCooldown = kFlowerRetryMin + random.nextInt(kFlowerRetryMax - kFlowerRetryMin + 1);
}

bool closerThan(const ServerEntity& bee, const Pos& target, double distance) {
    const auto dx = static_cast<double>(target[0] - static_cast<std::int32_t>(std::floor(bee.x)));
    const auto dy = static_cast<double>(target[1] - static_cast<std::int32_t>(std::floor(bee.y)));
    const auto dz = static_cast<double>(target[2] - static_cast<std::int32_t>(std::floor(bee.z)));
    return (dx * dx) + (dy * dy) + (dz * dz) < distance * distance;
}

void stripHiveTags(NbtCompound& tag) {
    for (const std::string_view key : kIgnoredInHive) {
        tag.remove(key);
    }
}

} // namespace basalt::server::runtime::entity::bees
