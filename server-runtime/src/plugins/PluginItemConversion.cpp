#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/plugins/PluginItemConversion.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins {

server::plugins::PluginItem pluginItemOf(const storage::StoredItemStack& stack) {
    server::plugins::PluginItem item{.id = stack.id, .count = stack.count, .components = stack.components};
    if (const wire::nbt::NbtTag* name = stack.components.find(item::itemComponents::kCustomName);
        name != nullptr && name->get<std::string>() != nullptr) {
        item.name = *name->get<std::string>();
    }
    item.damage = stack.components.getInt(item::itemComponents::kDamage);
    for (const auto& [id, level] : player::enchantmentLevels(stack)) {
        item.enchantments.emplace(id.toString(), level);
    }
    return item;
}

storage::StoredItemStack storedItemOf(const server::plugins::PluginItem& item) {
    wire::nbt::NbtCompound components = item.components;
    if (item.name.has_value()) {
        components.set(item::itemComponents::kCustomName, wire::nbt::NbtTag::ofString(*item.name));
    }
    if (item.damage > 0) {
        components.set(item::itemComponents::kDamage, wire::nbt::NbtTag::ofInt(item.damage));
    }
    if (!item.enchantments.empty()) {
        wire::nbt::NbtCompound levels = components.getCompound(item::itemComponents::kEnchantments) != nullptr
                                            ? *components.getCompound(item::itemComponents::kEnchantments)
                                            : wire::nbt::NbtCompound{};
        for (const auto& [id, level] : item.enchantments) {
            levels.set(id, wire::nbt::NbtTag::ofInt(level));
        }
        components.set(item::itemComponents::kEnchantments, wire::nbt::NbtTag::ofCompound(std::move(levels)));
    }
    return storage::StoredItemStack{
        .id = item.id, .count = std::max(item.count, 1), .components = std::move(components)};
}

std::string snbtOf(const storage::StoredItemStack& stack) {
    wire::nbt::NbtCompound root;
    root.putString("id", stack.id);
    root.putInt("count", stack.count);
    root.set("components", wire::nbt::NbtTag::ofCompound(stack.components));
    return wire::nbt::snbt::stringify(wire::nbt::NbtTag::ofCompound(std::move(root)));
}

std::optional<storage::StoredItemStack> stackFromSnbt(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(text);
    const wire::nbt::NbtCompound* root = parsed.has_value() ? parsed->get<wire::nbt::NbtCompound>() : nullptr;
    if (root == nullptr || !root->contains("id")) {
        return std::nullopt;
    }
    storage::StoredItemStack stack{.id = std::string{root->getString("id")}, .count = root->getInt("count", 1)};
    if (const wire::nbt::NbtCompound* components = root->getCompound("components"); components != nullptr) {
        stack.components = *components;
    }
    return stack;
}

} // namespace basalt::server::runtime::plugins
