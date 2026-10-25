#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::entity {
namespace {

using wire::common::Identifier;

[[nodiscard]] bool holds(std::span<const std::string_view> names, std::string_view value) {
    return std::ranges::find(names, value) != names.end();
}

} // namespace

bool mobs::isMob(const Identifier& type) {
    // parity: `MobCategory.MISC`.
    return registry::entityTypeShapes::categoryOf(type) != "misc" ||
           registry::entityAttributes::baseValue(type, "follow_range").has_value();
}

std::string_view mobs::categoryOf(const Identifier& type) { return registry::entityTypeShapes::categoryOf(type); }

bool cubeMobs::isCube(std::string_view typePath) {
    static constexpr std::array<std::string_view, 3> kCubes{"slime", "magma_cube", "sulfur_cube"};
    return holds(kCubes, typePath);
}

bool cubeMobs::isCube(const Identifier& type) { return isCube(type.path()); }

void cubeMobs::applySize(ServerEntity& entity) { applySize(entity, entity.cubeSize); }

void cubeMobs::applySize(ServerEntity& entity, std::int32_t size) {
    if (!isCube(entity.type())) {
        return;
    }
    static constexpr double kBaseSpeed = 0.2;
    static constexpr double kSpeedPerSize = 0.1;
    const std::int32_t actual = std::clamp(size, kMinSize, kMaxSize);
    entity.cubeSize = actual;
    entity.attributeValues[std::string{registry::entityAttributes::kMovementSpeed}] =
        kBaseSpeed + kSpeedPerSize * actual;
    entity.attributeValues[std::string{registry::entityAttributes::kMaxHealth}] = static_cast<double>(actual) * actual;
    entity.attributeValues[std::string{registry::entityAttributes::kAttackDamage}] = actual;
    entity.health = static_cast<double>(actual) * actual;
}

std::vector<cubeMobs::Split> cubeMobs::splitsOf(const ServerEntity& cube, random::RandomSource& random) {
    std::vector<Split> splits;
    if (!isCube(cube.type()) || cube.cubeSize <= kMinSize) {
        return splits;
    }
    // parity: `AbstractCubeMob.remove`.
    const auto offset = static_cast<float>(cube.width() * cube.cubeSize) / 2.0F;
    const std::int32_t half = cube.cubeSize / 2;
    const std::int32_t count = 2 + random.nextInt(3);
    splits.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 0; index < count; ++index) {
        const float dx = (static_cast<float>(index % 2) - 0.5F) * offset;
        const float dz = (static_cast<float>(index / 2) - 0.5F) * offset;
        splits.push_back(Split{.x = cube.x + static_cast<double>(dx),
                               .y = cube.y + 0.5,
                               .z = cube.z + static_cast<double>(dz),
                               .yaw = random.nextFloat() * 360.0F,
                               .size = half});
    }
    return splits;
}

bool ageableMobs::isAgeable(std::string_view typePath) {
    static constexpr std::array<std::string_view, 42> kTypes{"armadillo",  "axolotl",      "bee",
                                                             "camel",      "camel_husk",   "cat",
                                                             "chicken",    "cow",          "dolphin",
                                                             "donkey",     "fox",          "frog",
                                                             "glow_squid", "goat",         "happy_ghast",
                                                             "hoglin",     "horse",        "llama",
                                                             "magma_cube", "mooshroom",    "mule",
                                                             "nautilus",   "ocelot",       "panda",
                                                             "parrot",     "pig",          "polar_bear",
                                                             "rabbit",     "sheep",        "skeleton_horse",
                                                             "slime",      "sniffer",      "squid",
                                                             "strider",    "sulfur_cube",  "trader_llama",
                                                             "turtle",     "villager",     "wandering_trader",
                                                             "wolf",       "zombie_horse", "zombie_nautilus"};
    return holds(kTypes, typePath);
}

bool ageableMobs::isAgeable(const Identifier& type) { return isAgeable(type.path()); }

