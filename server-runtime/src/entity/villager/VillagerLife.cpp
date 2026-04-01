#include "BrainInternals.hpp"

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace basalt::server::runtime::entity::villager {
namespace {

namespace poiTypes = world::poi::poiTypes;

/// parity: `Villager.GOSSIP_COOLDOWN` and `GOSSIP_DECAY_INTERVAL`.
constexpr std::int64_t kGossipCooldown = 1200;
constexpr std::int64_t kGossipDecay = 24000;

[[nodiscard]] std::array<const Ops*, static_cast<std::size_t>(Kind::kCount)> buildOps() {
    std::array<const Ops*, static_cast<std::size_t>(Kind::kCount)> table{};
    for (std::size_t index = 0; index < table.size(); ++index) {
        const auto kind = static_cast<Kind>(index);
        for (const Ops* found :
             {ops::core(kind), ops::poi(kind), ops::work(kind), ops::social(kind), ops::raid(kind)}) {
            if (found != nullptr) {
                table[index] = found;
            }
        }
    }
    return table;
}

[[nodiscard]] std::optional<std::string> uuidOf(Ctx& ctx, std::int32_t entityId) {
    if (const ai::PlayerView* player = look::player(ctx, entityId); player != nullptr) {
        return player->uuid;
    }
    if (const ServerEntity* mob = look::mob(ctx, entityId); mob != nullptr) {
        return mob->uuid().toString();
    }
    return std::nullopt;
}

/// parity: `Mob.aiStep`'s pick-up, a block to each side and none up or down.
void pickUpItems(Ctx& ctx) {
    if (!ctx.world.mobGriefing(ctx.self)) {
        return;
    }
    std::vector<VillagerWorld::ItemView> items;
    ctx.world.itemsNear(ctx.self, 1.0, 0.0, items);
    for (const VillagerWorld::ItemView& item : items) {
        if (!item.pickable || item.stack.count <= 0 || !villagerData::wantsToPickUp(ctx.self, ctx.state, item.stack)) {
            continue;
        }
        const std::int32_t left = villagerData::addItem(ctx.state, item.stack);
        const std::int32_t taken = item.stack.count - left;
        if (taken > 0) {
            static_cast<void>(ctx.world.takeItem(ctx.self, item.id, taken));
        }
    }
}

/// parity: `Villager.setLastHurtByMob`'s `VILLAGER_HURT` and the angry particles a player earns.
void noticeHurt(Ctx& ctx) {
    const std::int32_t panic = ctx.self.panicTicks;
    const bool fresh = panic > ctx.state.seenPanic;
    ctx.state.seenPanic = panic;
    if (!fresh || !ctx.self.hurtBy.has_value()) {
        return;
    }
    const std::int32_t attacker = *ctx.self.hurtBy;
    if (const std::optional<std::string> uuid = uuidOf(ctx, attacker); uuid.has_value()) {
        ctx.state.gossips.add(*uuid, GossipType::kMinorNegative, Gossips::kPerEvent);
    }
    if (look::player(ctx, attacker) != nullptr) {
        ctx.world.entityEvent(ctx.self, 13);
    }
}

[[nodiscard]] Ctx contextFor(ai::MobContext& mob, VillagerWorld& world) {
    VillagerState& state = villagerBrain::stateOf(mob.mob);
    return Ctx{.mob = mob,
               .self = mob.mob,
               .state = state,
               .mem = state.memories,
               .poi = mob.world.poi(),
               .world = world,
               .random = mob.random,
               .now = mob.gameTime};
}

} // namespace

const Ops& ops::of(Kind kind) {
    static const std::array<const Ops*, static_cast<std::size_t>(Kind::kCount)> kTable = buildOps();
    static const Ops kNothing{};
    const Ops* found = kTable[static_cast<std::size_t>(kind)];
    return found == nullptr ? kNothing : *found;
}

namespace brain {

void releasePoi(Ctx& ctx, MemoryId id) {
    Memory<BlockPos>* memory = ctx.mem.position(id);
    if (memory == nullptr || !memory->present()) {
        return;
    }
    const BlockPos pos = *memory->value;
    const std::optional<world::poi::PoiType> type = ctx.poi.type(pos);
    if (!type.has_value()) {
        return;
    }
    // parity: `Villager.POI_MEMORIES`, each memory with the type it may hold.
    bool matches = false;
    switch (id) {
    case MemoryId::kHome:
        matches = *type == world::poi::PoiType::kHome;
        break;
    case MemoryId::kJobSite:
        matches = poiTypes::jobSiteOf(ctx.self.profession) == *type;
        break;
    case MemoryId::kPotentialJobSite:
        matches = world::poi::matches(poiTypes::kAcquirableJobSites, *type);
        break;
    case MemoryId::kMeetingPoint:
        matches = *type == world::poi::PoiType::kMeeting;
        break;
    default:
        break;
    }
    if (matches) {
        static_cast<void>(ctx.poi.release(pos));
    }
}

void spawnGolemIfNeeded(Ctx& ctx, std::int32_t villagersNeeded) {
    if (!villagerBrain::wantsGolem(ctx.self, ctx.now)) {
        return;
    }
    // parity: every villager in the box grown by ten, the first five of them asked.
    const Aabb box = Aabb::of(ctx.self.x, ctx.self.y, ctx.self.z, ctx.self.width(), ctx.self.height()).inflate(10.0);
    std::vector<ServerEntity*> nearby;
    for (ServerEntity* other : ctx.mob.entities) {
        if (other != nullptr && !other->removed && other->type().path() == "villager" &&
            Aabb::of(other->x, other->y, other->z, other->width(), other->height()).intersects(box)) {
            nearby.push_back(other);
        }
    }
    std::int32_t agreeing = 0;
    for (ServerEntity* other : nearby) {
        if (agreeing >= 5) {
            break;
        }
        if (villagerBrain::wantsGolem(*other, ctx.now)) {
            ++agreeing;
        }
    }
    if (agreeing < villagersNeeded || !ctx.world.summonGolem(ctx.self)) {
        return;
    }
    for (ServerEntity* other : nearby) {
        villagerBrain::golemDetected(*other);
    }
}

void gossip(Ctx& ctx, ServerEntity& other) {
    VillagerState& theirs = villagerBrain::stateOf(other);
    const auto ready = [&ctx](std::int64_t last) { return ctx.now < last || ctx.now >= last + kGossipCooldown; };
    if (!ready(ctx.state.lastGossipTime) || !ready(theirs.lastGossipTime)) {
        return;
    }
    static_cast<void>(ctx.state.gossips.transferFrom(theirs.gossips, ctx.random, 10));
    ctx.state.lastGossipTime = ctx.now;
    theirs.lastGossipTime = ctx.now;
    spawnGolemIfNeeded(ctx, 5);
}

} // namespace brain

namespace villagerData {

void setProfession(Ctx& ctx, std::string_view profession) {
    if (ctx.self.profession != profession) {
        // parity: `setVillagerData`, whose new profession throws the old offers away.
        ctx.self.offers.reset();
    }
    ctx.self.profession = std::string{profession};
    if (!ctx.self.sourceNbt.has_value()) {
        ctx.self.sourceNbt.emplace();
    }
    wire::nbt::NbtCompound data;
    if (const wire::nbt::NbtCompound* old = ctx.self.sourceNbt->getCompound("VillagerData"); old != nullptr) {
        data = *old;
    }
    data.putString("profession", "minecraft:" + std::string{profession});
    data.putInt("level", ctx.self.villagerLevel);
    ctx.self.sourceNbt->set("VillagerData", wire::nbt::NbtTag::ofCompound(std::move(data)));
    ctx.world.villagerDataChanged(ctx.self);
    // parity: `refreshBrain`, done once this turn is over rather than in the middle of it.
    ctx.state.refresh = true;
}

} // namespace villagerData

namespace trading {

namespace {

/// parity: `MerchantOffer.updateDemand`.
void updateDemand(world::trades::Offer& offer) {
    offer.demand = offer.demand + offer.uses - (offer.maxUses - offer.uses);
}

} // namespace

bool shouldRestock(Ctx& ctx) {
    std::vector<world::trades::Offer>* offers = ctx.world.offers(ctx.self);
    VillagerState& state = ctx.state;
    const std::int64_t halfDay = state.lastRestock + 12000;
    bool newDay = ctx.now > halfDay;
    const std::int64_t day = ctx.world.currentDay(ctx.self);
    newDay = newDay || (state.lastRestockCheckDay > 0 && day > state.lastRestockCheckDay);
    state.lastRestockCheckDay = day;
    if (newDay) {
        state.lastRestock = ctx.now;
        // parity: `resetNumberOfRestocks` over `catchUpDemand`.
        const std::int32_t missed = 2 - state.restocksToday;
        if (offers != nullptr) {
            if (missed > 0) {
                for (world::trades::Offer& offer : *offers) {
                    offer.uses = 0;
                }
            }
            for (std::int32_t update = 0; update < missed; ++update) {
                for (world::trades::Offer& offer : *offers) {
                    updateDemand(offer);
                }
            }
        }
        state.restocksToday = 0;
    }
    const bool allowed = state.restocksToday == 0 || (state.restocksToday < 2 && ctx.now > state.lastRestock + 2400);
    const bool needed = offers != nullptr &&
                        std::ranges::any_of(*offers, [](const world::trades::Offer& offer) { return offer.uses > 0; });
    return allowed && needed;
}

void restock(Ctx& ctx) {
    if (std::vector<world::trades::Offer>* offers = ctx.world.offers(ctx.self); offers != nullptr) {
        for (world::trades::Offer& offer : *offers) {
            updateDemand(offer);
        }
        for (world::trades::Offer& offer : *offers) {
            offer.uses = 0;
        }
    }
    ctx.state.lastRestock = ctx.now;
    ++ctx.state.restocksToday;
}

} // namespace trading

namespace villagerBrain {

VillagerState& stateOf(ServerEntity& villager) {
    if (villager.villager == nullptr) {
        villager.villager = std::make_shared<VillagerState>();
    }
    return *villager.villager;
}

void wokeUp(ServerEntity& villager, std::int64_t gameTime) { stateOf(villager).memories.lastWoken.set(gameTime); }

void golemDetected(ServerEntity& villager) {
    // parity: `GolemSensor.MEMORY_TIME_TO_LIVE`.
    constexpr std::int64_t kGolemMemory = 599;
    stateOf(villager).memories.golemDetectedRecently.set(true, kGolemMemory);
}

bool wantsGolem(ServerEntity& villager, std::int64_t gameTime) {
    // parity: `golemSpawnConditionsMet`, slept within the last day.
    constexpr std::int64_t kSleptWithin = 24000;
    const Memories& memories = stateOf(villager).memories;
    return memories.lastSlept.present() && gameTime - *memories.lastSlept.value < kSleptWithin &&
           !memories.golemDetectedRecently.present();
}

void heardBell(ServerEntity& villager, std::int64_t gameTime) {
    stateOf(villager).memories.heardBellTime.set(gameTime);
}

void curedBy(ServerEntity& villager, const std::string& playerUuid) {
    Gossips& gossips = stateOf(villager).gossips;
    gossips.add(playerUuid, GossipType::kMajorPositive, Gossips::kPerEverlastingMemory);
    gossips.add(playerUuid, GossipType::kMinorPositive, Gossips::kPerEvent);
}

std::int32_t reputationOf(ServerEntity& villager, const std::string& playerUuid) {
    return stateOf(villager).gossips.reputation(playerUuid);
}

void releaseAllPois(ai::MobContext& mob, VillagerWorld& world) {
    Ctx ctx = contextFor(mob, world);
    brain::releasePoi(ctx, MemoryId::kHome);
    brain::releasePoi(ctx, MemoryId::kJobSite);
    brain::releasePoi(ctx, MemoryId::kPotentialJobSite);
    brain::releasePoi(ctx, MemoryId::kMeetingPoint);
}

void died(ai::MobContext& mob, VillagerWorld& world, const std::optional<std::string>& killerUuid) {
    Ctx ctx = contextFor(mob, world);
    // parity: `tellWitnessesThatIWasMurdered`, every villager it could see.
    if (killerUuid.has_value() && ctx.mem.visibleLivingEntities.present()) {
        for (const std::int32_t id : *ctx.mem.visibleLivingEntities.value) {
            if (VillagerState* witness = look::villagerState(ctx, id); witness != nullptr) {
                witness->gossips.add(*killerUuid, GossipType::kMajorNegative, Gossips::kPerEvent);
            }
        }
    }
    releaseAllPois(mob, world);
    stopTrading(ctx.self);
}

void tick(ai::MobContext& mob, VillagerWorld& world) {
    Ctx ctx = contextFor(mob, world);
    VillagerState& state = ctx.state;
    // parity: `ageBoundaryReached`, a baby grown up gets an adult's brain.
    if (state.refresh || state.table == nullptr || state.wasBaby != look::isBaby(ctx.self)) {
        rebuild(ctx);
    }
    noticeHurt(ctx);
    engine::run(ctx);
    if (state.refresh) {
        rebuild(ctx);
    }
    // parity: `customServerAiStep`, the trade reputation once the menu has had its turn.
    if (state.lastTradedPlayer.has_value()) {
        if (const std::optional<std::string> uuid = uuidOf(ctx, *state.lastTradedPlayer); uuid.has_value()) {
            state.gossips.add(*uuid, GossipType::kTrading, Gossips::kPerTrade);
        }
        ctx.world.entityEvent(ctx.self, 14);
        state.lastTradedPlayer.reset();
    }
    if (ctx.random.nextInt(100) == 0) {
        if (const std::optional<RaidView> raid = ctx.world.raidAt(ctx.self);
            raid.has_value() && raid->active && !raid->victory && !raid->loss) {
            ctx.world.entityEvent(ctx.self, 42);
        }
    }
    if (state.tradingPlayer.has_value() && (ctx.self.offers == nullptr || ctx.self.offers->list.empty())) {
        stopTrading(ctx.self);
    }
    pickUpItems(ctx);
    // parity: `Villager.tick` after the living entity's own turn.
    if (state.unhappyCounter > 0) {
        --state.unhappyCounter;
    }
    if (state.lastGossipDecay == 0) {
        state.lastGossipDecay = ctx.now;
    } else if (ctx.now >= state.lastGossipDecay + kGossipDecay) {
        state.gossips.decay();
        state.lastGossipDecay = ctx.now;
    }
}

} // namespace villagerBrain

} // namespace basalt::server::runtime::entity::villager
