#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The jar as the two classes here read it: a directory listing and a file reader.
///
/// Callbacks rather than `ServerJarAssets`, for the same reason every other registry here takes
/// them: this needs the operator's own copy of the game, and a test has none to give.
struct SpawnEquipmentJar {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

/// Exact 26.2 `mob_spawn_equipment` enchantment-provider evaluator.
///
/// Every draw is made whether or not it changes anything: the enchantment a mob gets is invisible
/// next to the spawn stream it shares with everything else in the chunk, and skipping a draw here
/// moves every roll after it.
class SpawnEquipmentEnchantments {
public:
    explicit SpawnEquipmentEnchantments(const SpawnEquipmentJar& jar,
                                        wire::common::Logger logger = wire::common::Logger{"Spawn equipment"});

    /// Applies the provider to `stack`, consuming every draw even when no enchantment is available.
    void enchant(wire::nbt::NbtCompound& stack, random::RandomSource& random, float specialMultiplier) const;

private:
    /// An item or a tag of them, as `supported_items` and `exclusive_set` are both written.
    struct Selector {
        std::optional<wire::common::Identifier> id;
        std::optional<wire::common::Identifier> tag;

        [[nodiscard]] bool
        matches(const wire::common::Identifier& candidate,
                const std::map<wire::common::Identifier, std::set<wire::common::Identifier>>& tags) const;

        [[nodiscard]] static Selector parse(std::string_view value);
    };

    /// parity: `Enchantment.Cost`, linear in the level.
    struct Cost {
        std::int32_t base = 0;
        std::int32_t perLevel = 0;

        [[nodiscard]] std::int32_t at(std::int32_t level) const { return base + perLevel * (level - 1); }
    };

    struct Definition {
        wire::common::Identifier id;
        Selector primaryItems;
        std::int32_t weight = 0;
        std::int32_t maxLevel = 1;
        Cost minCost;
        Cost maxCost;
        Selector exclusive;
    };

    struct Candidate {
        const Definition* definition = nullptr;
        std::int32_t level = 0;
    };

    [[nodiscard]] std::vector<Candidate> available(const wire::common::Identifier& item, std::int32_t cost) const;

    [[nodiscard]] bool compatible(const Definition& first, const Definition& second) const;

    wire::common::Logger logger_;
    std::map<wire::common::Identifier, Definition> definitions_;
    std::map<wire::common::Identifier, std::set<wire::common::Identifier>> itemTags_;
    std::map<wire::common::Identifier, std::set<wire::common::Identifier>> enchantmentTags_;
};

/// Type-specific spawn equipment and its exact enchantment RNG.
class SpawnEquipmentFinalizer {
public:
    /// Today's date, so the Halloween head is testable rather than a thing that happens once a year.
    /// Answers whether it is 31 October where the server is.
    using IsHalloween = std::function<bool()>;

    explicit SpawnEquipmentFinalizer(const SpawnEquipmentJar& jar, IsHalloween isHalloween = {});

    void finalizeAfterCommon(wire::nbt::NbtCompound& data, std::string_view type, random::RandomSource& random,
                             float specialMultiplier, std::string_view difficulty) const;

    void finalizePiglinEquipmentBeforeCommon(wire::nbt::NbtCompound& data, random::RandomSource& random,
                                             float specialMultiplier, bool baby) const;

    /// parity: `Pillager.finalizeSpawn`, before `PatrollingMonster` and the common mob layer.
    void finalizePillagerEquipmentBeforeCommon(wire::nbt::NbtCompound& data, random::RandomSource& random,
                                               float specialMultiplier) const;

    void finalizeZombieEquipmentAfterCommon(wire::nbt::NbtCompound& data, std::string_view type,
                                            random::RandomSource& random, float specialMultiplier,
                                            std::string_view difficulty) const;

    /// The guaranteed helmet and provider pass applied after a skeleton trap rider's finalizer.
    void finalizeSkeletonTrapEquipment(wire::nbt::NbtCompound& data, random::RandomSource& random,
                                       float specialMultiplier) const;

    [[nodiscard]] std::optional<std::string> itemId(const wire::nbt::NbtCompound& data, std::string_view slot) const;

    void setGuaranteedDrop(wire::nbt::NbtCompound& data, std::string_view slot) const;

    void setItem(wire::nbt::NbtCompound& data, std::string_view slot, std::string_view item) const;

private:
    void finalizeSkeleton(wire::nbt::NbtCompound& data, std::string_view type, random::RandomSource& random,
                          float specialMultiplier, std::string_view difficulty) const;

    void populateArmor(wire::nbt::NbtCompound& data, random::RandomSource& random, float specialMultiplier,
                       std::string_view difficulty) const;

    void enchantSlot(wire::nbt::NbtCompound& data, std::string_view slot, random::RandomSource& random, float chance,
                     float specialMultiplier) const;

    void addHalloweenHead(wire::nbt::NbtCompound& data, random::RandomSource& random) const;

    SpawnEquipmentEnchantments enchantments_;
    IsHalloween isHalloween_;
};

/// The finalizer used where there is no jar, which is every test and every server that has not
/// downloaded one. It equips mobs and rolls the same draws; only the enchantments come out empty.
[[nodiscard]] const SpawnEquipmentFinalizer& spawnEquipmentWithoutAssets();

} // namespace basalt::server::runtime::world
