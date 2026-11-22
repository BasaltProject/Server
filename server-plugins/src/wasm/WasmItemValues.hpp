#pragma once

#include "WasmState.hpp"

#include <basalt/server/plugins/PluginPlayerAccess.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/wasm/Value.hpp>

#include <algorithm>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::plugins::wasm::items {
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using wire::wasm::MapEntry;
using wire::wasm::Value;

inline constexpr std::string_view kCustomName = "minecraft:custom_name";
inline constexpr std::string_view kDamage = "minecraft:damage";
inline constexpr std::string_view kEnchantments = "minecraft:enchantments";

using Slot = std::optional<PluginItem>;

[[nodiscard]] inline std::string qualified(std::string_view id) {
    return id.find(':') == std::string_view::npos ? "minecraft:" + std::string{id} : std::string{id};
}

/// The components with the name, damage and enchantments views folded back in, as the stack is stored.
[[nodiscard]] inline NbtCompound mergedComponents(const PluginItem& item) {
    NbtCompound components = item.components;
    if (item.name.has_value()) {
        components.set(kCustomName, NbtTag::ofString(*item.name));
    }
    if (item.damage > 0) {
        components.set(kDamage, NbtTag::ofInt(item.damage));
    }
    if (!item.enchantments.empty()) {
        NbtCompound levels;
        if (const NbtCompound* held = components.getCompound(kEnchantments); held != nullptr) {
            levels = *held;
        }
        for (const auto& [id, level] : item.enchantments) {
            levels.set(id, NbtTag::ofInt(level));
        }
        components.set(kEnchantments, NbtTag::ofCompound(std::move(levels)));
    }
    return components;
}

[[nodiscard]] inline PluginItem itemOfCompound(const NbtCompound& root) {
    PluginItem item{.id = std::string{root.getString("id")}, .count = root.getInt("count", 1)};
    if (const NbtCompound* components = root.getCompound("components"); components != nullptr) {
        item.components = *components;
    }
    if (const NbtTag* name = item.components.find(kCustomName); name != nullptr && name->get<std::string>()) {
        item.name = *name->get<std::string>();
    }
    item.damage = item.components.getInt(kDamage);
    if (const NbtCompound* levels = item.components.getCompound(kEnchantments); levels != nullptr) {
        for (const std::string& id : levels->names()) {
            item.enchantments.emplace(id, levels->getInt(id));
        }
    }
    return item;
}

[[nodiscard]] inline NbtCompound compoundOf(const PluginItem& item) {
    NbtCompound out;
    out.putString("id", item.id);
    out.putInt("count", std::max(item.count, 1));
    out.set("components", NbtTag::ofCompound(mergedComponents(item)));
    return out;
}

/// A stack as `{id:...,count:...,components:{...}}` text; the shape the player operations take.
[[nodiscard]] inline std::string snbtOf(const PluginItem& item) {
    return wire::nbt::snbt::stringify(NbtTag::ofCompound(compoundOf(item)));
}

[[nodiscard]] inline std::optional<PluginItem> itemFromSnbt(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    const std::expected<NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(text);
    const NbtCompound* root = parsed.has_value() ? parsed->get<NbtCompound>() : nullptr;
    if (root == nullptr || root->getString("id").empty()) {
        return std::nullopt;
    }
    return itemOfCompound(*root);
}

[[nodiscard]] inline std::vector<std::uint8_t> anonymous(NbtCompound compound) {
    return wire::nbt::nbtIO::writeAnonymous(NbtTag::ofCompound(std::move(compound)));
}

[[nodiscard]] inline std::optional<NbtCompound> compoundFromBytes(std::span<const std::uint8_t> bytes) {
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(bytes);
    const NbtCompound* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    return root == nullptr ? std::nullopt : std::optional<NbtCompound>{*root};
}

[[nodiscard]] inline Value strings(const std::vector<std::string>& texts) {
    Value::List list;
    list.reserve(texts.size());
    for (const std::string& text : texts) {
        list.emplace_back(text);
    }
    return Value{std::move(list)};
}

[[nodiscard]] inline Value toValue(const PluginItem& item) {
    Value::Map map{{"id", Value{item.id}}, {"count", Value{static_cast<std::int64_t>(item.count)}}};
    if (item.name.has_value()) {
        map.push_back({"name", Value{*item.name}});
    }
    if (!item.lore.empty()) {
        map.push_back({"lore", strings(item.lore)});
    }
    if (item.damage > 0) {
        map.push_back({"damage", Value{static_cast<std::int64_t>(item.damage)}});
    }
    if (!item.enchantments.empty()) {
        Value::Map levels;
        for (const auto& [id, level] : item.enchantments) {
            levels.push_back({id, Value{static_cast<std::int64_t>(level)}});
        }
        map.push_back({"enchantments", Value{std::move(levels)}});
    }
    if (!item.components.empty()) {
        Value::Map components;
        for (std::size_t at = 0; at < item.components.names().size(); ++at) {
            components.push_back(
                {item.components.names()[at], Value{wire::nbt::snbt::stringify(item.components.values()[at])}});
        }
        map.push_back({"components", Value{std::move(components)}});
    }
    return Value{std::move(map)};
}

[[nodiscard]] inline Value toValue(const Slot& item) { return item.has_value() ? toValue(*item) : Value{}; }

