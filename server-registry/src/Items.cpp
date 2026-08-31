#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/VanillaEntries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <unordered_map>
#include <vector>

namespace basalt::server::registry {
namespace {

using wire::common::Identifier;

[[nodiscard]] Identifier vanilla(std::string_view path) { return Identifier::vanilla(std::string{path}); }

[[nodiscard]] std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    while (true) {
        const std::size_t at = text.find(separator);
        if (at == std::string_view::npos) {
            parts.push_back(text);
            return parts;
        }
        parts.push_back(text.substr(0, at));
        text = text.substr(at + 1);
    }
}

[[nodiscard]] float parseFloat(std::string_view text) {
    float value = 0.0F;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

[[nodiscard]] std::optional<std::int32_t> parseInt(std::string_view text) {
    std::int32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

/// A name-keyed table built once from one of the generated lists.
template <typename T, typename Build>
[[nodiscard]] const std::unordered_map<Identifier, T>& lazyTable(Build build) {
    static const std::unordered_map<Identifier, T> table = build();
    return table;
}

} // namespace

namespace itemArmour {

const ArmourPiece* of(const Identifier& item) {
    static const std::unordered_map<Identifier, ArmourPiece> table = [] {
        std::unordered_map<Identifier, ArmourPiece> built;
        for (const std::string_view encoded : registry::itemArmourTable()) {
            const std::vector<std::string_view> fields = split(encoded, '|');
            built.emplace(vanilla(fields[0]),
                          ArmourPiece{fields[1], parseFloat(fields[2]), parseFloat(fields[3]), parseFloat(fields[4])});
        }
        return built;
    }();
    const auto found = table.find(item);
    return found == table.end() ? nullptr : &found->second;
}

} // namespace itemArmour

namespace itemWeapons {

const WeaponStats* of(const Identifier& item) {
    static const std::unordered_map<Identifier, WeaponStats> table = [] {
        std::unordered_map<Identifier, WeaponStats> built;
        for (const std::string_view encoded : registry::itemWeaponsTable()) {
            const std::vector<std::string_view> fields = split(encoded, '|');
            built.emplace(vanilla(fields[0]), WeaponStats{static_cast<double>(parseFloat(fields[1])),
                                                          static_cast<double>(parseFloat(fields[2]))});
        }
        return built;
    }();
    const auto found = table.find(item);
    return found == table.end() ? nullptr : &found->second;
}

} // namespace itemWeapons

namespace itemStacking {
namespace {

[[nodiscard]] const std::unordered_map<std::string_view, ItemStackingEntry>& table() {
    static const std::unordered_map<std::string_view, ItemStackingEntry> built = [] {
        std::unordered_map<std::string_view, ItemStackingEntry> table;
        for (const std::string_view row : registry::itemStackingTable()) {
            const std::vector<std::string_view> parts = split(row, '|');
            if (parts.size() != 3) {
                continue;
            }
            const std::optional<std::int32_t> stack = parseInt(parts[1]);
            const std::optional<std::int32_t> cooldown = parseInt(parts[2]);
            if (!stack.has_value() || !cooldown.has_value()) {
                continue;
            }
            table.emplace(parts[0], ItemStackingEntry{*stack, *cooldown});
        }
        return table;
    }();
    return built;
}

} // namespace

const ItemStackingEntry* of(const Identifier& item) {
    const auto found = table().find(item.path());
    return found == table().end() ? nullptr : &found->second;
}

std::int32_t maxStackSize(const Identifier& item) {
    const ItemStackingEntry* entry = of(item);
    return entry == nullptr ? kDefaultStackSize : entry->maxStackSize;
}

std::int32_t cooldownTicks(const Identifier& item) {
    const ItemStackingEntry* entry = of(item);
    return entry == nullptr ? 0 : entry->cooldownTicks;
}

} // namespace itemStacking

namespace itemDurability {

const Entry* of(const Identifier& item) {
    static const std::unordered_map<Identifier, Entry> table = [] {
        std::unordered_map<Identifier, Entry> built;
        for (const ItemDurabilityEntry& entry : registry::itemDurabilityTable()) {
            built.emplace(vanilla(entry.item), Entry{entry.maxDamage, entry.damagePerBlock, entry.damagePerAttack});
        }
        return built;
    }();
    const auto found = table.find(item);
    return found == table.end() ? nullptr : &found->second;
}

std::int32_t maxDamageOf(const Identifier& item) {
    const Entry* entry = of(item);
    return entry == nullptr ? 0 : entry->maxDamage;
}

bool isDamageable(const Identifier& item) { return of(item) != nullptr; }

} // namespace itemDurability

namespace itemBlocks {
namespace {

struct Table {
    std::unordered_map<Identifier, Identifier> byItem;
    std::unordered_map<Identifier, Identifier> byBlock;
};

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        for (const auto& [item, block] : registry::itemBlocksTable()) {
            table.byItem.emplace(vanilla(item), vanilla(block));
            table.byBlock.emplace(vanilla(block), vanilla(item));
        }
        return table;
    }();
    return built;
}

} // namespace

Identifier of(const Identifier& item) {
    const auto found = table().byItem.find(item);
    return found == table().byItem.end() ? item : found->second;
}

Identifier itemOf(const Identifier& block) {
    const auto found = table().byBlock.find(block);
    return found == table().byBlock.end() ? block : found->second;
}

} // namespace itemBlocks

