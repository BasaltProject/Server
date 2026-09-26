#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/EntityPushSystem.hpp>
#include <basalt/server/runtime/VehicleSystem.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <map>
#include <string>

namespace basalt::server::runtime {
namespace {

using entity::Aabb;
using entity::ServerEntity;
namespace mth = wire::common::mth;

/// parity: `AbstractBoat`'s bubble clock, and the two entity events that end it.
constexpr std::int32_t kBubbleTicks = 60;
constexpr std::int8_t kDragDownEvent = 71;
constexpr std::int8_t kPushUpEvent = 72;

[[nodiscard]] Aabb boxOf(const ServerEntity& boat) {
    return Aabb::of(boat.x, boat.y, boat.z, boat.width(), boat.height());
}

} // namespace

void VehicleSystem::tickBoatBubbles(world::LoadedWorld& world, ServerEntity& boat, bool driverIsPlayer) {
    entity::boats::BoatState& state = *boat.boat;
    const bool simulated = !driverIsPlayer;
    // parity: `applyEffectsFromBlocks`, twice a turn, and the bubble column's `entityInside`: over
    // open water it is `onAboveBubbleColumn`, the boat's own; under something it is the default push.
    const Aabb box = boxOf(boat).inflate(-static_cast<double>(1.0E-5F));
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    for (std::int32_t pass = 0; pass < 2; ++pass) {
        for (std::int32_t x = mth::floor(box.minX); x <= mth::floor(box.maxX); ++x) {
            for (std::int32_t y = mth::floor(box.minY); y <= mth::floor(box.maxY); ++y) {
                for (std::int32_t z = mth::floor(box.minZ); z <= mth::floor(box.maxZ); ++z) {
                    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
                    const wire::common::Identifier* block =
                        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
                    if (block == nullptr || block->path() != "bubble_column") {
                        continue;
                    }
                    const std::map<std::string, std::string> properties =
                        registry::blockStateDefinitions::propertiesOf(*stateId);
                    const bool down = properties.contains("drag") && properties.at("drag") == "true";
                    const std::int32_t above = world.state().blockState(x, y + 1, z).value_or(0);
                    if (registry::blockShapes::isEmptyShape(above) && !registry::blockFluids::hasFluid(above)) {
                        state.aboveBubbleColumn = true;
                        state.bubbleColumnDown = down;
                        if (state.bubbleTime == 0) {
                            state.bubbleTime = kBubbleTicks;
                        }
                        // parity: the splash one turn in a hundred, heard by sculk as `SPLASH`.
                        if (state.status != entity::boats::Status::UnderWater &&
                            state.status != entity::boats::Status::UnderFlowingWater && random.nextInt(100) == 0) {
                            world::vibrations::emit(world.state(), "splash", boat.x, boat.y, boat.z, world.gameTime(),
                                                    {.entity = boat.uuid()});
                        }
                    } else if (simulated) {
                        boat.velocityY =
                            down ? std::max(-0.3, boat.velocityY - 0.03) : std::min(0.7, boat.velocityY + 0.06);
                    }
                }
            }
        }
    }
    // parity: `tickBubbleColumn` on the server.
    if (!state.aboveBubbleColumn) {
        state.bubbleTime = 0;
    }
    if (state.bubbleTime <= 0) {
        return;
    }
    --state.bubbleTime;
    const std::int32_t diff = kBubbleTicks - state.bubbleTime - 1;
    if (diff > 0 && state.bubbleTime == 0) {
        if (state.bubbleColumnDown) {
            ejectPassengers(world, boat);
            entityEvent(world, boat, kDragDownEvent);
        } else {
            entityEvent(world, boat, kPushUpEvent);
        }
        // parity: `handleBubbleColumnEffect`, on whichever side moves the boat.
        if (simulated) {
            if (state.bubbleColumnDown) {
                boat.velocityY -= 0.7;
            } else {
                boat.velocityY = 0.6;
            }
        }
    }
    state.aboveBubbleColumn = false;
}

void VehicleSystem::boatPushes(world::LoadedWorld& world, ServerEntity& boat, bool driverIsPlayer) {
    // parity: the end of `AbstractBoat.tick`: the box grown a fifth sideways and shrunk a hundredth
    // up and down, a mob taken aboard while nobody steers it, anything else pushed away.
    const EntityPushSystem::Body self{.mob = &boat};
    const Aabb box = boxOf(boat);
    const Aabb reach{box.minX - 0.2, box.minY + 0.01, box.minZ - 0.2, box.maxX + 0.2, box.maxY - 0.01, box.maxZ + 0.2};
    for (const EntityPushSystem::Body& other : pushes_->pushablesIn(world, self, reach)) {
        if (boat.vehicleId.has_value() && other.mob != nullptr && *boat.vehicleId == other.mob->entityId()) {
            continue;
        }
        const bool mob = other.mob != nullptr;
        const bool boardable =
            mob && !driverIsPlayer &&
            static_cast<std::int32_t>(boat.passengerIds.size()) < entity::boats::maxPassengers(boat.type()) &&
            !other.mob->vehicleId.has_value() && other.mob->width() < boat.width() &&
            registry::entityAttributes::isLiving(other.mob->type()) &&
            !(options_.entityTags != nullptr &&
              world::realEntityTypeTags::membersOf(*options_.entityTags,
                                                   wire::common::Identifier::vanilla("cannot_be_pushed_onto_boats"))
                  .contains(other.mob->type()));
        if (boardable && board(world, boat, *other.mob)) {
            continue;
        }
        if (!boardable) {
            pushes_->push(world, self, other);
        }
    }
}

} // namespace basalt::server::runtime
