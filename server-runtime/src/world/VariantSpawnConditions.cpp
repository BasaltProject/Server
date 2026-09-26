#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/VariantSpawnConditions.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace basalt::server::runtime::world::variantSpawnConditions {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

constexpr std::array<std::string_view, 7> kVariantRegistries{
    "wolf_variant",           "cat_variant", "pig_variant", "cow_variant", "chicken_variant", "frog_variant",
    "zombie_nautilus_variant"};
constexpr std::array<std::string_view, 5> kSoundRegistries{
    "wolf_sound_variant", "cat_sound_variant", "pig_sound_variant", "cow_sound_variant", "chicken_sound_variant"};

struct Condition {
    enum class Kind : std::uint8_t { Always, Biome, MoonBrightness, Structure };

    Kind kind = Kind::Always;
    /// Biome ids or `#tags`, resolved against `biomeTags` when tested.
    std::vector<std::string> holders;
    std::optional<double> min;
    std::optional<double> max;
};

struct Selector {
    std::size_t entry = 0;
    std::int32_t priority = 0;
    Condition condition;
};

struct State {
    std::map<std::string, std::vector<Selector>, std::less<>> registries;
    std::map<std::string, std::vector<Identifier>, std::less<>> entries;
    std::map<Identifier, std::set<Identifier>> biomeTags;
};

[[nodiscard]] State& state() {
    static State installedState;
    return installedState;
}

[[nodiscard]] bool& isInstalled() {
    static bool value = false;
    return value;
}

[[nodiscard]] std::vector<std::string> holdersOf(const JsonValue* value) {
    std::vector<std::string> holders;
    if (value == nullptr) {
        return holders;
    }
    if (const auto* one = value->get<std::string>(); one != nullptr) {
        holders.push_back(*one);
    } else if (const auto* list = value->get<std::vector<JsonValue>>(); list != nullptr) {
        for (const JsonValue& element : *list) {
            if (const auto* one = element.get<std::string>(); one != nullptr) {
                holders.push_back(*one);
            }
        }
    }
    return holders;
}

[[nodiscard]] std::optional<double> numberOf(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* real = value->get<double>(); real != nullptr) {
        return *real;
    }
    if (const auto* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    return std::nullopt;
}

[[nodiscard]] Condition conditionOf(const JsonObject* object) {
    Condition condition;
    if (object == nullptr) {
        return condition;
    }
    const JsonValue* type = object->find("type");
    const auto* name = type == nullptr ? nullptr : type->get<std::string>();
    const std::string kind =
        name == nullptr ? std::string{} : Identifier::parse(*name).value_or(Identifier::vanilla("")).toString();
    if (kind == "minecraft:biome") {
        condition.kind = Condition::Kind::Biome;
        condition.holders = holdersOf(object->find("biomes"));
    } else if (kind == "minecraft:structure") {
        condition.kind = Condition::Kind::Structure;
        condition.holders = holdersOf(object->find("structures"));
    } else if (kind == "minecraft:moon_brightness") {
        condition.kind = Condition::Kind::MoonBrightness;
        const JsonValue* range = object->find("range");
        if (const std::optional<double> exact = numberOf(range); exact.has_value()) {
            condition.min = exact;
            condition.max = exact;
        } else if (const auto* bounds = range == nullptr ? nullptr : range->get<JsonObject>(); bounds != nullptr) {
            condition.min = numberOf(bounds->find("min"));
            condition.max = numberOf(bounds->find("max"));
        }
    }
    return condition;
}

[[nodiscard]] bool inBiome(const Condition& condition, const VariantSpawnFacts& facts) {
    if (!facts.biome.has_value()) {
        return false;
    }
    return std::ranges::any_of(condition.holders, [&facts](const std::string& holder) {
        if (holder.starts_with('#')) {
            const auto tag = Identifier::parse(std::string_view{holder}.substr(1));
            const auto found = tag.has_value() ? state().biomeTags.find(*tag) : state().biomeTags.end();
            return found != state().biomeTags.end() && found->second.contains(*facts.biome);
        }
        return Identifier::parse(holder) == *facts.biome;
    });
}

