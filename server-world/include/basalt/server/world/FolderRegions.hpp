#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::world::folderRegions {

[[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& folder, std::int32_t chunkX,
                                                            std::int32_t chunkZ);

void write(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t> data,
           std::int32_t timestamp);

[[nodiscard]] std::int32_t timestamp(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ);

/// Writes the regions of the folder that changed; false when a file could not be written.
bool flush(const std::string& folder);

/// Flushes and forgets the folder.
bool release(const std::string& folder);

} // namespace basalt::server::world::folderRegions
