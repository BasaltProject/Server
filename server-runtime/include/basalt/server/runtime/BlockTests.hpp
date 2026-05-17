#pragma once

#include <basalt/server/runtime/world/RealTestEnvironments.hpp>
#include <basalt/server/runtime/world/RealTestInstances.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime {

class BlockChangeBroadcaster;

namespace player {
class PlayerRegistry;
class PlayerSession;
} // namespace player

namespace world {
class LoadedWorld;
}

namespace command {
class CommandEnvironment;
}

/// parity: `BlockBasedTestInstance` run by `GameTestRunner`: a `test_instance_block`, its template with test blocks
/// inside a ring of barriers, started and watched per tick.
class BlockTests {
public:
    struct Report {
        wire::common::Identifier id;
        bool passed = false;
        bool required = true;
        std::int32_t ticks = 0;
        std::optional<wire::text::Component> failure;
    };

    using Reporter = std::function<void(const Report&)>;
    using Placed = std::function<void(const server::world::structure::StructureBoundingBox&)>;

    BlockTests(world::StructureTemplates& templates, const world::RealTestInstances& instances,
               const world::RealTestEnvironments& environments, const command::CommandEnvironment& environment,
               world::RealTestEnvironments::RunFunction runFunction, BlockChangeBroadcaster& broadcaster,
               player::PlayerRegistry& players);

    /// Puts a test instance block at `blockAt` with the template beside it and starts counting; why it cannot, if so.
    [[nodiscard]] std::optional<std::string> start(world::LoadedWorld& world, const world::TestInstance& instance,
                                                   const std::array<std::int32_t, 3>& blockAt, Reporter reporter,
                                                   const Placed& placed);

    void tick(world::LoadedWorld& world);

    [[nodiscard]] bool running() const noexcept { return !runs_.empty(); }

    /// Forgets every run without a report. parity: `/test stop`.
    void stopAll();

    /// Empties the space of every test instance block within `radius` of a point and answers how many.
    /// parity: `/test clearall`.
    [[nodiscard]] std::int32_t clearAll(world::LoadedWorld& world, std::int32_t centerX, std::int32_t centerZ,
                                        std::int32_t radius);

    struct Found {
        wire::common::Identifier test;
        std::array<std::int32_t, 3> at{};
    };

    /// The test instance blocks in the loaded chunks within `radius` of a point, nearest first.
    [[nodiscard]] std::vector<Found> find(world::LoadedWorld& world, std::int32_t centerX, std::int32_t centerY,
                                          std::int32_t centerZ, std::int32_t radius) const;

    /// Starts the test a block already holds again, in place.
    [[nodiscard]] std::optional<std::string> restart(world::LoadedWorld& world, const Found& block, Reporter reporter,
                                                     const Placed& placed);

    /// Puts the test back as its template has it and clears the block's status. parity: `resetTest`.
    [[nodiscard]] std::optional<std::string> reset(world::LoadedWorld& world, const Found& block, const Placed& placed);

    /// Puts an empty test instance block of the given size at `blockAt`. parity: `/test create`.
    [[nodiscard]] std::optional<std::string> create(world::LoadedWorld& world, const wire::common::Identifier& id,
                                                    const std::array<std::int32_t, 3>& blockAt,
                                                    const std::array<std::int32_t, 3>& size, const Placed& placed);

private:
    struct Layout {
        server::world::structure::StructureBoundingBox structure;
        server::world::structure::StructureBoundingBox test;
        std::array<std::int32_t, 3> startCorner{};
    };

    struct Run {
        world::TestInstance instance;
        std::string worldName;
        std::array<std::int32_t, 3> blockAt{};
        std::array<std::int32_t, 3> size{1, 1, 1};
        Layout layout;
        std::int32_t ticks = 0;
        Reporter reporter;
        std::string environmentKey;
    };

    enum class Mode : std::uint8_t { Start, Log, Fail, Accept };

    [[nodiscard]] Layout layoutOf(const std::array<std::int32_t, 3>& blockAt, const std::array<std::int32_t, 3>& size,
                                  const std::string& rotation, std::int32_t padding) const;
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> blocksIn(world::LoadedWorld& world, const Run& run,
                                                                    Mode mode) const;
    /// Clears the space, puts the template and the block there; the layout it used, or why it could not.
    [[nodiscard]] std::optional<std::string> place(world::LoadedWorld& world, const world::TestInstance& instance,
                                                   const std::array<std::int32_t, 3>& blockAt, Layout& layout,
                                                   bool block);
    void enter(world::LoadedWorld& world, Run& run);
    void leave(Run& run);
    void finish(Run& run, bool passed, std::optional<wire::text::Component> failure);
    void powerStart(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);
    void setBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);
    void encase(world::LoadedWorld& world, const Layout& layout, bool skyAccess, bool barriers);
    void clearSpace(world::LoadedWorld& world, const server::world::structure::StructureBoundingBox& box);
    void writeBlockEntity(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                          const wire::common::Identifier& test, const std::array<std::int32_t, 3>& size,
                          const std::string& rotation, const std::string& status,
                          const std::optional<wire::text::Component>& error);
    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    world::StructureTemplates* templates_;
    const world::RealTestInstances* instances_;
    const world::RealTestEnvironments* environments_;
    const command::CommandEnvironment* environment_;
    world::RealTestEnvironments::RunFunction runFunction_;

    struct Active {
        std::function<void()> undo;
        std::int32_t users = 0;
    };

    std::map<std::string, Active> active_;
    BlockChangeBroadcaster* broadcaster_;
    player::PlayerRegistry* players_;
    std::vector<Run> runs_;
    std::vector<Report> finished_;
};

} // namespace basalt::server::runtime
