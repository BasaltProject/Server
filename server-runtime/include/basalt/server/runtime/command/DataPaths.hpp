#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// NBT paths, the part of vanilla's syntax a datapack actually leans on: dotted keys, `list[index]`
/// and the compound filter `list[{k:v}]`.
///
/// Shared by `/data` and by `/execute if data`, so both read a path the same way.
namespace basalt::server::runtime::command::dataPaths {

/// One step of a path: a key, a list index, or a filter over a list's elements.
struct Step {
    enum class Kind : std::uint8_t { Key, Index, Filter };

    Kind kind = Kind::Key;
    std::string name;
    std::int32_t index = 0;
    wire::nbt::NbtCompound filter;
};

/// The steps of a path, or nothing when it is malformed.
[[nodiscard]] std::optional<std::vector<Step>> steps(std::string_view path);

[[nodiscard]] wire::nbt::NbtCompound merge(const wire::nbt::NbtCompound& base, const wire::nbt::NbtCompound& incoming);

/// The tag at a path, or nothing when the path leads nowhere.
[[nodiscard]] std::optional<wire::nbt::NbtTag> resolve(const wire::nbt::NbtTag& root, std::string_view path);

/// The root with `value` written at the path, or nothing when the path is not writable: only a
/// chain of keys can be written to.
[[nodiscard]] std::optional<wire::nbt::NbtCompound> set(const wire::nbt::NbtCompound& root, std::string_view path,
                                                        const wire::nbt::NbtTag& value);

/// The root without whatever the path names, or nothing when there was nothing there.
[[nodiscard]] std::optional<wire::nbt::NbtCompound> remove(const wire::nbt::NbtCompound& root, std::string_view path);

} // namespace basalt::server::runtime::command::dataPaths
