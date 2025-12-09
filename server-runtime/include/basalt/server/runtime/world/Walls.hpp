#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world {

/// How a wall stands: which sides it reaches out to, how high, and whether it keeps its post.
///
/// parity: `WallBlock.updateShape` over `updateSides` and `shouldRaisePost`. The two questions about
/// what sits on top are shape tests, and they are asked of the real shapes: a wall under a full
/// block grows its sides tall, and a wall in a straight line loses its post unless something covers
/// the middle.
namespace walls {

/// The four sides and the post, as the properties a wall state carries, or nothing when the block is
/// not a wall at all.
[[nodiscard]] std::optional<std::map<std::string, std::string>> shapeOf(std::int32_t stateId, std::int32_t north,
                                                                        std::int32_t south, std::int32_t west,
                                                                        std::int32_t east, std::int32_t above,
                                                                        bool aboveOverridesPost = false);

} // namespace walls

} // namespace basalt::server::runtime::world
