#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/EntityPushSystem.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/FluidCurrents.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace basalt::server::runtime {
namespace {

using entity::Aabb;
using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::CollisionRule;
using wire::protocol::play::Gamemode;
using wire::protocol::play::Pose;
namespace mth = wire::common::mth;

/// parity: `Entity.push(Entity)`'s twentieth, and the cart's own half of a tenth.
constexpr double kEntityPush = static_cast<double>(0.05F);
constexpr double kCartPush = static_cast<double>(0.1F) * static_cast<double>(0.5F);
constexpr double kPushThreshold = static_cast<double>(0.01F);
constexpr double kCartPushThreshold = static_cast<double>(1.0E-4F);
/// parity: `ArmorStand.pushEntities`, a rideable cart within this squared distance.
constexpr double kArmorStandCartReach = 0.2;
constexpr float kCrammingDamage = 6.0F;

[[nodiscard]] std::optional<std::int32_t> vehicleOf(const EntityPushSystem::Body& body) {
    return body.mob != nullptr ? body.mob->vehicleId : body.player->vehicleId;
}

[[nodiscard]] std::int32_t idOf(const EntityPushSystem::Body& body) {
    return body.mob != nullptr ? body.mob->entityId() : body.player->entityId();
}

[[nodiscard]] double xOf(const EntityPushSystem::Body& body) {
    return body.mob != nullptr ? body.mob->x : body.player->x;
}

[[nodiscard]] double zOf(const EntityPushSystem::Body& body) {
    return body.mob != nullptr ? body.mob->z : body.player->z;
}

/// parity: `Entity.getRootVehicle`, the bottom of the stack.
[[nodiscard]] std::int32_t rootOf(world::LoadedWorld& world, const EntityPushSystem::Body& body) {
    std::int32_t root = idOf(body);
    std::optional<std::int32_t> below = vehicleOf(body);
    for (std::int32_t guard = 0; below.has_value() && guard < 64; ++guard) {
        root = *below;
        const ServerEntity* vehicle = world.state().entity(*below);
        below = vehicle == nullptr ? std::nullopt : vehicle->vehicleId;
    }
    return root;
}

/// parity: `Entity.isVehicle`.
[[nodiscard]] bool isVehicle(const EntityPushSystem::Body& body) {
    return body.mob != nullptr && !body.mob->passengerIds.empty();
}

/// parity: `Entity.noPhysics`: a vex passes through everything, a spectator too.
[[nodiscard]] bool noPhysics(const EntityPushSystem::Body& body) {
    if (body.player != nullptr) {
        return body.player->gamemode == Gamemode::Spectator;
    }
    return body.mob->noPhysics || body.mob->type().path() == "vex";
}

[[nodiscard]] bool isSpectator(const EntityPushSystem::Body& body) {
    return body.player != nullptr && body.player->gamemode == Gamemode::Spectator;
}

[[nodiscard]] std::string memberOf(const EntityPushSystem::Body& body) {
    return body.player != nullptr ? std::string{body.player->name()} : body.mob->uuid().toString();
}

[[nodiscard]] double playerHeight(Pose pose) {
    switch (pose) {
    case Pose::Sleeping:
    case Pose::Dying:
        return 0.2;
    case Pose::FallFlying:
    case Pose::Swimming:
    case Pose::SpinAttack:
        return 0.6;
    case Pose::Crouching:
        return 1.5;
    default:
        return 1.8;
    }
}

} // namespace

EntityPushSystem::EntityPushSystem(player::PlayerRegistry& players, CombatSystem& combat, Hooks hooks, Options options)
    : players_(&players), combat_(&combat), hooks_(std::move(hooks)), options_(std::move(options)) {}

Aabb EntityPushSystem::boxOf(const Body& body) {
    if (body.mob != nullptr) {
        return Aabb::of(body.mob->x, body.mob->y, body.mob->z, body.mob->width(), body.mob->height());
    }
    const double width = body.player->pose == Pose::Sleeping || body.player->pose == Pose::Dying ? 0.2 : 0.6;
    return Aabb::of(body.player->x, body.player->y, body.player->z, width, playerHeight(body.player->pose));
}

void EntityPushSystem::addMotion(const Body& body, double x, double y, double z) {
    if (body.mob != nullptr) {
        body.mob->velocityX += x;
        body.mob->velocityY += y;
        body.mob->velocityZ += z;
        return;
    }
    body.player->deltaMovement[0] += x;
    body.player->deltaMovement[1] += y;
    body.player->deltaMovement[2] += z;
}

