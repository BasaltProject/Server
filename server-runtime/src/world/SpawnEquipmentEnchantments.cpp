#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

namespace reg = server::registry;
namespace tags = server::registry::tags;

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

constexpr std::string_view kEnchantmentPrefix = "data/minecraft/enchantment/";
constexpr std::string_view kItemTagPrefix = "data/minecraft/tags/item/";
constexpr std::string_view kEnchantmentTagPrefix = "data/minecraft/tags/enchantment/";
constexpr std::string_view kEnchantmentsComponent = "minecraft:enchantments";
constexpr std::int32_t kMinCost = 5;
constexpr std::int32_t kMaxCostSpan = 17;
constexpr float kCostRandomSpan = 0.15F;
constexpr std::int32_t kAdditionalEnchantmentBound = 50;

[[nodiscard]] const Identifier& onMobSpawnEquipment() {
    static const Identifier tag = Identifier::vanilla("on_mob_spawn_equipment");
    return tag;
}

[[nodiscard]] std::optional<std::int32_t> intAt(const JsonObject& root, std::string_view key) {
    const JsonValue* value = root.find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<std::int32_t>(*whole);
    }
    const auto* text = value->get<std::string>();
    if (text == nullptr) {
        return std::nullopt;
    }
    try {
        return static_cast<std::int32_t>(std::stoi(*text));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

[[nodiscard]] const std::string* stringAt(const JsonObject& root, std::string_view key) {
    const JsonValue* value = root.find(key);
    return value == nullptr ? nullptr : value->get<std::string>();
}

[[nodiscard]] std::vector<std::pair<Identifier, std::string>> entriesOf(const SpawnEquipmentJar& jar,
                                                                        std::string_view prefix) {
    std::vector<std::pair<Identifier, std::string>> found;
    if (!jar.entriesUnder) {
        return found;
    }
    std::vector<std::string> names = jar.entriesUnder(prefix);
    std::ranges::sort(names);
    for (const std::string& entry : names) {
        if (!entry.ends_with(".json")) {
            continue;
        }
        std::string path = entry.substr(prefix.size());
        path.resize(path.size() - 5);
        found.emplace_back(Identifier::vanilla(path), entry);
    }
    return found;
}

} // namespace

bool SpawnEquipmentEnchantments::Selector::matches(const Identifier& candidate,
                                                   const std::map<Identifier, std::set<Identifier>>& tags) const {
    if (id.has_value() && *id == candidate) {
        return true;
    }
    if (!tag.has_value()) {
        return false;
    }
    const auto found = tags.find(*tag);
    return found != tags.end() && found->second.contains(candidate);
}

SpawnEquipmentEnchantments::Selector SpawnEquipmentEnchantments::Selector::parse(std::string_view value) {
    const bool isTag = value.starts_with('#');
    const std::expected<Identifier, wire::common::IdentifierError> parsed =
        Identifier::parse(isTag ? value.substr(1) : value);
    if (!parsed.has_value()) {
        return {};
    }
    return isTag ? Selector{.tag = *parsed} : Selector{.id = *parsed};
}

SpawnEquipmentEnchantments::SpawnEquipmentEnchantments(const SpawnEquipmentJar& jar, wire::common::Logger logger)
    : logger_(std::move(logger)) {
    if (!jar.readText) {
        return;
    }
    for (const auto& [id, entry] : entriesOf(jar, kEnchantmentPrefix)) {
        const std::optional<std::string> text = jar.readText(entry);
        const std::expected<JsonValue, wire::common::JsonError> json =
            text.has_value() ? wire::common::parseJson(*text) : std::unexpected(wire::common::JsonError::SyntaxError);
        const auto* root = json.has_value() ? json->get<JsonObject>() : nullptr;
        const std::string* supported = root == nullptr ? nullptr : stringAt(*root, "supported_items");
        const std::optional<std::int32_t> weight = root == nullptr ? std::nullopt : intAt(*root, "weight");
        const std::optional<std::int32_t> maxLevel = root == nullptr ? std::nullopt : intAt(*root, "max_level");
        const JsonValue* minCost = root == nullptr ? nullptr : root->find("min_cost");
        const JsonValue* maxCost = root == nullptr ? nullptr : root->find("max_cost");
        const auto* minObject = minCost == nullptr ? nullptr : minCost->get<JsonObject>();
        const auto* maxObject = maxCost == nullptr ? nullptr : maxCost->get<JsonObject>();
        if (supported == nullptr || !weight.has_value() || !maxLevel.has_value() || minObject == nullptr ||
            maxObject == nullptr) {
            logger_.warn([&id] { return "Skipped spawn enchantment " + id.toString() + ": it is not one."; });
            continue;
        }
        const Selector supportedItems = Selector::parse(*supported);
        const std::string* primary = stringAt(*root, "primary_items");
        const std::string* exclusive = stringAt(*root, "exclusive_set");
        definitions_.insert_or_assign(
            id, Definition{.id = id,
                           .primaryItems = primary == nullptr ? supportedItems : Selector::parse(*primary),
                           .weight = *weight,
                           .maxLevel = *maxLevel,
                           .minCost = {.base = intAt(*minObject, "base").value_or(0),
                                       .perLevel = intAt(*minObject, "per_level_above_first").value_or(0)},
                           .maxCost = {.base = intAt(*maxObject, "base").value_or(0),
                                       .perLevel = intAt(*maxObject, "per_level_above_first").value_or(0)},
                           .exclusive = exclusive == nullptr ? Selector{} : Selector::parse(*exclusive)});
    }

    const auto loadTags = [this, &jar](std::string_view prefix, const tags::ElementExists& exists) {
        std::map<Identifier, tags::TagFile> files;
        for (const auto& [id, entry] : entriesOf(jar, prefix)) {
            const std::optional<std::string> text = jar.readText(entry);
            const std::expected<tags::TagFile, tags::tagFileJson::ParseError> file =
                text.has_value() ? tags::tagFileJson::parse(*text)
                                 : std::unexpected(tags::tagFileJson::ParseError::NotAnObject);
            if (file.has_value()) {
                files.insert_or_assign(id, *file);
            } else {
                logger_.warn([&id] { return "Skipped spawn-equipment tag " + id.toString() + ": it is not one."; });
            }
        }
        const std::expected<std::map<Identifier, std::set<Identifier>>, tags::TagError> resolved =
            tags::resolve(files, exists);
        if (resolved.has_value()) {
            return *resolved;
        }
        logger_.warn(
            [&resolved] { return "Spawn-equipment tags could not be resolved: " + resolved.error().message(); });
        return std::map<Identifier, std::set<Identifier>>{};
    };
    itemTags_ =
        loadTags(kItemTagPrefix, [](const Identifier& id) { return reg::items::registry().byName(id) != nullptr; });
    enchantmentTags_ =
        loadTags(kEnchantmentTagPrefix, [this](const Identifier& id) { return definitions_.contains(id); });
}

