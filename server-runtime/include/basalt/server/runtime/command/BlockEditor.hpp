#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::server::runtime::command {

class BlockEditor {
public:
    BlockEditor(const CommandEnvironment& environment, world::LoadedWorld& world);

    [[nodiscard]] world::LoadedWorld& world() const noexcept { return *world_; }

    [[nodiscard]] std::int32_t minY() const noexcept { return bounds_.minY(); }

    [[nodiscard]] std::int32_t maxY() const noexcept { return bounds_.maxY(); }

    [[nodiscard]] std::optional<std::int32_t> stateAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// parity: `BlockPosArgument.getLoadedBlockPos`.
    [[nodiscard]] std::optional<CommandSyntaxError> require(std::int32_t x, std::int32_t y, std::int32_t z) const;

    bool set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// parity: `Level.destroyBlock(pos, true)`.
    bool destroy(std::int32_t x, std::int32_t y, std::int32_t z);

    /// parity: `BlockInput.place`.
    void keepWritten(std::int32_t x, std::int32_t y, std::int32_t z, std::map<std::string, std::string> properties);

    void flush();

    /// parity: `/setblock ... strict` and `/fill ... strict`.
    void writeStrictly() noexcept { strict_ = true; }

private:
    bool write(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               world::WorldState::SideEffects sideEffects);

    void record(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// parity: `BlockInput.place`'s `overwriteWithDefinedProperties`.
    void keepNamedProperties(const std::array<std::int32_t, 3>& at);

    /// parity: `BaseRailBlock.onPlace`, the one `onPlace` a command has to run itself.
    void bendRail(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  std::optional<std::int32_t> before);

    /// parity: `Level.setBlock`.
    void createBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// True when the written block, what it replaced and all 26 around it are blocks no update reacts to.
    [[nodiscard]] bool reactsToNothing(const std::array<std::int32_t, 3>& at);

    [[nodiscard]] bool sectionAllInert(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ) const;

    void prepareInertCells(const std::vector<std::array<std::int32_t, 3>>& positions);

    void notifyNeighbours();

    bool strict_ = false;
    std::map<std::array<std::int32_t, 3>, std::map<std::string, std::string>> written_properties_;
    std::map<std::array<std::int32_t, 3>, std::optional<std::int32_t>> replaced_;
    /// parity: `destroy`.
    std::set<std::array<std::int32_t, 3>> destroyed_;

    const CommandEnvironment* environment_;
    world::LoadedWorld* world_;
    server::world::ChunkHeight bounds_;
    std::map<std::int64_t, std::map<std::int16_t, std::int32_t>> changesBySection_;
    /// parity: `/setblock` and `/fill`.
    std::vector<std::array<std::int32_t, 3>> written_;
    std::vector<world::BlockChange> effects_;

    struct SectionInertness {
        const server::world::ChunkSection* section = nullptr;
        std::uint64_t version = 0;
        bool inert = false;
    };

    mutable std::unordered_map<std::int64_t, SectionInertness> sectionInertness_;
    /// 0 unknown, 1 inert, 2 not, over the box a notify pass touches; dropped whenever a block changes.
    std::vector<std::uint8_t> inertCells_;
    std::array<std::int32_t, 3> inertCellsMin_{};
    std::array<std::size_t, 3> inertCellsSize_{};
    std::uint64_t inertCellsVersion_ = 0;
};

[[nodiscard]] std::expected<BlockEditor, CommandSyntaxError> blocksOf(const CommandContext& context);

} // namespace basalt::server::runtime::command
