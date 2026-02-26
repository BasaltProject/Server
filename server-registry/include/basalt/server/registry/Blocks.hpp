#pragma once

#include <basalt/server/registry/Registry.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::registry {

/// A single block state, identified by its vanilla network state id.
struct BlockState {
    wire::common::Identifier block;
    std::int32_t stateId = 0;
    std::map<std::string, std::string> properties;

    friend bool operator==(const BlockState&, const BlockState&) = default;
};

/// How long a block takes to break and whether it drops anything without the right tool.
///
/// A `destroyTime` of zero means it comes apart instantly, which matters more than it sounds: a
/// client destroys those on the spot and only ever sends the *start* of the dig, never the end, so a
/// server that waits for the end never breaks a flower. Below zero means unbreakable.
struct BlockHardness {
    float destroyTime = 0.0F;
    bool requiresCorrectTool = false;
    /// parity: `Block.getExplosionResistance`, what an explosion has to spend to get through.
    float explosionResistance = 0.0F;

    [[nodiscard]] bool isInstant() const noexcept { return destroyTime == 0.0F; }

    [[nodiscard]] bool isUnbreakable() const noexcept { return destroyTime < 0.0F; }
};

/// Every block's default state, straight from the real 26.2 server jar's own data generator:
/// authoritative state ids, not decompiled or guessed.
namespace blocks {

[[nodiscard]] const Registry<BlockState>& registry();

/// This block's `minecraft:block` registry id, or absent if it is not a known block.
///
/// This is **not** a block state id and must never be written into the world or sent as one. The two
/// numbering spaces look alike and are not: `redstone_block` is block 475 and state 11311, and state
/// 475 belongs to `pale_oak_leaves`. For a state id use `defaultStateIdOf`.
[[nodiscard]] std::optional<std::int32_t> blockIdOf(const wire::common::Identifier& block);

/// This block's default block **state** id, the number the world and the protocol both speak.
[[nodiscard]] std::optional<std::int32_t> defaultStateIdOf(const wire::common::Identifier& block);

/// This block's destroy time and tool requirement, or absent for one this build's table does not
/// list.
[[nodiscard]] const BlockHardness* hardnessOf(const wire::common::Identifier& block);

/// The block state with this id, whatever it is.
///
/// A registry entry is one block's default state and carries no properties with it, so a question
/// about a property (`/execute if block ... redstone_torch[lit=true]`) would be answered "no" for
/// every block sitting in its own default state. The definition has them all, so it answers first
/// wherever it has something to say.
[[nodiscard]] std::optional<BlockState> byStateId(std::int32_t stateId);

/// Resolves a block plus its serialized vanilla properties back to a network state id.
///
/// parity: `BlockStateParser`, which reads a partial state the same way: whatever is left unsaid
/// keeps the default state's value.
[[nodiscard]] std::optional<std::int32_t> stateIdOf(const wire::common::Identifier& block,
                                                    const std::map<std::string, std::string>& properties);

/// Number of ids vanilla's direct block-state palette needs.
[[nodiscard]] std::int32_t stateCount();

/// parity: BlockState.isAir, including the two non-default air blocks worldgen uses.
[[nodiscard]] bool isAir(std::int32_t stateId);

/// The handful of blocks named for readability. Everything else is looked up by name.
[[nodiscard]] const BlockState& air();
[[nodiscard]] const BlockState& stone();
[[nodiscard]] const BlockState& water();
[[nodiscard]] const BlockState& lava();
[[nodiscard]] const BlockState& bedrock();
[[nodiscard]] const BlockState& voidAir();
[[nodiscard]] const BlockState& caveAir();

/// One named block by its path, for the rest of the constants the Kotlin spelled out one by one.
[[nodiscard]] const BlockState* named(std::string_view path);

} // namespace blocks

} // namespace basalt::server::registry
