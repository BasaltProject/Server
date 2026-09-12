#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::runtime::bedrock {

/// One column of the world, written as a Bedrock client reads it.
///
/// Called where the chunk lives and nowhere else: a chunk belongs to its region's owner, and what
/// crosses to the door's own thread is the finished bytes rather than the chunk.
[[nodiscard]] std::vector<std::uint8_t> writeColumn(const server::world::Chunk& chunk, int protocol);

} // namespace basalt::server::runtime::bedrock
