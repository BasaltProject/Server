#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace basalt::server::runtime::command {

/// Block writes on behalf of a command, batched so one `/fill` is not one packet per block: changes
/// are collected per section and flushed as `block_update` or `section_blocks_update`, the same split
/// vanilla's chunk tracker makes.
///
/// Only loaded chunks are written and `require` refuses anything outside one, which is vanilla's rule
/// and keeps a `/fill` off the generation path.
class BlockEditor {
public:
    BlockEditor(const CommandEnvironment& environment, world::LoadedWorld& world);

    [[nodiscard]] world::LoadedWorld& world() const noexcept { return *world_; }

    [[nodiscard]] std::int32_t minY() const noexcept { return bounds_.minY(); }

    [[nodiscard]] std::int32_t maxY() const noexcept { return bounds_.maxY(); }

    /// The state at this position, or nothing when its chunk is not loaded.
    [[nodiscard]] std::optional<std::int32_t> stateAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// parity: `BlockPosArgument.getLoadedBlockPos`, which refuses an unloaded position by name.
    [[nodiscard]] std::optional<CommandSyntaxError> require(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// Writes one block, remembering it for `flush`. False when nothing changed or the chunk is
    /// gone.
    bool set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// Sends everything written since the last flush to whoever is in this world.
    void flush();

    /// parity: `BlockInput.place` runs `Block.updateFromNeighbourShapes` on the state unless the
    /// caller asked for `strict`, which is why `/setblock` of a dust beside a dust draws a line and
    /// not a stub. Say this before writing to get the `strict` behaviour, which places what is
    /// written and nothing else.
    void placeExactly() noexcept { adjustToNeighbours_ = false; }

private:
    /// parity: `Level.setBlock` gives a block its block entity as it lands, so `/setblock` of a
    /// chest leaves something `/data` can read. This build creates them where they are used instead,
    /// which left a command-placed container with nowhere to put anything.
    void createBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// Lets what was written act on what was already there, which is what makes a command build
    /// work.
    void notifyNeighbours();

    bool adjustToNeighbours_ = true;

    const CommandEnvironment* environment_;
    world::LoadedWorld* world_;
    server::world::ChunkHeight bounds_;
    std::map<std::int64_t, std::map<std::int16_t, std::int32_t>> changesBySection_;
    /// Where blocks were written, so their neighbours can be told once the whole edit is done.
    ///
    /// parity: `/setblock` and `/fill` both write with `Block.UPDATE_ALL`, so a redstone block
    /// placed by command powers what is next to it and a door beside it swings. Doing it after the
    /// writes rather than during them is what stops a `/fill` reacting to a structure it is still
    /// building.
    std::vector<std::array<std::int32_t, 3>> written_;
    std::vector<world::BlockChange> effects_;
};

/// A `BlockEditor` over the world this command acts on.
[[nodiscard]] std::expected<BlockEditor, CommandSyntaxError> blocksOf(const CommandContext& context);

} // namespace basalt::server::runtime::command
