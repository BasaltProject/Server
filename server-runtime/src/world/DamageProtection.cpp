#include <basalt/server/runtime/world/DamageProtection.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::damageProtection {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

[[nodiscard]] bool matches(const NbtCompound* requirements, const Identifier* damageType, const Sources& sources);

/// parity: `LevelBasedValue.Linear`, which is the only shape a protection uses.
[[nodiscard]] float amountOf(const NbtCompound* effect, std::int32_t level) {
    const NbtCompound* value = effect == nullptr ? nullptr : effect->getCompound("value");
    if (value == nullptr) {
        return 0.0F;
    }
    const auto base = static_cast<float>(value->getDouble("base"));
    return base + static_cast<float>(value->getDouble("per_level_above_first")) * static_cast<float>(level - 1);
}

[[nodiscard]] bool matchesSource(const NbtCompound& condition, const Identifier* damageType, const Sources& sources) {
    const NbtCompound* predicate = condition.getCompound("predicate");
    if (predicate == nullptr) {
        return true;
    }
    const NbtList* tests = predicate->getList("tags");
    if (tests == nullptr) {
        return true;
    }
    if (damageType == nullptr) {
        return false;
    }
    for (const NbtTag& element : *tests) {
        const NbtCompound* test = element.get<NbtCompound>();
        if (test == nullptr) {
            continue;
        }
        const NbtTag* id = test->find("id");
        const std::string* name = id == nullptr ? nullptr : id->get<std::string>();
        if (name == nullptr) {
            continue;
        }
        // A name that is not an identifier at all names no tag, so it tests nothing.
        const std::expected<Identifier, wire::common::IdentifierError> tag = Identifier::parse(*name);
        if (!tag.has_value()) {
            continue;
        }
        if (sources.isDamageTypeIn(*tag, *damageType) != test->getBoolean("expected")) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool allTermsMatch(const NbtCompound& requirements, const Identifier* damageType,
                                 const Sources& sources) {
    const NbtList* terms = requirements.getList("terms");
    if (terms == nullptr) {
        return true;
    }
    for (const NbtTag& element : *terms) {
        const NbtCompound* term = element.get<NbtCompound>();
        if (term != nullptr && !matches(term, damageType, sources)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool anyTermMatches(const NbtCompound& requirements, const Identifier* damageType,
                                  const Sources& sources) {
    const NbtList* terms = requirements.getList("terms");
    if (terms == nullptr) {
        return true;
    }
    for (const NbtTag& element : *terms) {
        const NbtCompound* term = element.get<NbtCompound>();
        if (term != nullptr && matches(term, damageType, sources)) {
            return true;
        }
    }
    return false;
}

bool matches(const NbtCompound* requirements, const Identifier* damageType, const Sources& sources) {
    if (requirements == nullptr) {
        return true;
    }
    const std::string_view condition = requirements->getString("condition");
    if (condition == "minecraft:all_of") {
        return allTermsMatch(*requirements, damageType, sources);
    }
    if (condition == "minecraft:any_of") {
        return anyTermMatches(*requirements, damageType, sources);
    }
    if (condition == "minecraft:damage_source_properties") {
        return matchesSource(*requirements, damageType, sources);
    }
    // A condition this does not know is treated as unmet: counting protection that vanilla would
    // not is worse than missing some, since it would make armour stronger than real.
    return false;
}

} // namespace

float of(std::span<const EnchantmentLevels> worn, const Identifier* damageType, const Sources& sources) {
    if (!sources.definitionOf || !sources.isDamageTypeIn) {
        return 0.0F;
    }
    float protection = 0.0F;
    for (const EnchantmentLevels& stack : worn) {
        for (const auto& [id, level] : stack) {
            const NbtCompound* definition = sources.definitionOf(id);
            if (definition == nullptr) {
                continue;
            }
            const NbtCompound* effects = definition->getCompound("effects");
            if (effects == nullptr) {
                continue;
            }
            const NbtList* entries = effects->getList("minecraft:damage_protection");
            if (entries == nullptr) {
                continue;
            }
            for (const NbtTag& element : *entries) {
                const NbtCompound* entry = element.get<NbtCompound>();
                if (entry == nullptr) {
                    continue;
                }
                if (!matches(entry->getCompound("requirements"), damageType, sources)) {
                    continue;
                }
                protection += amountOf(entry->getCompound("effect"), level);
            }
        }
    }
    return protection;
}

} // namespace basalt::server::runtime::world::damageProtection
