#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/BlockTests.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace structure = server::world::structure;

using structure::StructurePos;
using structure::StructureRotation;
using wire::text::Component;

[[nodiscard]] StructureRotation rotationOf(const std::string& name) {
    if (name == "clockwise_90") {
        return StructureRotation::Clockwise90;
    }
    if (name == "180") {
        return StructureRotation::Clockwise180;
    }
    if (name == "counterclockwise_90") {
        return StructureRotation::Counterclockwise90;
    }
    return StructureRotation::None;
}

[[nodiscard]] std::string_view modeName(const std::string_view mode) { return mode; }

[[nodiscard]] Component modeDisplay(const std::string_view mode) {
    return Component::translatable("test_block.mode." + std::string{modeName(mode)});
}

} // namespace

BlockTests::BlockTests(world::StructureTemplates& templates, BlockChangeBroadcaster& broadcaster,
                       player::PlayerRegistry& players)
    : templates_(&templates), broadcaster_(&broadcaster), players_(&players) {}

std::optional<std::string> BlockTests::start(world::LoadedWorld& world, const world::TestInstance& instance,
                                             const std::array<std::int32_t, 3>& origin, Reporter reporter,
                                             const Placed& placed) {
    if (!instance.structure.has_value()) {
        return "the test names no structure";
    }
    const structure::StructureTemplate* found = templates_->find(*instance.structure);
    if (found == nullptr) {
        return "there is no structure " + instance.structure->toString();
    }
    const StructureRotation rotation = rotationOf(instance.rotation);
    const StructurePos at{origin[0], origin[1], origin[2]};
    const StructurePos pivot{0, 0, 0};
    const structure::StructureBoundingBox box =
        found->getBoundingBox(at, rotation, pivot, structure::StructureMirror::None);
    for (std::int32_t chunkX = box.minX >> 4; chunkX <= (box.maxX >> 4); chunkX++) {
        for (std::int32_t chunkZ = box.minZ >> 4; chunkZ <= (box.maxZ >> 4); chunkZ++) {
            if (world.state().chunkIfLoaded(chunkX, chunkZ) == nullptr) {
                return "the chunks around " + std::to_string(origin[0]) + ", " + std::to_string(origin[2]) +
                       " are not loaded";
            }
        }
    }
    for (std::int32_t chunkX = box.minX >> 4; chunkX <= (box.maxX >> 4); chunkX++) {
        for (std::int32_t chunkZ = box.minZ >> 4; chunkZ <= (box.maxZ >> 4); chunkZ++) {
            found->placeInto(*world.state().chunkIfLoaded(chunkX, chunkZ),
                             structure::TemplatePlacement{.position = at,
                                                          .rotation = rotation,
                                                          .pivot = pivot,
                                                          .clipTo = structure::StructureBoundingBox{
                                                              chunkX << 4, box.minY, chunkZ << 4, (chunkX << 4) + 15,
                                                              box.maxY, (chunkZ << 4) + 15}});
        }
    }
    if (placed) {
        placed(box);
    }
    runs_.push_back(
        Run{.instance = instance, .worldName = world.name(), .box = box, .ticks = 0, .reporter = std::move(reporter)});
    return std::nullopt;
}

std::vector<std::array<std::int32_t, 3>> BlockTests::blocksIn(world::LoadedWorld& world, const Run& run,
                                                              const Mode mode) const {
    static constexpr std::array<std::string_view, 4> kModes{"start", "log", "fail", "accept"};
    const std::string_view wanted = kModes[static_cast<std::size_t>(mode)];
    std::vector<std::array<std::int32_t, 3>> found;
    for (std::int32_t x = run.box.minX; x <= run.box.maxX; x++) {
        for (std::int32_t y = run.box.minY; y <= run.box.maxY; y++) {
            for (std::int32_t z = run.box.minZ; z <= run.box.maxZ; z++) {
                const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
                if (!stateId.has_value()) {
                    continue;
                }
                const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(*stateId);
                if (block == nullptr || block->path() != "test_block") {
                    continue;
                }
                const auto properties = server::registry::blockStateDefinitions::propertiesOf(*stateId);
                const auto kind = properties.find("mode");
                if (kind != properties.end() && kind->second == wanted) {
                    found.push_back({x, y, z});
                }
            }
        }
    }
    return found;
}