bool EntityPushSystem::onClimbable(world::LoadedWorld& world, double x, double y, double z) const {
    const std::int32_t bx = mth::floor(x);
    const std::int32_t by = mth::floor(y);
    const std::int32_t bz = mth::floor(z);
    const std::optional<std::int32_t> state = world.state().blockState(bx, by, bz);
    const Identifier* block = state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    if (block == nullptr) {
        return false;
    }
    if (options_.blockTags != nullptr &&
        world::realBlockTags::contains(*options_.blockTags, "minecraft:climbable", *block)) {
        return true;
    }
    // parity: `trapdoorUsableAsLadder`, an open trapdoor over a ladder facing the same way.
    if (!block->path().ends_with("_trapdoor")) {
        return false;
    }
    const std::map<std::string, std::string> trapdoor = registry::blockStateDefinitions::propertiesOf(*state);
    const auto open = trapdoor.find("open");
    if (open == trapdoor.end() || open->second != "true") {
        return false;
    }
    const std::optional<std::int32_t> below = world.state().blockState(bx, by - 1, bz);
    const Identifier* belowBlock = below.has_value() ? registry::blockStateDefinitions::blockOf(*below) : nullptr;
    if (belowBlock == nullptr || belowBlock->path() != "ladder") {
        return false;
    }
    const std::map<std::string, std::string> ladder = registry::blockStateDefinitions::propertiesOf(*below);
    return ladder.contains("facing") && trapdoor.contains("facing") && ladder.at("facing") == trapdoor.at("facing");
}

bool EntityPushSystem::isPushable(world::LoadedWorld& world, const Body& body) const {
    if (body.player != nullptr) {
        const PlayerSession& player = *body.player;
        return !player.dead && player.gamemode != Gamemode::Spectator &&
               !onClimbable(world, player.x, player.y, player.z);
    }
    const ServerEntity& mob = *body.mob;
    if (mob.removed || !mob.collidable) {
        return false;
    }
    if (world::detectorRails::isMinecart(mob.type()) || entity::boats::isBoat(mob.type())) {
        return true;
    }
    if (!registry::entityAttributes::isLiving(mob.type())) {
        return false;
    }
    const std::string_view path = mob.type().path();
    if (path == "bat" || path == "armor_stand") {
        return false;
    }
    if (path == "parrot") {
        return true;
    }
    if (entity::horses::isHorse(mob.type()) && !mob.passengerIds.empty()) {
        return false;
    }
    if (entity::wardens::isWarden(mob.type())) {
        const Pose pose = entity::wardenBrain::poseOf(mob);
        if (pose == Pose::Digging || pose == Pose::Emerging) {
            return false;
        }
    }
    if (path == "creaking" && mob.creaking != nullptr && !mob.creaking->canMove) {
        return false;
    }
    return !mob.dying && mob.health > 0.0 && !onClimbable(world, mob.x, mob.y, mob.z);
}

bool EntityPushSystem::pushableBy(world::LoadedWorld& world, const Body& pusher, const Body& input) const {
    // parity: `EntitySelector.pushableBy`.
    const scoreboard::Team* own = options_.teams == nullptr ? nullptr : options_.teams->teamOf(memberOf(pusher));
    const CollisionRule ownRule = own == nullptr ? CollisionRule::Always : own->collisionRule;
    if (ownRule == CollisionRule::Never || isSpectator(input) || !isPushable(world, input)) {
        return false;
    }
    const scoreboard::Team* theirs = options_.teams == nullptr ? nullptr : options_.teams->teamOf(memberOf(input));
    const CollisionRule theirRule = theirs == nullptr ? CollisionRule::Always : theirs->collisionRule;
    if (theirRule == CollisionRule::Never) {
        return false;
    }
    const bool sameTeam = own != nullptr && own == theirs;
    if ((ownRule == CollisionRule::PushOwnTeam || theirRule == CollisionRule::PushOwnTeam) && sameTeam) {
        return false;
    }
    return !((ownRule == CollisionRule::PushOtherTeams || theirRule == CollisionRule::PushOtherTeams) && !sameTeam);
}

