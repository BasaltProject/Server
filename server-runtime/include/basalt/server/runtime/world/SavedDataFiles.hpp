#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <optional>
#include <string_view>

namespace basalt::server::runtime::world::savedData {

/// The `data` of `<root>/<name>.nbt`, else that of the gzipped file a vanilla world copied in keeps it in, brought up
/// to date. `dimension` is the path of the dimension a per-dimension file such as raids belongs to, like `overworld`.
[[nodiscard]] std::optional<wire::nbt::NbtCompound> read(std::string_view worldRoot, std::string_view name,
                                                         std::string_view dimension = {});

/// Writes `data` as `<root>/<name>.nbt` under the current data version; false if the file could not be written.
bool write(std::string_view worldRoot, std::string_view name, wire::nbt::NbtCompound data);

} // namespace basalt::server::runtime::world::savedData
