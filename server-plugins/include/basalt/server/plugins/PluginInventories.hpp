#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
class PluginInventories {
public:
    PluginInventories() = default;
    PluginInventories(const PluginInventories&) = delete;
    PluginInventories& operator=(const PluginInventories&) = delete;
    PluginInventories(PluginInventories&&) = delete;
    PluginInventories& operator=(PluginInventories&&) = delete;
    virtual ~PluginInventories() = default;

    /// What the registries know about an item (stack size, durability, food, fuel, armour...) as an unnamed NBT
    /// compound.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> itemData(std::string_view id) const {
        static_cast<void>(id);
        return std::nullopt;
    }

    /// The ids of every enchantment the datapack defines.
    [[nodiscard]] virtual std::vector<std::string> enchantmentIds() const { return {}; }

    /// What a crafting grid makes: `{items:[...]}` in, `{recipe, result, remaining:[...]}` out; nothing when no recipe
    /// fits.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> craft(std::int32_t width, std::int32_t height,
                                                                         std::span<const std::uint8_t> items) const {
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(items);
        return std::nullopt;
    }

    /// The datapack definition of an enchantment as an unnamed NBT compound.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> enchantmentData(std::string_view id) const {
        static_cast<void>(id);
        return std::nullopt;
    }

    /// Every mob effect as a compound list under `effects`: id, protocol_id and colour.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> mobEffects() const { return std::nullopt; }

    /// Every potion as a compound list under `potions`: id and the effects it carries (id, amplifier, duration).
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> potions() const { return std::nullopt; }

    /// The ids of every crafting-table, cooking, stonecutting and smithing recipe the server has.
    [[nodiscard]] virtual std::vector<std::string> recipeIds() const { return {}; }

    /// A recipe as an unnamed NBT compound mirroring its fields; nothing when there is none by that id.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> recipeData(std::string_view id) const {
        static_cast<void>(id);
        return std::nullopt;
    }

    /// Registers a recipe from the same compound; false when the id is taken or the data is not a recipe.
    virtual bool addRecipe(std::string_view id, std::span<const std::uint8_t> data) {
        static_cast<void>(id);
        static_cast<void>(data);
        return false;
    }

    /// Registers a recipe written in the datapack's own JSON; false when the id is taken or the text is not one.
    virtual bool addRecipeJson(std::string_view id, std::string_view json) {
        static_cast<void>(id);
        static_cast<void>(json);
        return false;
    }

    virtual bool removeRecipe(std::string_view id) {
        static_cast<void>(id);
        return false;
    }

    /// Empties the recipe list, or puts back the datapack's own.
    virtual void clearRecipes() {}

    virtual void resetRecipes() {}

    /// Tags of a datapack registry as tag, members joined by `|`, tag, members...
    [[nodiscard]] virtual std::vector<std::string> registryTags(std::string_view registry) const {
        static_cast<void>(registry);
        return {};
    }
};
} // namespace basalt::server::plugins
