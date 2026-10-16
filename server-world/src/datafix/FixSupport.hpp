#pragma once

#include <basalt/server/world/datafix/DataFixer.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::datafix {

/// `Root` runs on the whole tree whatever the domain, for fixes that find their own places.
enum class Target : std::uint8_t { Chunk, BlockEntity, ItemStack, Components, Palette, Root };

struct Fix {
    std::int32_t version = 0;
    Target target = Target::Chunk;
    std::string_view blockEntityId;
    std::function<void(wire::nbt::NbtCompound&, const Context&)> apply;
};

[[nodiscard]] std::string namespaced(std::string_view text);

void rename(wire::nbt::NbtCompound& compound, std::string_view from, std::string_view to);

void forEachCompound(wire::nbt::NbtCompound& root, std::string_view listKey,
                     const std::function<void(wire::nbt::NbtCompound&)>& edit);

/// parity: `ExtraDataFixUtils.getPlainTranslationKey`, a component that is only `{translate: key}`.
[[nodiscard]] std::string_view plainTranslationKey(const wire::nbt::NbtTag* component);

void addTextFixes(std::vector<Fix>& fixes);

void addFixesFrom4000(std::vector<Fix>& fixes);

void addFixesFrom4300(std::vector<Fix>& fixes);

void addFixesFrom4996(std::vector<Fix>& fixes);

} // namespace basalt::server::world::datafix
