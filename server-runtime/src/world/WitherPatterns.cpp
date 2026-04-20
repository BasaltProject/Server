#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/WitherPatterns.hpp>

#include <array>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world::witherPatterns {
namespace {

using server::registry::BlockFace;

enum class Wants : std::uint8_t { Skull, Base, Air, Any };

constexpr std::int32_t kSize = 3;

constexpr std::array<Wants, 9> kCells{Wants::Skull, Wants::Skull, Wants::Skull, Wants::Base, Wants::Base,
                                      Wants::Base,  Wants::Air,   Wants::Base,  Wants::Air};

} // namespace

bool isWitherSkull(std::int32_t stateId) {
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    return path == "wither_skeleton_skull" || path == "wither_skeleton_wall_skull";
}

[[nodiscard]] bool matchesCells(const WorldState& world, const IsBase& isBase, const blockPatterns::Cell& origin,
                                BlockFace forwards, BlockFace up, const std::array<Wants, 9>& cells) {
    // parity: `BlockPattern.matches`, the columns outermost.
    for (std::int32_t right = 0; right < kSize; ++right) {
        for (std::int32_t down = 0; down < kSize; ++down) {
            const Wants wants = cells[static_cast<std::size_t>((down * kSize) + right)];
            if (wants == Wants::Any) {
                continue;
            }
            const blockPatterns::Cell at = blockPatterns::translateAndRotate(origin, forwards, up, right, down, 0);
            const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
            if (!state.has_value()) {
                return false;
            }
            const bool ok = wants == Wants::Skull  ? isWitherSkull(*state)
                            : wants == Wants::Base ? isBase && isBase(*state)
                                                   : server::registry::blocks::isAir(*state);
            if (!ok) {
                return false;
            }
        }
    }
    return true;
}

bool canComplete(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const IsBase& isBase) {
    // parity: the base pattern is the full one with its skull row left open.
    std::array<Wants, 9> base = kCells;
    base[0] = Wants::Any;
    base[1] = Wants::Any;
    base[2] = Wants::Any;
    const auto matches = [&world, &isBase, &base](const blockPatterns::Cell& origin, BlockFace forwards, BlockFace up) {
        return matchesCells(world, isBase, origin, forwards, up, base);
    };
    return blockPatterns::find({x, y, z}, kSize, matches).has_value();
}

std::optional<Match> find(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                          const IsBase& isBase) {
    const auto matches = [&world, &isBase](const blockPatterns::Cell& origin, BlockFace forwards, BlockFace up) {
        return matchesCells(world, isBase, origin, forwards, up, kCells);
    };
    const std::optional<blockPatterns::Found> found = blockPatterns::find({x, y, z}, kSize, matches);
    if (!found.has_value()) {
        return std::nullopt;
    }
    Match match;
    for (std::int32_t right = 0; right < kSize; ++right) {
        for (std::int32_t down = 0; down < kSize; ++down) {
            match.cells.push_back(
                blockPatterns::translateAndRotate(found->origin, found->forwards, found->up, right, down, 0));
        }
    }
    match.spawn = blockPatterns::translateAndRotate(found->origin, found->forwards, found->up, 1, 2, 0);
    match.alongX = found->forwards == BlockFace::West || found->forwards == BlockFace::East;
    return match;
}

} // namespace basalt::server::runtime::world::witherPatterns