void BlockTests::powerStart(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
    if (entity == nullptr) {
        return;
    }
    // parity: `TestBlockEntity.trigger` for a start block: powered, neighbours told, and its message logged.
    entity->data().putBoolean("powered", true);
    chunk->markDirty();
    std::vector<player::PlayerSession*> watchers;
    for (player::PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    broadcaster_->announceRedstone(
        world, watchers,
        world::BlockChange{
            .x = at[0], .y = at[1], .z = at[2], .stateId = world.state().blockState(at[0], at[1], at[2]).value_or(0)});
    const std::string message{entity->data().getString("message")};
    if (!message.empty()) {
        wire::common::Logger{"Test"}.info([&] {
            return "Test start (at " + std::to_string(at[0]) + ", " + std::to_string(at[1]) + ", " +
                   std::to_string(at[2]) + "): " + message;
        });
    }
}

void BlockTests::finish(Run& run, const bool passed, std::optional<Component> failure) {
    finished_.push_back(
        Report{.id = run.instance.id, .passed = passed, .ticks = run.ticks, .failure = std::move(failure)});
    run.ticks = -1;
}

void BlockTests::tick(world::LoadedWorld& world) {
    for (Run& run : runs_) {
        if (run.worldName != world.name() || run.ticks < 0) {
            continue;
        }
        run.ticks++;
        const std::int32_t setup = run.instance.setupTicks;
        if (run.ticks == setup + 1) {
            // parity: `BlockBasedTestInstance.run` finds the one start block and triggers it.
            const auto starts = blocksIn(world, run, Mode::Start);
            if (starts.empty()) {
                finish(run, false, Component::translatable("test_block.error.missing", {modeDisplay("start")}));
            } else if (starts.size() != 1) {
                finish(run, false, Component::translatable("test_block.error.too_many", {modeDisplay("start")}));
            } else {
                powerStart(world, starts.front());
            }
            continue;
        }
        if (run.ticks > setup + 1) {
            const auto accepts = blocksIn(world, run, Mode::Accept);
            if (accepts.empty()) {
                finish(run, false, Component::translatable("test_block.error.missing", {modeDisplay("accept")}));
                continue;
            }
            const bool accepted = std::ranges::any_of(
                accepts, [&](const auto& at) { return world.state().testTriggered(at[0], at[1], at[2]); });
            if (accepted) {
                finish(run, true, std::nullopt);
                continue;
            }
            bool failed = false;
            for (const auto& at : blocksIn(world, run, Mode::Fail)) {
                if (!world.state().testTriggered(at[0], at[1], at[2])) {
                    continue;
                }
                server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
                const server::world::ChunkBlockEntity* entity =
                    chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
                finish(run, false,
                       Component::text(entity == nullptr ? std::string{}
                                                         : std::string{entity->data().getString("message")}));
                failed = true;
                break;
            }
            if (failed) {
                continue;
            }
            for (const auto& at : blocksIn(world, run, Mode::Log)) {
                world.state().setTestTriggered(at[0], at[1], at[2], false);
            }
        }
        if (run.ticks > run.instance.maxTicks) {
            finish(run, false, Component::text("Timed out after " + std::to_string(run.instance.maxTicks) + " ticks"));
        }
    }
    std::vector<Report> done;
    done.swap(finished_);
    for (const Report& report : done) {
        const auto owner =
            std::ranges::find_if(runs_, [&](const Run& run) { return run.ticks < 0 && run.instance.id == report.id; });
        if (owner != runs_.end()) {
            owner->reporter(report);
        }
    }
    std::erase_if(runs_, [](const Run& run) { return run.ticks < 0; });
}

} // namespace basalt::server::runtime