std::vector<SpawnEquipmentEnchantments::Candidate> SpawnEquipmentEnchantments::available(const Identifier& item,
                                                                                         std::int32_t cost) const {
    std::vector<Candidate> candidates;
    const auto tag = enchantmentTags_.find(onMobSpawnEquipment());
    if (tag == enchantmentTags_.end()) {
        return candidates;
    }
    for (const Identifier& id : tag->second) {
        const auto found = definitions_.find(id);
        if (found == definitions_.end() || !found->second.primaryItems.matches(item, itemTags_)) {
            continue;
        }
        // parity: the highest level whose cost band this draw lands in, which is why a rich draw.
        for (std::int32_t level = found->second.maxLevel; level >= 1; level--) {
            if (cost >= found->second.minCost.at(level) && cost <= found->second.maxCost.at(level)) {
                candidates.push_back({.definition = &found->second, .level = level});
                break;
            }
        }
    }
    return candidates;
}

bool SpawnEquipmentEnchantments::compatible(const Definition& first, const Definition& second) const {
    return first.id != second.id && !first.exclusive.matches(second.id, enchantmentTags_) &&
           !second.exclusive.matches(first.id, enchantmentTags_);
}

void SpawnEquipmentEnchantments::enchant(NbtCompound& stack, random::RandomSource& random,
                                         float specialMultiplier) const {
    // parity: `Mth.randomBetweenInclusive` always draws, including over a zero-width range.
    const std::int32_t cost =
        kMinCost + random.nextInt(static_cast<std::int32_t>(specialMultiplier * kMaxCostSpan) + 1);
    const NbtTag* idTag = stack.find("id");
    const auto* name = idTag == nullptr ? nullptr : idTag->get<std::string>();
    if (name == nullptr) {
        return;
    }
    const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(*name);
    if (!item.has_value()) {
        return;
    }
    const std::optional<std::int32_t> enchantability = vanillaItemEnchantability::valueOf(item->toString());
    if (!enchantability.has_value()) {
        return;
    }
    std::int32_t effectiveCost =
        cost + 1 + random.nextInt(*enchantability / 4 + 1) + random.nextInt(*enchantability / 4 + 1);
    const float randomSpan = (random.nextFloat() + random.nextFloat() - 1.0F) * kCostRandomSpan;
    effectiveCost = std::max(1, static_cast<std::int32_t>(std::lround(static_cast<float>(effectiveCost) +
                                                                      static_cast<float>(effectiveCost) * randomSpan)));

    std::vector<Candidate> candidates = available(*item, effectiveCost);
    if (candidates.empty()) {
        return;
    }
    const auto weightedPick = [&random](const std::vector<Candidate>& from) {
        std::int32_t total = 0;
        for (const Candidate& candidate : from) {
            total += candidate.definition->weight;
        }
        std::int32_t index = random.nextInt(total);
        for (const Candidate& candidate : from) {
            index -= candidate.definition->weight;
            if (index < 0) {
                return candidate;
            }
        }
        return from.back();
    };

    std::vector<Candidate> selected{weightedPick(candidates)};
    while (random.nextInt(kAdditionalEnchantmentBound) <= effectiveCost) {
        const Candidate previous = selected.back();
        std::erase_if(candidates, [this, &previous](const Candidate& one) {
            return !compatible(*previous.definition, *one.definition);
        });
        if (candidates.empty()) {
            break;
        }
        selected.push_back(weightedPick(candidates));
        effectiveCost /= 2;
    }

    NbtCompound components;
    if (const NbtTag* existing = stack.find("components"); existing != nullptr) {
        if (const auto* compound = existing->get<NbtCompound>(); compound != nullptr) {
            components = *compound;
        }
    }
    NbtCompound levels;
    if (const NbtTag* existing = components.find(kEnchantmentsComponent); existing != nullptr) {
        if (const auto* compound = existing->get<NbtCompound>(); compound != nullptr) {
            levels = *compound;
        }
    }
    for (const Candidate& candidate : selected) {
        const std::string key = candidate.definition->id.toString();
        const NbtTag* current = levels.find(key);
        const auto* held = current == nullptr ? nullptr : current->get<std::int32_t>();
        levels.putInt(key, std::max(held == nullptr ? 0 : *held, candidate.level));
    }
    components.set(kEnchantmentsComponent, NbtTag::ofCompound(std::move(levels)));
    stack.set("components", NbtTag::ofCompound(std::move(components)));
}

} // namespace basalt::server::runtime::world