namespace craftingRemainders {

const Identifier* of(const Identifier& item) {
    static const std::unordered_map<Identifier, Identifier> table = [] {
        std::unordered_map<Identifier, Identifier> built;
        for (const auto& [from, left] : registry::craftingRemaindersTable()) {
            built.emplace(vanilla(from), vanilla(left));
        }
        return built;
    }();
    const auto found = table.find(item);
    return found == table.end() ? nullptr : &found->second;
}

} // namespace craftingRemainders

namespace trimMaterials {

const Identifier* of(const Identifier& item) {
    static const std::unordered_map<Identifier, Identifier> table = [] {
        std::unordered_map<Identifier, Identifier> built;
        for (const auto& [from, material] : registry::trimMaterialsTable()) {
            built.emplace(vanilla(from), vanilla(material));
        }
        return built;
    }();
    const auto found = table.find(item);
    return found == table.end() ? nullptr : &found->second;
}

} // namespace trimMaterials

namespace fuels {

std::int32_t burnTicks(const Identifier& item) {
    static const std::unordered_map<std::string_view, std::int32_t> table = [] {
        std::unordered_map<std::string_view, std::int32_t> built;
        for (const auto& [name, ticks] : fuelValuesTable()) {
            built.emplace(name, ticks);
        }
        return built;
    }();
    const auto found = table.find(item.path());
    return found == table.end() ? 0 : found->second;
}

bool isFuel(const Identifier& item) { return burnTicks(item) > 0; }

} // namespace fuels

namespace dyeColours {
namespace {

/// parity: `DyeColor`'s own textureDiffuseColor, in its own declaration order.
constexpr std::array<std::pair<std::string_view, std::int32_t>, 16> kDyes = {{
    {"white_dye", 16383998},
    {"orange_dye", 16351261},
    {"magenta_dye", 13061821},
    {"light_blue_dye", 3847130},
    {"yellow_dye", 16701501},
    {"lime_dye", 8439583},
    {"pink_dye", 15961002},
    {"gray_dye", 4673362},
    {"light_gray_dye", 10329495},
    {"cyan_dye", 1481884},
    {"purple_dye", 8991416},
    {"blue_dye", 3949738},
    {"brown_dye", 8606770},
    {"green_dye", 6192150},
    {"red_dye", 11546150},
    {"black_dye", 1908001},
}};

} // namespace

std::optional<std::int32_t> of(const Identifier& item) {
    if (item.namespaceName() != Identifier::kVanillaNamespace) {
        return std::nullopt;
    }
    const auto found = std::ranges::find(kDyes, item.path(), &std::pair<std::string_view, std::int32_t>::first);
    return found == kDyes.end() ? std::nullopt : std::optional{found->second};
}

} // namespace dyeColours

namespace weathering {
namespace {

struct Table {
    std::unordered_map<std::string_view, std::string_view> next;
    std::unordered_map<std::string_view, std::int32_t> stages;
};

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        std::unordered_map<std::string_view, std::string_view> previous;
        for (const auto& [from, to] : weatheringStepsTable()) {
            table.next.emplace(from, to);
            previous.emplace(to, from);
        }
        // Walk back up each chain to see how far along a block already is.
        const auto stageOf = [&previous](std::string_view block) {
            std::int32_t stage = 0;
            std::string_view current = block;
            while (true) {
                const auto earlier = previous.find(current);
                if (earlier == previous.end()) {
                    return stage;
                }
                current = earlier->second;
                stage++;
            }
        };
        for (const auto& [from, to] : weatheringStepsTable()) {
            table.stages.emplace(from, stageOf(from));
            table.stages.emplace(to, stageOf(to));
        }
        return table;
    }();
    return built;
}

} // namespace

std::optional<std::string_view> next(std::string_view block) {
    const auto found = table().next.find(block);
    return found == table().next.end() ? std::nullopt : std::optional{found->second};
}

std::optional<std::int32_t> stageOf(std::string_view block) {
    const auto found = table().stages.find(block);
    return found == table().stages.end() ? std::nullopt : std::optional{found->second};
}

bool weathers(std::string_view block) { return table().stages.contains(block); }

} // namespace weathering

} // namespace basalt::server::registry