std::vector<EntityPushSystem::Body> EntityPushSystem::entitiesIn(world::LoadedWorld& world, const Body& self,
                                                                 const Aabb& box) const {
    std::vector<Body> found;
    // parity: `EntitySectionStorage`.
    constexpr double kMargin = 2.0;
    for (std::int32_t chunkX = mth::floor(box.minX - kMargin) >> 4; chunkX <= mth::floor(box.maxX + kMargin) >> 4;
         ++chunkX) {
        for (std::int32_t chunkZ = mth::floor(box.minZ - kMargin) >> 4; chunkZ <= mth::floor(box.maxZ + kMargin) >> 4;
             ++chunkZ) {
            for (ServerEntity* mob : world.state().entitiesIn(chunkX, chunkZ)) {
                if (mob == self.mob || mob->removed) {
                    continue;
                }
                const Body body{.mob = mob};
                if (boxOf(body).intersects(box)) {
                    found.push_back(body);
                }
            }
        }
    }
    for (PlayerSession* player : players_->snapshot()) {
        if (player == self.player || player->worldName != world.name() || player->dead) {
            continue;
        }
        const Body body{.player = player};
        if (boxOf(body).intersects(box)) {
            found.push_back(body);
        }
    }
    return found;
}

std::vector<EntityPushSystem::Body> EntityPushSystem::pushablesIn(world::LoadedWorld& world, const Body& pusher,
                                                                  const Aabb& box) const {
    std::vector<Body> found = entitiesIn(world, pusher, box);
    std::erase_if(found, [&](const Body& input) { return !pushableBy(world, pusher, input); });
    return found;
}

void EntityPushSystem::crammed(world::LoadedWorld& world, const Body& body) {
    if (body.player != nullptr) {
        if (body.player->connection().hurt) {
            body.player->connection().hurt(kCrammingDamage, "cramming", world::damageTypes::kCramming, std::nullopt);
        }
        return;
    }
    const entity::entityCombat::Result result =
        entity::entityCombat::hurt(*body.mob, kCrammingDamage, std::nullopt, std::nullopt, hooks_.combat, "cramming");
    if (result.landed) {
        std::vector<PlayerSession*> watchers;
        for (PlayerSession* player : players_->snapshot()) {
            if (player->worldName == world.name()) {
                watchers.push_back(player);
            }
        }
        combat_->announceEntityHit(watchers, *body.mob, world::damageTypes::kCramming, -1, result.died);
    }
}

void EntityPushSystem::pushByMob(world::LoadedWorld& world, ServerEntity& mob) {
    const Body self{.mob = &mob};
    const std::string_view path = mob.type().path();
    if (path == "bat") {
        return;
    }
    // parity: `ArmorStand.pushEntities`, only a rideable cart close to its middle.
    if (path == "armor_stand") {
        for (const Body& other : entitiesIn(world, self, boxOf(self))) {
            if (other.mob == nullptr || other.mob->type().path() != "minecart") {
                continue;
            }
            const double dx = other.mob->x - mob.x;
            const double dy = other.mob->y - mob.y;
            const double dz = other.mob->z - mob.z;
            if ((dx * dx) + (dy * dy) + (dz * dz) <= kArmorStandCartReach) {
                push(world, other, self);
            }
        }
        return;
    }
    const std::vector<Body> pushables = pushablesIn(world, self, boxOf(self));
    if (pushables.empty()) {
        return;
    }
    const std::int32_t maxCramming = world.gameRules().get(world::gameRules::kMaxEntityCramming);
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    if (maxCramming > 0 && static_cast<std::int32_t>(pushables.size()) > maxCramming - 1 && random.nextInt(4) == 0) {
        const auto count =
            std::ranges::count_if(pushables, [](const Body& other) { return !vehicleOf(other).has_value(); });
        if (count > maxCramming - 1) {
            crammed(world, self);
        }
    }
    for (const Body& other : pushables) {
        doPush(world, self, other);
    }
}

void EntityPushSystem::pushByFluid(world::LoadedWorld& world, ServerEntity& entity) {
    static_cast<void>(world::fluidCurrents::carry(world, entity, options_.blockTags));
}

void EntityPushSystem::pushByFluid(world::LoadedWorld& world, PlayerSession& player) {
    static_cast<void>(world::fluidCurrents::carry(world, player, options_.blockTags));
}

void EntityPushSystem::pushByPlayer(world::LoadedWorld& world, PlayerSession& player) {
    const Body self{.player = &player};
    if (player.dead || player.gamemode == Gamemode::Spectator) {
        return;
    }
    const std::vector<Body> pushables = pushablesIn(world, self, boxOf(self));
    if (pushables.empty()) {
        return;
    }
    const std::int32_t maxCramming = world.gameRules().get(world::gameRules::kMaxEntityCramming);
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    if (maxCramming > 0 && static_cast<std::int32_t>(pushables.size()) > maxCramming - 1 && random.nextInt(4) == 0) {
        const auto count =
            std::ranges::count_if(pushables, [](const Body& other) { return !vehicleOf(other).has_value(); });
        if (count > maxCramming - 1) {
            crammed(world, self);
        }
    }
    for (const Body& other : pushables) {
        doPush(world, self, other);
    }
}

