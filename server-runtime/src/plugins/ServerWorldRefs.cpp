#include "ServerWorldRefs.hpp"

#include "DataFix.hpp"
#include "WorldBlockUpdates.hpp"
#include "WorldItemUse.hpp"

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/DroppedItemOps.hpp>
#include <basalt/server/runtime/plugins/EntityOps.hpp>
#include <basalt/server/runtime/plugins/PlayerEntityOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/plugins/ServerWorldEnv.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>
#include <basalt/server/runtime/world/poi/PoiTypes.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace basalt::server::runtime::plugins::worldModel {
namespace reg = server::registry;
using wire::common::Identifier;

const Identifier& textDisplay() {
    static const Identifier kType = Identifier::vanilla("text_display");
    return kType;
}

WorldEntityRef::WorldEntityRef(world::LoadedWorld& world, std::int32_t entityId, std::string type,
                               std::shared_ptr<const ModelHooks> hooks,
                               std::function<std::vector<player::PlayerSession*>()> players)
    : world_(&world), hooks_(std::move(hooks)), players_(std::move(players)), entityId_(entityId),
      type_(std::move(type)) {}

std::int32_t WorldEntityRef::id() const { return entityId_; }

std::string WorldEntityRef::type() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? type_ : found->type().toString();
}

std::string WorldEntityRef::world() const { return world_->name(); }

double WorldEntityRef::x() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? 0.0 : found->x;
}

double WorldEntityRef::y() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? 0.0 : found->y;
}

double WorldEntityRef::z() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? 0.0 : found->z;
}

bool WorldEntityRef::alive() const { return entity() != nullptr; }

double WorldEntityRef::health() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? 0.0 : found->health;
}

void WorldEntityRef::setHealth(double value) {
    entity::ServerEntity* found = entity();
    if (found != nullptr) {
        found->health = value;
    }
}

void WorldEntityRef::remove() {
    entity::ServerEntity* found = entity();
    if (found != nullptr) {
        found->removed = true;
    }
}

bool WorldEntityRef::teleport(double x, double y, double z) {
    entity::ServerEntity* found = entity();
    if (found == nullptr) {
        return false;
    }
    found->x = x;
    found->y = y;
    found->z = z;
    return true;
}

std::string WorldEntityRef::uuid() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? std::string{} : found->uuid().toString();
}

std::array<float, 2> WorldEntityRef::rotation() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? std::array<float, 2>{0.0F, 0.0F} : std::array<float, 2>{found->yaw, found->pitch};
}

bool WorldEntityRef::setRotation(float yaw, float pitch) {
    entity::ServerEntity* found = entity();
    if (found == nullptr) {
        return false;
    }
    found->yaw = yaw;
    found->pitch = pitch;
    return true;
}

std::array<double, 3> WorldEntityRef::velocity() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? std::array<double, 3>{0.0, 0.0, 0.0}
                            : std::array<double, 3>{found->velocityX, found->velocityY, found->velocityZ};
}

bool WorldEntityRef::setVelocity(double x, double y, double z) {
    entity::ServerEntity* found = entity();
    if (found == nullptr) {
        return false;
    }
    found->velocityX = x;
    found->velocityY = y;
    found->velocityZ = z;
    return true;
}

bool WorldEntityRef::onGround() const {
    const entity::ServerEntity* found = entity();
    return found != nullptr && found->onGround;
}

std::int32_t WorldEntityRef::age() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? 0 : found->age;
}

double WorldEntityRef::width() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? 0.6 : reg::entityTypeShapes::widthOf(found->type());
}

double WorldEntityRef::height() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? 1.8 : reg::entityTypeShapes::heightOf(found->type());
}

std::optional<std::string> WorldEntityRef::customName() const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? std::nullopt : found->customName;
}

std::optional<server::plugins::PluginEntityValue> WorldEntityRef::property(std::string_view name) const {
    const entity::ServerEntity* found = entity();
    return found == nullptr ? std::nullopt : entityOps::get(environment(), *found, name);
}

bool WorldEntityRef::setProperty(std::string_view name, const server::plugins::PluginEntityValue& value) {
    entity::ServerEntity* found = entity();
    return found != nullptr && entityOps::set(environment(), *found, name, value);
}

std::optional<server::plugins::PluginEntityValue>
WorldEntityRef::invoke(std::string_view operation, const std::vector<server::plugins::PluginEntityValue>& arguments) {
    entity::ServerEntity* found = entity();
    return found == nullptr ? std::nullopt : entityOps::invoke(environment(), *found, operation, arguments);
}

entity::ServerEntity* WorldEntityRef::entity() const {
    entity::ServerEntity* found = world_->state().entity(entityId_);
    return found != nullptr && !found->removed ? found : nullptr;
}

entityOps::Environment WorldEntityRef::environment() const {
    return entityOps::Environment{.hooks = hooks_.get(), .world = world_, .players = players_};
}

HologramRef::HologramRef(world::LoadedWorld& world, std::int32_t entityId, std::string type)
    : inner_(world, entityId, std::move(type)) {}

std::int32_t HologramRef::id() const { return inner_.id(); }

std::string HologramRef::type() const { return inner_.type(); }

std::string HologramRef::world() const { return inner_.world(); }

double HologramRef::x() const { return inner_.x(); }

double HologramRef::y() const { return inner_.y(); }

double HologramRef::z() const { return inner_.z(); }

bool HologramRef::alive() const { return inner_.alive(); }

double HologramRef::health() const { return inner_.health(); }

void HologramRef::setHealth(double value) { inner_.setHealth(value); }

void HologramRef::remove() { inner_.remove(); }

bool HologramRef::teleport(double x, double y, double z) { return inner_.teleport(x, y, z); }

std::string HologramRef::text() const {
    const entity::ServerEntity* found = held();
    return found == nullptr ? "" : found->displayText.value_or("");
}

void HologramRef::setText(std::string_view value) {
    entity::ServerEntity* found = held();
    if (found != nullptr) {
        found->displayText = std::string{value};
    }
}

void HologramRef::showOnlyTo(const std::vector<std::string>& uuids) {
    entity::ServerEntity* found = held();
    if (found != nullptr) {
        found->visibleTo = uuids;
    }
}

void HologramRef::showToAll() {
    entity::ServerEntity* found = held();
    if (found != nullptr) {
        found->visibleTo.reset();
    }
}

std::optional<std::vector<std::string>> HologramRef::audience() const {
    const entity::ServerEntity* found = held();
    return found == nullptr ? std::nullopt : found->visibleTo;
}

entity::ServerEntity* HologramRef::held() const { return inner_.entity(); }
} // namespace basalt::server::runtime::plugins::worldModel
