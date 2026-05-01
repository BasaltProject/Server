#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <optional>
#include <string_view>

namespace basalt::server::runtime::world::savedData {

/// The `data` of `<root>/<name>.nbt`, else of the gzipped `<root>/data/minecraft/<name>.dat` of a vanilla world copied
/// in.
[[nodiscard]] std::optional<wire::nbt::NbtCompound> read(std::string_view worldRoot, std::string_view name);

/// Writes `data` as `<root>/<name>.nbt` under the current data version; false if the file could not be written.
bool write(std::string_view worldRoot, std::string_view name, wire::nbt::NbtCompound data);

} // namespace basalt::server::runtime::world::savedData
