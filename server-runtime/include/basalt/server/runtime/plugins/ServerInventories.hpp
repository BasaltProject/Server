#pragma once

#include <basalt/server/plugins/PluginInventories.hpp>

#include <functional>

namespace basalt::server::runtime {
class VanillaGameData;
}

namespace basalt::server::runtime::plugins {

class ServerInventories final : public server::plugins::PluginInventories {
public:
    explicit ServerInventories(std::function<VanillaGameData&()> data) : data_(std::move(data)) {}

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> itemData(std::string_view id) const override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> enchantmentData(std::string_view id) const override;

    [[nodiscard]] std::vector<std::string> enchantmentIds() const override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> craft(std::int32_t width, std::int32_t height,
                                                                 std::span<const std::uint8_t> items) const override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> mobEffects() const override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> potions() const override;

    [[nodiscard]] std::vector<std::string> recipeIds() const override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> recipeData(std::string_view id) const override;

    bool addRecipe(std::string_view id, std::span<const std::uint8_t> data) override;

    bool addRecipeJson(std::string_view id, std::string_view json) override;

    bool removeRecipe(std::string_view id) override;

    void clearRecipes() override;

    void resetRecipes() override;

    [[nodiscard]] std::vector<std::string> registryTags(std::string_view registry) const override;

private:
    std::function<VanillaGameData&()> data_;
};

} // namespace basalt::server::runtime::plugins
