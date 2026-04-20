#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::item {

class ItemDamageEffects {
public:
    using DefinitionOf = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

    using ItemInTag = std::function<bool(std::string_view, const wire::common::Identifier&)>;

    explicit ItemDamageEffects(DefinitionOf definitionOf, ItemInTag itemInTag = {});

    [[nodiscard]] std::int32_t process(const storage::StoredItemStack& stack, std::int32_t amount,
                                       random::RandomSource& random) const;

private:
    DefinitionOf definitionOf_;
    ItemInTag itemInTag_;
};

} // namespace basalt::server::runtime::item
