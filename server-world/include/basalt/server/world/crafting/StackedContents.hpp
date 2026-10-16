#pragma once

#include <basalt/server/world/crafting/Crafting.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::crafting {

/// parity: `StackedContents` over item ids; items iterate in insertion order, not by identity hash.
class StackedContents {
public:
    void account(std::string_view item, std::int32_t count);

    [[nodiscard]] bool tryPick(std::span<const Ingredient* const> ingredients, std::int32_t amount,
                               std::vector<std::string>* output = nullptr);

    [[nodiscard]] std::int32_t tryPickAll(std::span<const Ingredient* const> ingredients, std::int32_t maxSize,
                                          std::vector<std::string>* output = nullptr);

private:
    friend class RecipePicker;

    std::vector<std::pair<std::string, std::int32_t>> amounts_;
};

} // namespace basalt::server::world::crafting
