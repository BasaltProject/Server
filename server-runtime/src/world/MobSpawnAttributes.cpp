#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/world/MobSpawnAttributes.hpp>
#include <basalt/server/runtime/world/MobSpawnVariantData.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr std::string_view kMaxHealth = "minecraft:max_health";
constexpr std::string_view kMovementSpeed = "minecraft:movement_speed";
constexpr std::string_view kJumpStrength = "minecraft:jump_strength";
constexpr std::int32_t kHorseBaseHealth = 15;
constexpr double kPandaWeakHealth = 10.0;
constexpr double kPandaLazySpeed = 0.07000000029802322;

[[nodiscard]] double horseMaxHealth(random::RandomSource& random) {
    return static_cast<double>(kHorseBaseHealth + random.nextInt(8) + random.nextInt(9));
}

[[nodiscard]] double horseSpeed(random::RandomSource& random) {
    // parity: the float literal is a float in vanilla and the three rolls are doubles, so the sum.
    return (static_cast<double>(0.45F) + random.nextDouble() * 0.3 + random.nextDouble() * 0.3 +
            random.nextDouble() * 0.3) *
           0.25;
}

[[nodiscard]] double horseJumpStrength(random::RandomSource& random) {
    return static_cast<double>(0.4F) + random.nextDouble() * 0.2 + random.nextDouble() * 0.2 +
           random.nextDouble() * 0.2;
}

[[nodiscard]] PandaGene geneNamed(const NbtCompound& data, std::string_view key) {
    const NbtTag* found = data.find(key);
    const auto* name = found == nullptr ? nullptr : found->get<std::string>();
    return name == nullptr ? PandaGene::Normal : pandaGeneByName(*name).value_or(PandaGene::Normal);
}

void writePanda(const NbtCompound& data, std::map<std::string, double>& attributes) {
    const PandaGene main = geneNamed(data, "MainGene");
    const PandaGene hidden = geneNamed(data, "HiddenGene");
    // parity: a recessive gene only shows when both copies agree, which is what makes brown pandas.
    const PandaGene expressed = pandaGeneRecessive(main) && main != hidden ? PandaGene::Normal : main;
    if (expressed == PandaGene::Weak) {
        attributes[std::string{kMaxHealth}] = kPandaWeakHealth;
    } else if (expressed == PandaGene::Lazy) {
        attributes[std::string{kMovementSpeed}] = kPandaLazySpeed;
    }
}

[[nodiscard]] std::vector<NbtCompound> compoundsAt(const NbtCompound& data, std::string_view key) {
    std::vector<NbtCompound> entries;
    const NbtTag* found = data.find(key);
    const auto* list = found == nullptr ? nullptr : found->get<NbtList>();
    if (list == nullptr) {
        return entries;
    }
    for (const NbtTag& entry : list->elements()) {
        if (const auto* compound = entry.get<NbtCompound>(); compound != nullptr) {
            entries.push_back(*compound);
        }
    }
    return entries;
}

[[nodiscard]] std::string idOf(const NbtCompound& entry) {
    const NbtTag* found = entry.find("id");
    const auto* name = found == nullptr ? nullptr : found->get<std::string>();
    return name == nullptr ? std::string{} : *name;
}

[[nodiscard]] NbtTag listOf(std::vector<NbtCompound> entries) {
    std::vector<NbtTag> tags;
    tags.reserve(entries.size());
    for (NbtCompound& entry : entries) {
        tags.push_back(NbtTag::ofCompound(std::move(entry)));
    }
    return NbtTag::ofList(NbtList{wire::nbt::NbtType::Compound, std::move(tags)});
}

void update(NbtCompound& data, std::string_view attributeId, double base, const NbtCompound* modifier) {
    std::vector<NbtCompound> packed = compoundsAt(data, "attributes");
    auto found =
        std::ranges::find_if(packed, [attributeId](const NbtCompound& one) { return idOf(one) == attributeId; });
    if (found == packed.end()) {
        NbtCompound fresh;
        fresh.set("id", NbtTag::ofString(std::string{attributeId}));
        packed.push_back(std::move(fresh));
        found = packed.end() - 1;
    }
    found->set("base", NbtTag::ofDouble(base));
    if (modifier != nullptr) {
        std::vector<NbtCompound> modifiers = compoundsAt(*found, "modifiers");
        const std::string wanted = idOf(*modifier);
        std::erase_if(modifiers, [&wanted](const NbtCompound& one) { return idOf(one) == wanted; });
        modifiers.push_back(*modifier);
        found->set("modifiers", listOf(std::move(modifiers)));
    }
    data.set("attributes", listOf(std::move(packed)));
}

} // namespace

namespace mobSpawnAttributes {

void write(std::string_view type, random::RandomSource& random, const NbtCompound& data,
           std::map<std::string, double>& attributes) {
    if (type == "horse") {
        attributes[std::string{kMaxHealth}] = horseMaxHealth(random);
        attributes[std::string{kMovementSpeed}] = horseSpeed(random);
        attributes[std::string{kJumpStrength}] = horseJumpStrength(random);
    } else if (type == "donkey" || type == "llama" || type == "mule" || type == "trader_llama") {
        attributes[std::string{kMaxHealth}] = horseMaxHealth(random);
    } else if (type == "skeleton_horse") {
        attributes[std::string{kJumpStrength}] = horseJumpStrength(random);
    } else if (type == "panda") {
        writePanda(data, attributes);
    }
}

PandaGene pandaGene(std::int32_t roll) {
    switch (roll) {
    case 0:
        return PandaGene::Lazy;
    case 1:
        return PandaGene::Worried;
    case 2:
        return PandaGene::Playful;
    case 4:
        return PandaGene::Aggressive;
    case 9:
    case 10:
        return PandaGene::Brown;
    default:
        break;
    }
    // parity: every other roll inside the range is `weak`.
    return roll >= 0 && roll <= 8 ? PandaGene::Weak : PandaGene::Normal;
}

} // namespace mobSpawnAttributes

namespace spawnAttributeNbt {

void setBase(NbtCompound& data, std::string_view attributeId, double base) { update(data, attributeId, base, nullptr); }

void addModifier(NbtCompound& data, std::string_view attributeId, double base, std::string_view modifierId,
                 double amount, entity::AttributeOperation operation) {
    NbtCompound modifier;
    modifier.set("id", NbtTag::ofString(std::string{modifierId}));
    modifier.set("amount", NbtTag::ofDouble(amount));
    modifier.set("operation", NbtTag::ofString(std::string{entity::attributeOperationName(operation)}));
    update(data, attributeId, base, &modifier);
}

} // namespace spawnAttributeNbt

} // namespace basalt::server::runtime::world