bool ageableMobs::usesBooleanBabyFlag(std::string_view typePath) {
    // parity: the zombie family, whose baby state is a flag rather than an age.
    static constexpr std::array<std::string_view, 6> kBooleanBabies{
        "drowned", "husk", "piglin", "zombie", "zombie_villager", "zombified_piglin"};
    return holds(kBooleanBabies, typePath);
}

bool ageableMobs::hasBabyMetadata(std::string_view typePath) {
    return isAgeable(typePath) || usesBooleanBabyFlag(typePath);
}

namespace {

/// parity: what each tamable animal is tamed with.
[[nodiscard]] std::span<const std::string_view> tamingFood(std::string_view typePath) {
    static constexpr std::array<std::string_view, 1> kWolf{"bone"};
    static constexpr std::array<std::string_view, 2> kCat{"cod", "salmon"};
    static constexpr std::array<std::string_view, 5> kParrot{"wheat_seeds", "melon_seeds", "pumpkin_seeds",
                                                             "beetroot_seeds", "torchflower_seeds"};
    if (typePath == "wolf") {
        return kWolf;
    }
    if (typePath == "cat" || typePath == "ocelot") {
        return kCat;
    }
    if (typePath == "parrot") {
        return kParrot;
    }
    return {};
}

} // namespace

bool taming::isTamable(const Identifier& type) { return !tamingFood(type.path()).empty(); }

bool taming::tamedBy(const Identifier& type, const Identifier& item) {
    return holds(tamingFood(type.path()), item.path());
}

std::int32_t taming::flagsOf(const ServerEntity& entity) {
    std::int32_t flags = 0;
    if (entity.sitting || entity.sittingPose) {
        flags |= kFlagSitting;
    }
    if (entity.owner.has_value()) {
        flags |= kFlagTamed;
    }
    return flags;
}

namespace {

[[nodiscard]] bool hasBodyItem(const wire::nbt::NbtCompound* data) {
    if (data == nullptr) {
        return false;
    }
    const wire::nbt::NbtCompound* equipment = data->getCompound("equipment");
    if (equipment == nullptr) {
        return false;
    }
    const wire::nbt::NbtCompound* body = equipment->getCompound("body");
    if (body == nullptr) {
        return false;
    }
    return !body->getString("id").empty() && body->getInt("count") > 0;
}

} // namespace

bool mobPersistence::keeps(const ServerEntity& entity) {
    return entity.persistent || requiresCustomPersistence(entity);
}

bool mobPersistence::requiresCustomPersistence(const ServerEntity& entity) {
    // Base Mob: a rider and a leashed mob cannot vanish out from under their relationship.
    if (entity.vehicleId.has_value()) {
        return true;
    }
    if (entity.leashHolderId.has_value() || entity.leashHolderUuid.has_value()) {
        return true;
    }
    const wire::nbt::NbtCompound* data = entity.sourceNbt.has_value() ? &*entity.sourceNbt : nullptr;
    if (data != nullptr && (data->contains("leash") || data->contains("Leash"))) {
        return true;
    }

    // Taming calls setPersistenceRequired in vanilla. The owner is the equivalent live state here.
    if (entity.owner.has_value()) {
        return true;
    }
    if (entity.raidId.has_value()) {
        return true;
    }

    const std::string_view path = entity.type().path();
    // AbstractFish (tadpoles included) and Axolotl's exact bucket-persistence key.
    static constexpr std::array<std::string_view, 6> kFromBucket{"axolotl", "cod",     "pufferfish",
                                                                 "salmon",  "tadpole", "tropical_fish"};
    if (holds(kFromBucket, path)) {
        return data != nullptr && data->getBoolean("FromBucket");
    }
    if (path == "enderman") {
        // parity: `BlockState.CODEC`, a bare id for a default state and a compound otherwise.
        const wire::nbt::NbtTag* carried = data == nullptr ? nullptr : data->find("carriedBlockState");
        if (carried == nullptr) {
            return false;
        }
        if (const std::string* bare = carried->get<std::string>(); bare != nullptr) {
            return !bare->empty() && *bare != "minecraft:air";
        }
        const wire::nbt::NbtCompound* compound = carried->get<wire::nbt::NbtCompound>();
        return compound != nullptr && compound->size() != 0;
    }
    if (path == "sulfur_cube") {
        return (data != nullptr && data->getBoolean("from_bucket")) || hasBodyItem(data);
    }
    return false;
}