/// The item a value describes; nullopt when it is malformed, `knownItem` rejecting an unknown id.
template <typename Known>
[[nodiscard]] std::optional<PluginItem> fromValue(const Value& value, const Known& knownItem) {
    if (value.asMap() == nullptr) {
        return std::nullopt;
    }
    const Value* id = value.find("id");
    if (id == nullptr || id->asString() == nullptr || id->asString()->empty()) {
        return std::nullopt;
    }
    PluginItem item;
    item.id = qualified(*id->asString());
    if (!knownItem(item.id)) {
        return std::nullopt;
    }
    if (const Value* count = value.find("count"); count != nullptr && !count->isNull()) {
        if (count->asInt() == nullptr) {
            return std::nullopt;
        }
        item.count = static_cast<std::int32_t>(std::clamp<std::int64_t>(*count->asInt(), 1, 99));
    }
    if (const Value* name = value.find("name"); name != nullptr && !name->isNull()) {
        if (name->asString() == nullptr) {
            return std::nullopt;
        }
        item.name = *name->asString();
    }
    if (const Value* lore = value.find("lore"); lore != nullptr && !lore->isNull()) {
        if (lore->asList() == nullptr) {
            return std::nullopt;
        }
        for (const Value& line : *lore->asList()) {
            if (line.asString() == nullptr) {
                return std::nullopt;
            }
            item.lore.push_back(*line.asString());
        }
    }
    if (const Value* damage = value.find("damage"); damage != nullptr && !damage->isNull()) {
        if (damage->asInt() == nullptr) {
            return std::nullopt;
        }
        item.damage = static_cast<std::int32_t>(std::max<std::int64_t>(0, *damage->asInt()));
    }
    if (const Value* levels = value.find("enchantments"); levels != nullptr && !levels->isNull()) {
        if (levels->asMap() == nullptr) {
            return std::nullopt;
        }
        for (const MapEntry& entry : *levels->asMap()) {
            if (entry.value.asInt() == nullptr) {
                return std::nullopt;
            }
            item.enchantments[qualified(entry.key)] = static_cast<std::int32_t>(*entry.value.asInt());
        }
    }
    if (const Value* components = value.find("components"); components != nullptr && !components->isNull()) {
        if (components->asMap() == nullptr) {
            return std::nullopt;
        }
        for (const MapEntry& entry : *components->asMap()) {
            const std::string* text = entry.value.asString();
            if (text == nullptr) {
                return std::nullopt;
            }
            const std::expected<NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(*text);
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            item.components.set(qualified(entry.key), *parsed);
        }
    }
    return item;
}

[[nodiscard]] inline Value nbtToValue(const NbtTag& tag) {
    if (const auto* compound = tag.get<NbtCompound>()) {
        Value::Map map;
        for (std::size_t at = 0; at < compound->names().size(); ++at) {
            map.push_back({compound->names()[at], nbtToValue(compound->values()[at])});
        }
        return Value{std::move(map)};
    }
    if (const auto* list = tag.get<wire::nbt::NbtList>()) {
        Value::List out;
        for (const NbtTag& element : *list) {
            out.push_back(nbtToValue(element));
        }
        return Value{std::move(out)};
    }
    if (const auto* text = tag.get<std::string>()) {
        return Value{*text};
    }
    if (const auto* whole = tag.get<std::int8_t>()) {
        return Value{static_cast<std::int64_t>(*whole)};
    }
    if (const auto* whole = tag.get<std::int16_t>()) {
        return Value{static_cast<std::int64_t>(*whole)};
    }
    if (const auto* whole = tag.get<std::int32_t>()) {
        return Value{static_cast<std::int64_t>(*whole)};
    }
    if (const auto* whole = tag.get<std::int64_t>()) {
        return Value{*whole};
    }
    if (const auto* real = tag.get<float>()) {
        return Value{static_cast<double>(*real)};
    }
    if (const auto* real = tag.get<double>()) {
        return Value{*real};
    }
    Value::List out;
    if (const auto* ints = tag.get<std::vector<std::int32_t>>()) {
        for (const std::int32_t whole : *ints) {
            out.emplace_back(static_cast<std::int64_t>(whole));
        }
    } else if (const auto* longs = tag.get<std::vector<std::int64_t>>()) {
        for (const std::int64_t whole : *longs) {
            out.emplace_back(whole);
        }
    } else if (const auto* bytes = tag.get<std::vector<std::int8_t>>()) {
        for (const std::int8_t whole : *bytes) {
            out.emplace_back(static_cast<std::int64_t>(whole));
        }
    }
    return Value{std::move(out)};
}

/// An item compound of the services (`{id, count, components}`, empty for none) as an item value or null.
[[nodiscard]] inline Value itemValueOfCompound(const NbtTag* tag) {
    const NbtCompound* compound = tag == nullptr ? nullptr : tag->get<NbtCompound>();
    return compound == nullptr || compound->getString("id").empty() ? Value{} : toValue(itemOfCompound(*compound));
}

/// What a crafting grid or station answers, with every stack turned into an item value.
[[nodiscard]] inline Value stationValue(const NbtCompound& answer) {
    Value::Map map;
    for (std::size_t at = 0; at < answer.names().size(); ++at) {
        const std::string& name = answer.names()[at];
        const NbtTag& tag = answer.values()[at];
        if (name == "result") {
            map.push_back({name, itemValueOfCompound(&tag)});
        } else if (const auto* list = tag.get<wire::nbt::NbtList>(); name == "remaining" && list != nullptr) {
            Value::List cells;
            for (const NbtTag& cell : *list) {
                cells.push_back(itemValueOfCompound(&cell));
            }
            map.push_back({name, Value{std::move(cells)}});
        } else {
            map.push_back({name, nbtToValue(tag)});
        }
    }
    return Value{std::move(map)};
}
} // namespace basalt::server::plugins::wasm::items
