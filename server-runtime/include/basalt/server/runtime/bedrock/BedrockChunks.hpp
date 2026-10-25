#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::runtime::bedrock {

[[nodiscard]] std::vector<std::uint8_t> writeColumn(const server::world::Chunk& chunk, int protocol,
                                                    const std::vector<std::vector<std::int32_t>>& replacements = {});

} // namespace basalt::server::runtime::bedrock