bool sunBurn::burns(const Identifier& type) {
    // parity: the mobs whose `isSunSensitive` answers true.
    static constexpr std::array<std::string_view, 9> kBurns{
        "zombie", "zombie_villager", "husk", "drowned", "skeleton", "stray", "bogged", "phantom", "giant"};
    return holds(kBurns, type.path());
}

bool sunBurn::shouldBurn(bool sensitive, bool day, std::int32_t light, bool openSky, bool inWater,
                         random::RandomSource& random) {
    if (!day || !openSky || inWater || !sensitive) {
        return false;
    }
    // parity: `getLightLevelDependentMagicValue` (light over fifteen) and the roll against thirty for dim light.
    static constexpr float kMaxLight = 15.0F;
    static constexpr float kHalf = 0.5F;
    static constexpr float kMargin = 0.4F;
    static constexpr float kRoll = 30.0F;
    const float brightness = static_cast<float>(light) / kMaxLight;
    if (brightness <= kHalf) {
        return false;
    }
    return random.nextFloat() * kRoll < (brightness - kMargin) * 2.0F;
}

bool mobDespawn::shouldDespawn(ServerEntity& mob, MobCategory category, const Ranges& ranges,
                               std::optional<double> nearestDistance, random::RandomSource& random) {
    static constexpr double kPatrolHardDespawnSquared = 16384.0;
    static constexpr std::int32_t kCatDespawnGraceTicks = 2400;
    static constexpr std::array<std::string_view, 25> kNeverDespawnAnimals{
        "armadillo", "bee",        "camel",        "cow",         "donkey",
        "fox",       "frog",       "goat",         "happy_ghast", "horse",
        "llama",     "mooshroom",  "mule",         "panda",       "parrot",
        "pig",       "polar_bear", "rabbit",       "sheep",       "skeleton_horse",
        "sniffer",   "strider",    "trader_llama", "turtle",      "wolf"};

    if (mobPersistence::keeps(mob)) {
        mob.idleTicks = 0;
        return false;
    }
    const std::string_view path = mob.type().path();
    if (path == "wandering_trader") {
        return false;
    }
    // parity: `Animal.removeWhenFarAway(false)`.
    if (holds(kNeverDespawnAnimals, path)) {
        return false;
    }
    // parity: `removeWhenFarAway` is false for misc-category mobs.
    if (category == MobCategory::Misc) {
        return false;
    }
    const wire::nbt::NbtCompound* data = mob.sourceNbt.has_value() ? &*mob.sourceNbt : nullptr;
    // Chicken alone opts back into normal distance despawning while carrying a jockey.
    if (path == "chicken" && (data == nullptr || !data->getBoolean("IsChickenJockey"))) {
        return false;
    }
    // Cat.removeWhenFarAway protects every newly spawned stray for its first two minutes.
    if (path == "cat" && mob.tickCount <= kCatDespawnGraceTicks) {
        return false;
    }
    // Ocelot has the same grace, and trusting one changes the answer permanently to false.
    if (path == "ocelot" &&
        (mob.tickCount <= kCatDespawnGraceTicks || (data != nullptr && data->getBoolean("Trusting")))) {
        return false;
    }
    if (!nearestDistance.has_value()) {
        return false;
    }
    const double distanceSquared = *nearestDistance * *nearestDistance;
    const double despawn = ranges.hard * ranges.hard;
    if (distanceSquared > despawn) {
        return true;
    }
    const double keep = ranges.soft * ranges.soft;
    if (distanceSquared < keep) {
        // parity: standing near it resets the clock, so it is never forgotten under your nose.
        mob.idleTicks = 0;
        return false;
    }
    // parity: `PatrollingMonster.removeWhenFarAway`.
    if (mob.patrolling && distanceSquared <= kPatrolHardDespawnSquared) {
        return false;
    }
    return mob.idleTicks > ranges.idleTicks && random.nextInt(ranges.randomOdds) == 0;
}