void EntityPushSystem::doPush(world::LoadedWorld& world, const Body& pusher, const Body& target) {
    if (!world.settings().rules.tuning.playerCollisions && pusher.player != nullptr && target.player != nullptr) {
        return;
    }
    if (pusher.mob != nullptr) {
        ServerEntity& mob = *pusher.mob;
        const std::string_view path = mob.type().path();
        if (entity::wardens::isWarden(mob.type()) && hooks_.wardenTouched) {
            hooks_.wardenTouched(world, mob, target);
        }
        // parity: `IronGolem.doPush`, one bump in twenty turns a monster into its target.
        if (path == "iron_golem" && target.mob != nullptr && hooks_.golemTouched &&
            entity::mobs::categoryOf(target.mob->type()) == "monster" && target.mob->type().path() != "creeper" &&
            !entity::horses::isHorse(target.mob->type())) {
            random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
            if (random.nextInt(20) == 0) {
                hooks_.golemTouched(mob, *target.mob);
            }
        }
        // parity: `Parrot.doPush`, which leaves players alone.
        if (path == "parrot" && target.player != nullptr) {
            return;
        }
    }
    push(world, target, pusher);
}

void EntityPushSystem::push(world::LoadedWorld& world, const Body& target, const Body& pusher) {
    if (target.mob != nullptr && world::detectorRails::isMinecart(target.mob->type())) {
        cartPush(world, *target.mob, pusher);
        return;
    }
    // parity: `AbstractBoat.push`.
    // from level with its bottom or lower.
    if (target.mob != nullptr && entity::boats::isBoat(target.mob->type())) {
        const Aabb mine = boxOf(target);
        const Aabb theirs = boxOf(pusher);
        const bool pusherIsBoat = pusher.mob != nullptr && entity::boats::isBoat(pusher.mob->type());
        if (pusherIsBoat ? theirs.minY < mine.maxY : theirs.minY <= mine.minY) {
            entityPush(world, target, pusher);
        }
        return;
    }
    entityPush(world, target, pusher);
}

void EntityPushSystem::entityPush(world::LoadedWorld& world, const Body& target, const Body& pusher) {
    // parity: `Entity.push(Entity)`, from the target's side.
    if (rootOf(world, target) == rootOf(world, pusher) || noPhysics(pusher) || noPhysics(target)) {
        return;
    }
    double xa = xOf(pusher) - xOf(target);
    double za = zOf(pusher) - zOf(target);
    double dd = std::max(std::abs(xa), std::abs(za));
    if (dd < kPushThreshold) {
        return;
    }
    dd = std::sqrt(dd);
    xa /= dd;
    za /= dd;
    const double pow = std::min(1.0 / dd, 1.0);
    xa *= pow * kEntityPush;
    za *= pow * kEntityPush;
    if (!isVehicle(target) && isPushable(world, target)) {
        addMotion(target, -xa, 0.0, -za);
    }
    if (!isVehicle(pusher) && isPushable(world, pusher)) {
        addMotion(pusher, xa, 0.0, za);
    }
}

void EntityPushSystem::cartPush(world::LoadedWorld& /*world*/, ServerEntity& cart, const Body& pusher) {
    // parity: `AbstractMinecart.push(Entity)`.
    if (noPhysics(pusher) || cart.noPhysics ||
        std::ranges::find(cart.passengerIds, idOf(pusher)) != cart.passengerIds.end()) {
        return;
    }
    double xa = xOf(pusher) - cart.x;
    double za = zOf(pusher) - cart.z;
    double dd = (xa * xa) + (za * za);
    if (dd < kCartPushThreshold) {
        return;
    }
    dd = std::sqrt(dd);
    xa /= dd;
    za /= dd;
    const double pow = std::min(1.0 / dd, 1.0);
    xa *= pow * kCartPush;
    za *= pow * kCartPush;
    if (pusher.mob != nullptr && world::detectorRails::isMinecart(pusher.mob->type())) {
        entity::minecarts::pushOtherMinecart(cart, *pusher.mob, xa, za,
                                             options_.minecartImprovements && options_.minecartImprovements());
        return;
    }
    cart.velocityX -= xa;
    cart.velocityZ -= za;
    addMotion(pusher, xa / 4.0, 0.0, za / 4.0);
}

} // namespace basalt::server::runtime
