#pragma once

#include <basalt/server/runtime/world/RealTestInstances.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime {

class BlockChangeBroadcaster;

namespace player {
class PlayerRegistry;
}

namespace world {
class LoadedWorld;
}

/// parity: `BlockBasedTestInstance` run by `GameTestRunner`: a template with test blocks, started and watched per tick.
class BlockTests {
public:
    struct Report {
        wire::common::Identifier id;
        bool passed = false;
        std::int32_t ticks = 0;
        std::optional<wire::text::Component> failure;
    };

    using Reporter = std::function<void(const Report&)>;
    using Placed = std::function<void(const server::world::structure::StructureBoundingBox&)>;

    BlockTests(world::StructureTemplates& templates, BlockChangeBroadcaster& broadcaster,
               player::PlayerRegistry& players);

    [[nodiscard]] std::optional<std::string> start(world::LoadedWorld& world, const world::TestInstance& instance,
                                                   const std::array<std::int32_t, 3>& origin, Reporter reporter,
                                                   const Placed& placed);

    void tick(world::LoadedWorld& world);

    [[nodiscard]] bool running() const noexcept { return !runs_.empty(); }

    /// Forgets every run without a report. parity: `/test stop`.
    void stopAll() { runs_.clear(); }

private:
    struct Run {
        world::TestInstance instance;
        std::string worldName;
        server::world::structure::StructureBoundingBox box;
        std::int32_t ticks = 0;
        Reporter reporter;
    };

    enum class Mode : std::uint8_t { Start, Log, Fail, Accept };

    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> blocksIn(world::LoadedWorld& world, const Run& run,
                                                                    Mode mode) const;
    void finish(Run& run, bool passed, std::optional<wire::text::Component> failure);
    void powerStart(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    world::StructureTemplates* templates_;
    BlockChangeBroadcaster* broadcaster_;
    player::PlayerRegistry* players_;
    std::vector<Run> runs_;
    std::vector<Report> finished_;
};

} // namespace basalt::server::runtime
