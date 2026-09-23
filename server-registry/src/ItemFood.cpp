#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <charconv>
#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace basalt::server::registry::itemFood {
namespace {

using wire::common::Identifier;

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

[[nodiscard]] std::int32_t parseInt(std::string_view text) {
    std::int32_t value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

[[nodiscard]] float parseFloat(std::string_view text) {
    float value = 0.0F;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

/// `apply:effect,ticks,amplifier,probability`, `remove:effect`, `teleport:diameter` or `clear`, joined
/// with semicolons.
[[nodiscard]] std::vector<ConsumeEffect> parseEffects(std::string_view encoded) {
    std::vector<ConsumeEffect> effects;
    for (const std::string_view entry : split(encoded, ';')) {
        if (entry.empty()) {
            continue;
        }
        if (entry == "clear") {
            effects.emplace_back(ConsumeEffect::ClearAll{});
            continue;
        }
        const std::size_t colon = entry.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        const std::string_view kind = entry.substr(0, colon);
        const std::string_view body = entry.substr(colon + 1);
        if (kind == "remove") {
            effects.emplace_back(ConsumeEffect::Remove{Identifier::vanilla(std::string{body})});
            continue;
        }
        if (kind == "teleport") {
            effects.emplace_back(ConsumeEffect::TeleportRandomly{parseFloat(body)});
            continue;
        }
        if (kind != "apply") {
            continue;
        }
        const std::vector<std::string_view> parts = split(body, ',');
        if (parts.size() != 4) {
            continue;
        }
        effects.emplace_back(ConsumeEffect::Apply{Identifier::vanilla(std::string{parts[0]}), parseInt(parts[1]),
                                                  parseInt(parts[2]), parseFloat(parts[3])});
    }
    return effects;
}

[[nodiscard]] const std::unordered_map<Identifier, FoodValue>& table() {
    static const std::unordered_map<Identifier, FoodValue> built = [] {
        std::unordered_map<Identifier, FoodValue> table;
        for (const std::string_view encoded : itemFoodTable()) {
            const std::vector<std::string_view> fields = split(encoded, '|');
            if (fields.size() != 7) {
                continue;
            }
            FoodValue food;
            food.nutrition = parseInt(fields[1]);
            food.saturation = parseFloat(fields[2]);
            food.canAlwaysEat = fields[3] == "true";
            food.consumeSeconds = parseFloat(fields[4]);
            food.effects = parseEffects(fields[5]);
            if (!fields[6].empty()) {
                food.remainder = Identifier::vanilla(std::string{fields[6]});
            }
            table.emplace(Identifier::vanilla(std::string{fields[0]}), std::move(food));
        }
        return table;
    }();
    return built;
}

} // namespace

const FoodValue* of(const Identifier& item) {
    const auto found = table().find(item);
    return found == table().end() ? nullptr : &found->second;
}

} // namespace basalt::server::registry::itemFood
