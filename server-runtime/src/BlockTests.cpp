#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/BlockTests.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace structure = server::world::structure;

using structure::StructureBoundingBox;
using structure::StructurePos;
using structure::StructureRotation;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using wire::text::Component;

constexpr std::string_view kTestInstanceBlock = "minecraft:test_instance_block";

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

[[nodiscard]] Component modeDisplay(const std::string_view mode) {
    return Component::translatable("test_block.mode." + std::string{mode});
}

[[nodiscard]] std::int32_t stateOf(std::string_view block) {
    return server::registry::blocks::stateIdOf(Identifier::parse(std::string{block}).value(), {}).value_or(0);
}

} // namespace

BlockTests::BlockTests(world::StructureTemplates& templates, const world::RealTestInstances& instances,
                       BlockChangeBroadcaster& broadcaster, player::PlayerRegistry& players)
    : templates_(&templates), instances_(&instances), broadcaster_(&broadcaster), players_(&players) {}

std::vector<player::PlayerSession*> BlockTests::watchersOf(const world::LoadedWorld& world) const {
    std::vector<player::PlayerSession*> watchers;
    for (player::PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

BlockTests::Layout BlockTests::layoutOf(const std::array<std::int32_t, 3>& blockAt,
                                        const std::array<std::int32_t, 3>& size, const std::string& rotation,
                                        const std::int32_t padding) const {
    // parity: `TestInstanceBlockEntity.getStructurePos`, one up and one south of the block, past the padding.
    const std::int32_t x = blockAt[0] + padding;
    const std::int32_t y = blockAt[1] + padding + 1;
    const std::int32_t z = blockAt[2] + padding + 1;
    const StructureRotation turned = rotationOf(rotation);
    const bool switched = turned == StructureRotation::Clockwise90 || turned == StructureRotation::Counterclockwise90;
    const std::int32_t width = switched ? size[2] : size[0];
    const std::int32_t depth = switched ? size[0] : size[2];
    Layout layout;
    layout.structure = StructureBoundingBox{x, y, z, x + width - 1, y + size[1] - 1, z + depth - 1};
    layout.test = StructureBoundingBox{layout.structure.minX - padding, layout.structure.minY - padding,
                                       layout.structure.minZ - padding, layout.structure.maxX + padding,
                                       layout.structure.maxY + padding, layout.structure.maxZ + padding};
    // parity: `TestInstanceBlockEntity.getStartCorner`.
    switch (turned) {
    case StructureRotation::None:
        layout.startCorner = {x, y, z};
        break;
    case StructureRotation::Clockwise90:
        layout.startCorner = {x + size[2] - 1, y, z};
        break;
    case StructureRotation::Clockwise180:
        layout.startCorner = {x + size[0] - 1, y, z + size[2] - 1};
        break;
    case StructureRotation::Counterclockwise90:
        layout.startCorner = {x, y, z + size[0] - 1};
        break;
    }
    return layout;
}

void BlockTests::setBlock(world::LoadedWorld& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                          const std::int32_t stateId) {
    const std::optional<std::int32_t> before = world.state().blockState(x, y, z);
    if (before.has_value() && *before != stateId && world.state().setBlockState(x, y, z, stateId)) {
        broadcaster_->announceBlock(watchersOf(world), world::BlockChange{.x = x, .y = y, .z = z, .stateId = stateId});
    }
}

void BlockTests::clearSpace(world::LoadedWorld& world, const StructureBoundingBox& box) {
    // parity: `StructureUtils.clearSpaceForStructure`, which also takes the layer the barriers stand on.
    const std::int32_t air = server::registry::blocks::air().stateId;
    for (std::int32_t x = box.minX - 1; x <= box.maxX + 1; x++) {
        for (std::int32_t y = box.minY - 1; y <= box.maxY + 1; y++) {
            for (std::int32_t z = box.minZ - 1; z <= box.maxZ + 1; z++) {
                setBlock(world, x, y, z, air);
            }
        }
    }
}

void BlockTests::encase(world::LoadedWorld& world, const Layout& layout, const bool skyAccess, const bool barriers) {
    // parity: `TestInstanceBlockEntity.processStructureBoundary`: a floor, four walls and, unless the sky is open, a
    // roof.
    const std::int32_t lowX = layout.structure.minX - 1;
    const std::int32_t lowY = layout.structure.minY - 1;
    const std::int32_t lowZ = layout.structure.minZ - 1;
    const std::int32_t highX = layout.structure.maxX + 1;
    const std::int32_t highY = layout.structure.maxY + 1;
    const std::int32_t highZ = layout.structure.maxZ + 1;
    const std::int32_t barrier = stateOf("minecraft:barrier");
    const std::int32_t air = server::registry::blocks::air().stateId;
    const std::int32_t instanceBlock = stateOf(kTestInstanceBlock);
    for (std::int32_t x = lowX; x <= highX; x++) {
        for (std::int32_t y = lowY; y <= highY; y++) {
            for (std::int32_t z = lowZ; z <= highZ; z++) {
                const bool edge = x == lowX || x == highX || z == lowZ || z == highZ || y == lowY;
                const bool ceiling = y == highY && !skyAccess;
                if (!edge && !ceiling) {
                    continue;
                }
                const std::optional<std::int32_t> here = world.state().blockState(x, y, z);
                if (barriers) {
                    if (here.value_or(0) != instanceBlock) {
                        setBlock(world, x, y, z, barrier);
                    }
                } else if (here.value_or(0) == barrier) {
                    setBlock(world, x, y, z, air);
                }
            }
        }
    }
}

void BlockTests::writeBlockEntity(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                                  const Identifier& test, const std::array<std::int32_t, 3>& size,
                                  const std::string& rotation, const std::string& status,
                                  const std::optional<Component>& error) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    if (chunk == nullptr) {
        return;
    }
    // parity: `TestInstanceBlockEntity.Data.CODEC`.
    NbtCompound data;
    data.putString("test", test.toString());
    data.putIntArray("size", {size[0], size[1], size[2]});
    data.putString("rotation", rotation);
    data.putBoolean("ignore_entities", false);
    data.putString("status", status);
    if (error.has_value()) {
        data.set("error_message", wire::text::componentNbt::serialize(*error));
    }
    NbtCompound entity;
    entity.set("data", NbtTag::ofCompound(std::move(data)));
    chunk->setBlockEntity(server::world::ChunkBlockEntity{
        at[0] & 15, at[1], at[2] & 15, Identifier::parse(std::string{kTestInstanceBlock}).value(), std::move(entity)});
    chunk->markDirty();
}

std::optional<std::string> BlockTests::place(world::LoadedWorld& world, const world::TestInstance& instance,
                                             const std::array<std::int32_t, 3>& blockAt, Layout& layout,
                                             const bool block) {
    if (!instance.structure.has_value()) {
        return "the test names no structure";
    }
    const structure::StructureTemplate* found = templates_->find(*instance.structure);
    if (found == nullptr) {
        return "there is no structure " + instance.structure->toString();
    }
    const StructurePos size = found->size();
    layout = layoutOf(blockAt, {size.x, size.y, size.z}, instance.rotation, instance.padding);
    const StructureBoundingBox reach{layout.test.minX - 1, layout.test.minY - 1, layout.test.minZ - 1,
                                     layout.test.maxX + 1, layout.test.maxY + 1, layout.test.maxZ + 1};
    for (std::int32_t chunkX = std::min(reach.minX, blockAt[0]) >> 4; chunkX <= (std::max(reach.maxX, blockAt[0]) >> 4);
         chunkX++) {
        for (std::int32_t chunkZ = std::min(reach.minZ, blockAt[2]) >> 4;
             chunkZ <= (std::max(reach.maxZ, blockAt[2]) >> 4); chunkZ++) {
            if (world.state().chunkIfLoaded(chunkX, chunkZ) == nullptr) {
                return "the chunks around " + std::to_string(blockAt[0]) + ", " + std::to_string(blockAt[2]) +
                       " are not loaded";
            }
        }
    }
    clearSpace(world, layout.test);
    const StructureRotation rotation = rotationOf(instance.rotation);
    const StructurePos corner{layout.startCorner[0], layout.startCorner[1], layout.startCorner[2]};
    for (std::int32_t chunkX = layout.structure.minX >> 4; chunkX <= (layout.structure.maxX >> 4); chunkX++) {
        for (std::int32_t chunkZ = layout.structure.minZ >> 4; chunkZ <= (layout.structure.maxZ >> 4); chunkZ++) {
            found->placeInto(
                *world.state().chunkIfLoaded(chunkX, chunkZ),
                structure::TemplatePlacement{.position = corner,
                                             .rotation = rotation,
                                             .pivot = StructurePos{0, 0, 0},
                                             .clipTo = StructureBoundingBox{chunkX << 4, layout.structure.minY,
                                                                            chunkZ << 4, (chunkX << 4) + 15,
                                                                            layout.structure.maxY, (chunkZ << 4) + 15},
                                             .ignoreEntities = false});
        }
    }
    if (block) {
        setBlock(world, blockAt[0], blockAt[1], blockAt[2], stateOf(kTestInstanceBlock));
    }
    writeBlockEntity(world, blockAt, instance.id, {size.x, size.y, size.z}, instance.rotation, "cleared", std::nullopt);
    return std::nullopt;
}

std::optional<std::string> BlockTests::start(world::LoadedWorld& world, const world::TestInstance& instance,
                                             const std::array<std::int32_t, 3>& blockAt, Reporter reporter,
                                             const Placed& placed) {
    Layout layout;
    if (std::optional<std::string> refused = place(world, instance, blockAt, layout, true); refused.has_value()) {
        return refused;
    }
    const StructurePos size = templates_->find(*instance.structure)->size();
    encase(world, layout, instance.skyAccess, true);
    if (placed) {
        placed(StructureBoundingBox{layout.test.minX - 1, layout.test.minY - 1, layout.test.minZ - 1,
                                    layout.test.maxX + 1, layout.test.maxY + 1, layout.test.maxZ + 1});
    }
    // A flag left by an earlier run on the same ground must not count for this one.
    const Run fresh{.instance = instance, .worldName = world.name(), .blockAt = blockAt, .layout = layout};
    for (const Mode mode : {Mode::Start, Mode::Log, Mode::Fail, Mode::Accept}) {
        for (const auto& at : blocksIn(world, fresh, mode)) {
            world.state().setTestTriggered(at[0], at[1], at[2], false);
        }
    }
    runs_.push_back(Run{.instance = instance,
                        .worldName = world.name(),
                        .blockAt = blockAt,
                        .size = {size.x, size.y, size.z},
                        .layout = layout,
                        .ticks = 0,
                        .reporter = std::move(reporter)});
    return std::nullopt;
}

std::vector<BlockTests::Found> BlockTests::find(world::LoadedWorld& world, const std::int32_t centerX,
                                                const std::int32_t centerY, const std::int32_t centerZ,
                                                const std::int32_t radius) const {
    std::vector<Found> found;
    for (std::int32_t chunkX = (centerX - radius) >> 4; chunkX <= (centerX + radius) >> 4; chunkX++) {
        for (std::int32_t chunkZ = (centerZ - radius) >> 4; chunkZ <= (centerZ + radius) >> 4; chunkZ++) {
            server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            for (const server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
                if (entity.type().toString() != kTestInstanceBlock) {
                    continue;
                }
                const NbtCompound* data = entity.data().getCompound("data");
                const auto test = data == nullptr
                                      ? std::expected<Identifier, wire::common::IdentifierError>{std::unexpected{
                                            wire::common::IdentifierError{}}}
                                      : Identifier::parse(std::string{data->getString("test")});
                if (!test.has_value()) {
                    continue;
                }
                found.push_back(
                    Found{*test, {(chunkX << 4) + entity.localX(), entity.y(), (chunkZ << 4) + entity.localZ()}});
            }
        }
    }
    std::ranges::sort(found, {}, [&](const Found& block) {
        const std::int64_t dx = block.at[0] - centerX;
        const std::int64_t dy = block.at[1] - centerY;
        const std::int64_t dz = block.at[2] - centerZ;
        return (dx * dx) + (dy * dy) + (dz * dz);
    });
    return found;
}

std::optional<std::string> BlockTests::restart(world::LoadedWorld& world, const Found& block, Reporter reporter,
                                               const Placed& placed) {
    const world::TestInstance* instance = instances_->find(block.test);
    if (instance == nullptr) {
        return "there is no test " + block.test.toString();
    }
    return start(world, *instance, block.at, std::move(reporter), placed);
}

std::optional<std::string> BlockTests::reset(world::LoadedWorld& world, const Found& block, const Placed& placed) {
    const world::TestInstance* instance = instances_->find(block.test);
    if (instance == nullptr) {
        return "there is no test " + block.test.toString();
    }
    // parity: `TestInstanceBlockEntity.resetTest` puts the barriers away before it places the structure again.
    const structure::StructureTemplate* found =
        instance->structure.has_value() ? templates_->find(*instance->structure) : nullptr;
    const StructurePos size = found == nullptr ? StructurePos{1, 1, 1} : found->size();
    Layout old = layoutOf(block.at, {size.x, size.y, size.z}, instance->rotation, instance->padding);
    encase(world, old, instance->skyAccess, false);
    Layout layout;
    if (std::optional<std::string> refused = place(world, *instance, block.at, layout, false); refused.has_value()) {
        return refused;
    }
    if (placed) {
        placed(StructureBoundingBox{layout.test.minX - 1, layout.test.minY - 1, layout.test.minZ - 1,
                                    layout.test.maxX + 1, layout.test.maxY + 1, layout.test.maxZ + 1});
    }
    return std::nullopt;
}

std::optional<std::string> BlockTests::create(world::LoadedWorld& world, const Identifier& id,
                                              const std::array<std::int32_t, 3>& blockAt,
                                              const std::array<std::int32_t, 3>& size, const Placed& placed) {
    const world::TestInstance* known = instances_->find(id);
    const Layout layout = layoutOf(blockAt, size, "none", known == nullptr ? 0 : known->padding);
    const StructureBoundingBox reach{layout.test.minX - 1, layout.test.minY - 1, layout.test.minZ - 1,
                                     layout.test.maxX + 1, layout.test.maxY + 1, layout.test.maxZ + 1};
    for (std::int32_t chunkX = std::min(reach.minX, blockAt[0]) >> 4; chunkX <= (std::max(reach.maxX, blockAt[0]) >> 4);
         chunkX++) {
        for (std::int32_t chunkZ = std::min(reach.minZ, blockAt[2]) >> 4;
             chunkZ <= (std::max(reach.maxZ, blockAt[2]) >> 4); chunkZ++) {
            if (world.state().chunkIfLoaded(chunkX, chunkZ) == nullptr) {
                return "the chunks around " + std::to_string(blockAt[0]) + ", " + std::to_string(blockAt[2]) +
                       " are not loaded";
            }
        }
    }
    clearSpace(world, layout.test);
    setBlock(world, blockAt[0], blockAt[1], blockAt[2], stateOf(kTestInstanceBlock));
    writeBlockEntity(world, blockAt, id, size, "none", "cleared", std::nullopt);
    if (placed) {
        placed(reach);
    }
    return std::nullopt;
}

std::vector<std::array<std::int32_t, 3>> BlockTests::blocksIn(world::LoadedWorld& world, const Run& run,
                                                              const Mode mode) const {
    static constexpr std::array<std::string_view, 4> kModes{"start", "log", "fail", "accept"};
    const std::string_view wanted = kModes[static_cast<std::size_t>(mode)];
    std::vector<std::array<std::int32_t, 3>> found;
    const StructureBoundingBox& box = run.layout.structure;
    for (std::int32_t x = box.minX; x <= box.maxX; x++) {
        for (std::int32_t y = box.minY; y <= box.maxY; y++) {
            for (std::int32_t z = box.minZ; z <= box.maxZ; z++) {
                const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
                if (!stateId.has_value()) {
                    continue;
                }
                const Identifier* block = server::registry::blockStateDefinitions::blockOf(*stateId);
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
    broadcaster_->announceRedstone(
        world, watchersOf(world),
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
    finished_.push_back(Report{.id = run.instance.id,
                               .passed = passed,
                               .required = run.instance.required,
                               .ticks = run.ticks,
                               .failure = std::move(failure)});
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
            writeBlockEntity(world, run.blockAt, run.instance.id, run.size, run.instance.rotation, "running",
                             std::nullopt);
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
        if (owner == runs_.end()) {
            continue;
        }
        // parity: `ReportGameListener`: the block says how it ended, and a passed test lets go of its barriers.
        writeBlockEntity(world, owner->blockAt, report.id, owner->size, owner->instance.rotation, "finished",
                         report.failure);
        if (report.passed) {
            encase(world, owner->layout, owner->instance.skyAccess, false);
        }
        owner->reporter(report);
    }
    std::erase_if(runs_, [](const Run& run) { return run.ticks < 0; });
}

std::int32_t BlockTests::clearAll(world::LoadedWorld& world, const std::int32_t centerX, const std::int32_t centerZ,
                                  const std::int32_t radius) {
    const std::int32_t air = server::registry::blocks::air().stateId;
    std::int32_t cleared = 0;
    for (std::int32_t chunkX = (centerX - radius) >> 4; chunkX <= (centerX + radius) >> 4; chunkX++) {
        for (std::int32_t chunkZ = (centerZ - radius) >> 4; chunkZ <= (centerZ + radius) >> 4; chunkZ++) {
            server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            for (const server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
                if (entity.type().toString() != kTestInstanceBlock) {
                    continue;
                }
                const std::array<std::int32_t, 3> at{(chunkX << 4) + entity.localX(), entity.y(),
                                                     (chunkZ << 4) + entity.localZ()};
                const NbtCompound* data = entity.data().getCompound("data");
                const auto size = data == nullptr ? std::vector<std::int32_t>{} : data->getIntArray("size");
                const std::string rotation = data == nullptr ? "none" : std::string{data->getString("rotation")};
                const std::string testName = data == nullptr ? std::string{} : std::string{data->getString("test")};
                const auto test = Identifier::parse(testName);
                const world::TestInstance* known = test.has_value() ? instances_->find(*test) : nullptr;
                const Layout layout =
                    layoutOf(at, size.size() == 3 ? std::array{size[0], size[1], size[2]} : std::array{1, 1, 1},
                             rotation, known == nullptr ? 0 : known->padding);
                encase(world, layout, known != nullptr && known->skyAccess, false);
                clearSpace(world, layout.test);
                setBlock(world, at[0], at[1], at[2], air);
                chunk->removeBlockEntityAt(at[0] & 15, at[1], at[2] & 15);
                cleared++;
            }
        }
    }
    std::erase_if(runs_, [&](const Run& run) { return run.worldName == world.name(); });
    return cleared;
}

} // namespace basalt::server::runtime