Identifier wardens::type() { return Identifier::vanilla("warden"); }

bool wardens::isWarden(const Identifier& type) { return type == wardens::type(); }

std::int32_t wardens::anger(ServerEntity& warden, std::int32_t suspectId, std::int32_t amount) {
    // parity: a burrowing or rising warden hears nothing and minds no hit.
    if (wardenBrain::isDiggingOrEmerging(warden) || warden.noAi) {
        return angerAt(warden, suspectId);
    }
    // parity: `increaseAngerAt` keeps a running dig cooldown running.
    wardenBrain::refreshDigCooldown(warden);
    if (!warden.anger.has_value()) {
        warden.anger.emplace();
    }
    std::map<std::int32_t, std::int32_t>& anger = *warden.anger;
    const auto found = anger.find(suspectId);
    const std::int32_t level = std::min((found == anger.end() ? 0 : found->second) + amount, kMaxAnger);
    anger[suspectId] = level;
    return level;
}

void wardens::tick(ServerEntity& warden) {
    if (!warden.anger.has_value()) {
        return;
    }
    std::map<std::int32_t, std::int32_t>& anger = *warden.anger;
    for (auto entry = anger.begin(); entry != anger.end();) {
        if (entry->second <= 1) {
            entry = anger.erase(entry);
        } else {
            --entry->second;
            ++entry;
        }
    }
}

std::optional<std::int32_t> wardens::target(const ServerEntity& warden) {
    if (!warden.anger.has_value()) {
        return std::nullopt;
    }
    std::optional<std::int32_t> best;
    std::int32_t bestAnger = kAngry - 1;
    for (const auto& [suspect, level] : *warden.anger) {
        if (level <= bestAnger) {
            continue;
        }
        bestAnger = level;
        best = suspect;
    }
    return best;
}

std::int32_t wardens::angerAt(const ServerEntity& warden, std::int32_t suspectId) {
    if (!warden.anger.has_value()) {
        return 0;
    }
    const auto found = warden.anger->find(suspectId);
    return found == warden.anger->end() ? 0 : found->second;
}

namespace {

[[nodiscard]] std::optional<Identifier> itemOrNothing(std::string path) {
    Identifier item = Identifier::vanilla(std::move(path));
    return registry::items::registry().byName(item) == nullptr ? std::nullopt : std::optional<Identifier>{item};
}

} // namespace

std::optional<Identifier> entityPickResults::itemFor(const Identifier& type) {
    if (type.namespaceName() != Identifier::kVanillaNamespace) {
        return std::nullopt;
    }
    const std::string path{type.path()};
    // parity: `LeashFenceKnotEntity.getPickResult`.
    if (path == "leash_knot") {
        return itemOrNothing("lead");
    }
    if (std::optional<Identifier> egg = itemOrNothing(path + "_spawn_egg"); egg.has_value()) {
        return egg;
    }
    // parity: the `getPickResult` overrides that are not a spawn egg.
    static constexpr std::array<std::string_view, 12> kPlaced{
        "minecart",        "chest_minecart",   "furnace_minecart",       "tnt_minecart",
        "hopper_minecart", "spawner_minecart", "command_block_minecart", "armor_stand",
        "painting",        "item_frame",       "glow_item_frame",        "end_crystal"};
    if (holds(kPlaced, path) || path.ends_with("_boat") || path.ends_with("_raft")) {
        return itemOrNothing(path);
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::entity
