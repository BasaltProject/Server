#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::entity::entityNbt::detail {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

inline constexpr std::array<std::string_view, 6> kPatrollingTypes{"pillager",   "vindicator", "evoker",
                                                                  "illusioner", "witch",      "ravager"};

[[nodiscard]] inline bool isArrow(const Identifier& type) {
    return type.namespaceName() == Identifier::kVanillaNamespace &&
           (type.path() == "arrow" || type.path() == "spectral_arrow" || type.path() == "trident");
}

[[nodiscard]] inline bool patrols(std::string_view path) {
    return std::ranges::find(kPatrollingTypes, path) != kPatrollingTypes.end();
}

[[nodiscard]] inline NbtTag doubles(double first, double second, double third) {
    NbtList list{NbtType::Double};
    list.push(NbtTag::ofDouble(first));
    list.push(NbtTag::ofDouble(second));
    list.push(NbtTag::ofDouble(third));
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] inline NbtTag floats(float first, float second) {
    NbtList list{NbtType::Float};
    list.push(NbtTag::ofFloat(first));
    list.push(NbtTag::ofFloat(second));
    return NbtTag::ofList(std::move(list));
}

/// A reference, not a copy: a `string_view` off a returned temporary would dangle.
[[nodiscard]] inline const NbtCompound& compoundOf(const NbtCompound& tag, std::string_view key) {
    static const NbtCompound empty;
    const NbtCompound* found = tag.getCompound(key);
    return found == nullptr ? empty : *found;
}

/// parity: `UUIDUtil.CODEC`: four ints, high word first; a string there loses every uuid.
[[nodiscard]] inline NbtTag uuidTag(const wire::common::Uuid& uuid) {
    return NbtTag::ofIntArray({static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high() >> 32)),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high())),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low() >> 32)),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low()))});
}

} // namespace basalt::server::runtime::entity::entityNbt::detail