[[nodiscard]] bool holds(const Condition& condition, const VariantSpawnFacts& facts) {
    switch (condition.kind) {
    case Condition::Kind::Biome:
        return inBiome(condition, facts);
    case Condition::Kind::Structure:
        return facts.inStructure && std::ranges::any_of(condition.holders, [&facts](const std::string& holder) {
                   return facts.inStructure(holder);
               });
    case Condition::Kind::MoonBrightness:
        return (!condition.min.has_value() || facts.moonBrightness >= *condition.min) &&
               (!condition.max.has_value() || facts.moonBrightness <= *condition.max);
    case Condition::Kind::Always:
    default:
        return true;
    }
}

} // namespace

void install(const datapack::EntriesUnder& entriesUnder, const datapack::ReadText& readText) {
    State built;
    if (!entriesUnder) {
        state() = std::move(built);
        isInstalled() = false;
        return;
    }
    built.biomeTags = datapack::resolveTags(
        entriesUnder, readText, "worldgen/biome", [](const Identifier&) { return true; },
        wire::common::Logger{"Variants"});
    for (const std::string_view registry : kSoundRegistries) {
        std::vector<Identifier>& ids = built.entries[std::string{registry}];
        for (const auto& [id, name] : datapack::dataFiles(entriesUnder, registry)) {
            ids.push_back(id);
        }
    }
    for (const std::string_view registry : kVariantRegistries) {
        std::vector<Selector>& selectors = built.registries[std::string{registry}];
        std::vector<Identifier>& ids = built.entries[std::string{registry}];
        // `dataFiles` answers in id order, which is the registry's.
        for (const auto& [id, name] : datapack::dataFiles(entriesUnder, registry)) {
            ids.push_back(id);
            const std::optional<std::string> text = readText ? readText(name) : std::nullopt;
            const auto json = text.has_value() ? wire::common::parseJson(*text)
                                               : std::expected<JsonValue, wire::common::JsonError>{
                                                     std::unexpected{wire::common::JsonError::SyntaxError}};
            const auto* root = json.has_value() ? json->get<JsonObject>() : nullptr;
            const JsonValue* conditions = root == nullptr ? nullptr : root->find("spawn_conditions");
            const auto* list = conditions == nullptr ? nullptr : conditions->get<std::vector<JsonValue>>();
            if (list == nullptr) {
                continue;
            }
            for (const JsonValue& element : *list) {
                const auto* selector = element.get<JsonObject>();
                if (selector == nullptr) {
                    continue;
                }
                const JsonValue* condition = selector->find("condition");
                selectors.push_back(
                    Selector{.entry = ids.size() - 1,
                             .priority = static_cast<std::int32_t>(numberOf(selector->find("priority")).value_or(0)),
                             .condition = conditionOf(condition == nullptr ? nullptr : condition->get<JsonObject>())});
            }
        }
    }
    // Without a jar or a pack there is nothing to read, and the built-in tables answer instead.
    const bool any = std::ranges::any_of(built.entries, [](const auto& entry) { return !entry.second.empty(); });
    state() = std::move(built);
    isInstalled() = any;
}

bool installed() { return isInstalled(); }

const std::vector<Identifier>& entries(std::string_view registry) {
    static const std::vector<Identifier> kNone;
    const auto found = state().entries.find(registry);
    return found == state().entries.end() ? kNone : found->second;
}

std::optional<Identifier> randomEntry(std::string_view registry, random::RandomSource& random) {
    const std::vector<Identifier>& ids = entries(registry);
    if (ids.empty()) {
        return std::nullopt;
    }
    return ids[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(ids.size())))];
}

std::optional<std::size_t> pick(std::string_view registry, const VariantSpawnFacts& facts,
                                random::RandomSource& random) {
    if (!isInstalled()) {
        return std::nullopt;
    }
    const auto found = state().registries.find(registry);
    if (found == state().registries.end()) {
        return std::nullopt;
    }
    std::vector<const Selector*> unpacked;
    for (const Selector& selector : found->second) {
        unpacked.push_back(&selector);
    }
    std::ranges::stable_sort(
        unpacked, [](const Selector* left, const Selector* right) { return left->priority > right->priority; });
    std::vector<std::size_t> selected;
    std::int32_t highest = std::numeric_limits<std::int32_t>::min();
    for (const Selector* selector : unpacked) {
        if (selector->priority < highest) {
            continue;
        }
        if (holds(selector->condition, facts)) {
            highest = selector->priority;
            selected.push_back(selector->entry);
        }
    }
    if (selected.empty()) {
        return std::nullopt;
    }
    // parity: `Util.getRandomSafe`, a `nextInt` even over one.
    return selected[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(selected.size())))];
}

} // namespace basalt::server::runtime::world::variantSpawnConditions
