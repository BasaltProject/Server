#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::command::dataPaths {

struct Step {
    enum class Kind : std::uint8_t { Key, Index, Filter };

    Kind kind = Kind::Key;
    std::string name;
    std::int32_t index = 0;
    wire::nbt::NbtCompound filter;
};

[[nodiscard]] std::optional<std::vector<Step>> steps(std::string_view path);

[[nodiscard]] wire::nbt::NbtCompound merge(const wire::nbt::NbtCompound& base, const wire::nbt::NbtCompound& incoming);

[[nodiscard]] std::optional<wire::nbt::NbtTag> resolve(const wire::nbt::NbtTag& root, std::string_view path);

[[nodiscard]] std::optional<wire::nbt::NbtCompound> set(const wire::nbt::NbtCompound& root, std::string_view path,
                                                        const wire::nbt::NbtTag& value);

[[nodiscard]] std::optional<wire::nbt::NbtCompound> remove(const wire::nbt::NbtCompound& root, std::string_view path);

} // namespace basalt::server::runtime::command::dataPaths
