#include "BrainInternals.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity::villager {
namespace {

namespace definitions = server::registry::blockStateDefinitions;
using server::world::loot::ItemStack;
using wire::common::Identifier;

[[nodiscard]] std::optional<std::int32_t> stateAt(Ctx& ctx, const BlockPos& pos) {
    return ctx.mob.world.blockState(pos.x, pos.y, pos.z);
}

[[nodiscard]] std::string_view pathOf(std::optional<std::int32_t> state) {
    const Identifier* block = state.has_value() ? definitions::blockOf(*state) : nullptr;
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

/// parity: `CropBlock` and its subclasses, the four vegetables and the torchflower.
[[nodiscard]] bool isCrop(std::string_view path) {
    return path == "wheat" || path == "carrots" || path == "potatoes" || path == "beetroots" ||
           path == "torchflower_crop";
}

/// parity: `CropBlock.isMaxAge`.
[[nodiscard]] bool isMaxAge(std::int32_t state) {
    const Identifier* block = definitions::blockOf(state);
    const definitions::Definition* definition = block == nullptr ? nullptr : definitions::of(*block);
    if (definition == nullptr) {
        return false;
    }
    const std::map<std::string, std::string> properties = definition->propertiesOf(state);
    const auto age = properties.find("age");
    const std::vector<std::string>& ages = definition->valuesOf("age");
    return age != properties.end() && !ages.empty() && age->second == ages.back();
}

[[nodiscard]] bool farmland(Ctx& ctx, const BlockPos& pos) {
    return pathOf(stateAt(ctx, BlockPos{pos.x, pos.y - 1, pos.z})) == "farmland";
}

[[nodiscard]] bool isAir(std::optional<std::int32_t> state) {
    const std::string_view path = pathOf(state);
    return path == "air" || path == "cave_air" || path == "void_air";
}

/// parity: `ItemTags.VILLAGER_PLANTABLE_SEEDS` and the crop each one plants.
[[nodiscard]] std::string_view plantedBy(std::string_view item) {
    if (item == "wheat_seeds") {
        return "wheat";
    }
    if (item == "potato") {
        return "potatoes";
    }
    if (item == "carrot") {
        return "carrots";
    }
    if (item == "beetroot_seeds") {
        return "beetroots";
    }
    if (item == "torchflower_seeds") {
        return "torchflower_crop";
    }
    return item == "pitcher_pod" ? "pitcher_crop" : std::string_view{};
}

void setMainHand(Ctx& ctx, std::optional<ItemStack> stack) {
    ctx.state.mainHand = std::move(stack);
    ctx.world.mainHandChanged(ctx.self);
}

bool workCheck(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (!ctx.mem.jobSite.present()) {
        return false;
    }
    // parity: `CHECK_COOLDOWN`, then a coin toss.
    if (ctx.now - state.t0 < 300 || ctx.random.nextInt(2) != 0) {
        return false;
    }
    state.t0 = ctx.now;
    return look::closerToCenterThan(*ctx.mem.jobSite.value, ctx.self, 1.73);
}

bool workStill(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    return ctx.mem.jobSite.present() && look::closerToCenterThan(*ctx.mem.jobSite.value, ctx.self, 1.73);
}

/// parity: `WorkAtComposter.makeBread`.
void makeBread(Ctx& ctx) {
    if (villagerData::countItem(ctx.state, "bread") > 36) {
        return;
    }
    const std::int32_t loaves = std::min(3, villagerData::countItem(ctx.state, "wheat") / 3);
    if (loaves == 0) {
        return;
    }
    villagerData::removeItem(ctx.state, "wheat", loaves * 3);
    const std::int32_t left =
        villagerData::addItem(ctx.state, ItemStack{.item = Identifier::vanilla("bread"), .count = loaves});
    if (left > 0) {
        ctx.world.dropItem(ctx.self, ItemStack{.item = Identifier::vanilla("bread"), .count = left}, 0.5F);
    }
}

/// parity: `WorkAtComposter.compostItems`.
void compostItems(Ctx& ctx, const BlockPos& pos, std::int32_t level) {
    if (level == 8) {
        ctx.world.composterExtract(ctx.self, pos);
    }
    constexpr std::array<std::string_view, 2> kCompostable{"wheat_seeds", "beetroot_seeds"};
    std::array<std::int32_t, 2> seen{};
    std::int32_t budget = 20;
    std::int32_t reached = level == 8 ? 0 : level;
    const std::int32_t before = reached;
    for (auto slot = static_cast<std::int32_t>(ctx.state.inventory.size()) - 1; slot >= 0 && budget > 0; --slot) {
        std::optional<ItemStack>& stack = ctx.state.inventory[static_cast<std::size_t>(slot)];
        if (!stack.has_value()) {
            continue;
        }
        const auto kind = std::ranges::find(kCompostable, std::string_view{stack->item.path()});
        if (kind == kCompostable.end()) {
            continue;
        }
        const auto index = static_cast<std::size_t>(kind - kCompostable.begin());
        const std::int32_t size = stack->count;
        seen[index] += size;
        const std::int32_t use = std::min({seen[index] - 10, budget, size});
        if (use <= 0) {
            continue;
        }
        budget -= use;
        for (std::int32_t item = 0; item < use; ++item) {
            const std::optional<std::int32_t> after = ctx.world.composterAdd(ctx.self, pos, stack->item);
            stack->count -= 1;
            if (after.has_value()) {
                reached = *after;
            }
            if (reached == 7) {
                if (stack->count <= 0) {
                    stack.reset();
                }
                ctx.world.levelEvent(ctx.self, 1500, pos, reached != before ? 1 : 0);
                return;
            }
        }
        if (stack->count <= 0) {
            stack.reset();
        }
    }
    ctx.world.levelEvent(ctx.self, 1500, pos, reached != before ? 1 : 0);
}

void workStart(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    ctx.mem.lastWorkedAtPoi.set(ctx.now);
    if (ctx.mem.jobSite.present()) {
        ctx.mem.lookTarget.set(Tracker::ofBlock(*ctx.mem.jobSite.value));
    }
    // parity: `VillagerProfession.workSound`.
    if (villagerData::hasJob(ctx.self)) {
        ctx.world.sound(ctx.self, "entity.villager.work_" + ctx.self.profession, 1.0F, 1.0F);
    }
    if (node.kind == Kind::kWorkAtComposter && ctx.mem.jobSite.present()) {
        const BlockPos pos = *ctx.mem.jobSite.value;
        const std::optional<std::int32_t> state = stateAt(ctx, pos);
        if (pathOf(state) == "composter") {
            makeBread(ctx);
            const std::map<std::string, std::string> properties = definitions::propertiesOf(*state);
            const auto level = properties.find("level");
            compostItems(ctx, pos, level == properties.end() ? 0 : std::stoi(level->second));
        }
    }
    if (trading::shouldRestock(ctx)) {
        trading::restock(ctx);
    }
}

[[nodiscard]] bool harvestable(Ctx& ctx, const BlockPos& pos) {
    const std::optional<std::int32_t> state = stateAt(ctx, pos);
    if (!state.has_value()) {
        return false;
    }
    if (isCrop(pathOf(state)) && isMaxAge(*state)) {
        return true;
    }
    return isAir(state) && farmland(ctx, pos);
}

bool harvestCheck(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (ctx.mem.lookTarget.present() || ctx.mem.walkTarget.present() || !ctx.mem.secondaryJobSite.present()) {
        return false;
    }
    if (!ctx.world.mobGriefing(ctx.self) || ctx.self.profession != "farmer") {
        return false;
    }
    NodeExtra& extra = extraOf(state);
    extra.positions.clear();
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                const BlockPos pos{static_cast<std::int32_t>(std::floor(ctx.self.x + dx)),
                                   static_cast<std::int32_t>(std::floor(ctx.self.y + dy)),
                                   static_cast<std::int32_t>(std::floor(ctx.self.z + dz))};
                if (harvestable(ctx, pos)) {
                    extra.positions.push_back(pos);
                }
            }
        }
    }
    state.pos.reset();
    if (!extra.positions.empty()) {
        state.pos = extra.positions[static_cast<std::size_t>(
            ctx.random.nextInt(static_cast<std::int32_t>(extra.positions.size())))];
    }
    return state.pos.has_value();
}

void harvestStart(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (ctx.now > state.t0 && state.pos.has_value()) {
        ctx.mem.lookTarget.set(Tracker::ofBlock(*state.pos));
        ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofBlock(*state.pos), .speed = 0.5F, .closeEnough = 1});
    }
}

void harvestStop(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    ctx.mem.lookTarget.erase();
    ctx.mem.walkTarget.erase();
    state.i0 = 0;
    state.t0 = ctx.now + 40;
}

bool harvestStill(Ctx& /*ctx*/, const Node& /*node*/, NodeState& state) { return state.i0 < 200; }

void harvestTick(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (state.pos.has_value() && !look::closerToCenterThan(*state.pos, ctx.self, 1.0)) {
        return;
    }
    if (state.pos.has_value() && ctx.now > state.t0) {
        const BlockPos pos = *state.pos;
        const std::optional<std::int32_t> blockState = stateAt(ctx, pos);
        const std::string_view path = pathOf(blockState);
        const bool crop = isCrop(path);
        if (crop && blockState.has_value() && isMaxAge(*blockState)) {
            ctx.world.destroyBlock(ctx.self, pos);
        }
        // parity: what was there before the harvest decides whether it is replanted this turn.
        if (isAir(blockState) && farmland(ctx, pos)) {
            for (std::optional<ItemStack>& slot : ctx.state.inventory) {
                if (!slot.has_value()) {
                    continue;
                }
                const std::string_view planted = plantedBy(slot->item.path());
                if (planted.empty()) {
                    continue;
                }
                if (const std::optional<std::int32_t> seed = server::registry::blockStateDefinitions::stateIdOf(
                        Identifier::vanilla(std::string{planted}), {});
                    seed.has_value()) {
                    static_cast<void>(ctx.world.setBlock(ctx.self, pos, *seed));
                    ctx.world.blockSound(ctx.self, pos, "item.crop.plant");
                    slot->count -= 1;
                    if (slot->count <= 0) {
                        slot.reset();
                    }
                    break;
                }
            }
        }
        if (crop && blockState.has_value() && !isMaxAge(*blockState)) {
            NodeExtra& extra = extraOf(state);
            std::erase(extra.positions, pos);
            state.pos.reset();
            if (!extra.positions.empty()) {
                state.pos = extra.positions[static_cast<std::size_t>(
                    ctx.random.nextInt(static_cast<std::int32_t>(extra.positions.size())))];
                state.t0 = ctx.now + 20;
                ctx.mem.walkTarget.set(
                    WalkTarget{.target = Tracker::ofBlock(*state.pos), .speed = 0.5F, .closeEnough = 1});
                ctx.mem.lookTarget.set(Tracker::ofBlock(*state.pos));
            }
        }
    }
    ++state.i0;
}

[[nodiscard]] std::optional<BlockPos> nextCrop(Ctx& ctx) {
    const BlockPos center = look::blockPosition(ctx.self);
    std::optional<BlockPos> result;
    std::int32_t count = 0;
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                const BlockPos pos{center.x + dx, center.y + dy, center.z + dz};
                const std::optional<std::int32_t> state = stateAt(ctx, pos);
                if (state.has_value() && isCrop(pathOf(state)) && !isMaxAge(*state)) {
                    ++count;
                    if (ctx.random.nextInt(count) == 0) {
                        result = pos;
                    }
                }
            }
        }
    }
    return result;
}

void targetCrop(Ctx& ctx, NodeState& state) {
    if (state.pos.has_value()) {
        const Tracker crop = Tracker::ofBlock(*state.pos);
        ctx.mem.lookTarget.set(crop);
        ctx.mem.walkTarget.set(WalkTarget{.target = crop, .speed = 0.5F, .closeEnough = 1});
    }
}

bool bonemealCheck(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (ctx.mem.lookTarget.present() || ctx.mem.walkTarget.present()) {
        return false;
    }
    // parity: every tenth tick, and eight seconds after the last session.
    if (ctx.self.tickCount % 10 != 0 || (state.t1 != 0 && state.t1 + 160 > ctx.self.tickCount)) {
        return false;
    }
    if (villagerData::countItem(ctx.state, "bone_meal") <= 0) {
        return false;
    }
    state.pos = nextCrop(ctx);
    return state.pos.has_value();
}

bool bonemealStill(Ctx& /*ctx*/, const Node& /*node*/, NodeState& state) {
    return state.i0 < 80 && state.pos.has_value();
}

void bonemealStart(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    targetCrop(ctx, state);
    setMainHand(ctx, ItemStack{.item = Identifier::vanilla("bone_meal"), .count = 1});
    state.t0 = ctx.now;
    state.i0 = 0;
}

void bonemealStop(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    setMainHand(ctx, std::nullopt);
    state.t1 = ctx.self.tickCount;
}

void bonemealTick(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (!state.pos.has_value()) {
        return;
    }
    const BlockPos target = *state.pos;
    if (ctx.now < state.t0 || !look::closerToCenterThan(target, ctx.self, 1.0)) {
        return;
    }
    if (villagerData::countItem(ctx.state, "bone_meal") > 0 && ctx.world.growCrop(ctx.self, target)) {
        villagerData::removeItem(ctx.state, "bone_meal", 1);
        ctx.world.levelEvent(ctx.self, 1505, target, 15);
        state.pos = nextCrop(ctx);
        targetCrop(ctx, state);
        state.t0 = ctx.now + 40;
    }
    ++state.i0;
}

bool showCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.interactionTarget.present()) {
        return false;
    }
    const std::int32_t target = *ctx.mem.interactionTarget.value;
    return look::player(ctx, target) != nullptr && !look::isBaby(ctx.self) && look::distanceSqr(ctx, target) <= 17.0;
}

void clearHeld(Ctx& ctx) {
    if (ctx.state.mainHand.has_value()) {
        setMainHand(ctx, std::nullopt);
    }
}

void showStart(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    ctx.mem.lookTarget.set(Tracker::ofEntity(*ctx.mem.interactionTarget.value, true));
    state.i0 = 0;
    state.i1 = 0;
    state.i2 = 40;
}

bool showStill(Ctx& ctx, const Node& node, NodeState& state) {
    return showCheck(ctx, node, state) && state.i2 > 0 && ctx.mem.interactionTarget.present();
}

void showTick(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    const std::int32_t target = *ctx.mem.interactionTarget.value;
    ctx.mem.lookTarget.set(Tracker::ofEntity(target, true));
    NodeExtra& extra = extraOf(state);
    // parity: `findItemsToDisplay`: a new item in the player's hand starts the list over.
    const std::string held = ctx.world.playerMainHand(target).value_or(std::string{});
    if (!extra.shownFor.has_value() || *extra.shownFor != held) {
        extra.shownFor = held;
        extra.display.clear();
        if (!held.empty()) {
            if (std::vector<world::trades::Offer>* offers = ctx.world.offers(ctx.self); offers != nullptr) {
                for (const world::trades::Offer& offer : *offers) {
                    const bool pays = offer.costA.item.toString() == held ||
                                      (offer.costB.has_value() && offer.costB->item.toString() == held);
                    if (!offer.outOfStock() && pays) {
                        extra.display.push_back(offer.result);
                    }
                }
            }
            if (!extra.display.empty()) {
                state.i2 = 900;
                setMainHand(ctx, extra.display.front());
            }
        }
    }
    if (!extra.display.empty()) {
        if (extra.display.size() >= 2 && ++state.i0 >= 40) {
            ++state.i1;
            state.i0 = 0;
            if (state.i1 > static_cast<std::int32_t>(extra.display.size()) - 1) {
                state.i1 = 0;
            }
            setMainHand(ctx, extra.display[static_cast<std::size_t>(state.i1)]);
        }
    } else {
        clearHeld(ctx);
        state.i2 = std::min(state.i2, 40);
    }
    --state.i2;
}

void showStop(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    ctx.mem.interactionTarget.erase();
    clearHeld(ctx);
    extraOf(state).shownFor.reset();
}

bool setLookAndInteract(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.interactionTarget.present() || !ctx.mem.visibleLivingEntities.present()) {
        return false;
    }
    const std::optional<std::int32_t> closest =
        look::closestVisible(ctx, node.filter, static_cast<double>(node.a * node.a));
    if (!closest.has_value()) {
        return false;
    }
    ctx.mem.interactionTarget.set(*closest);
    ctx.mem.lookTarget.set(Tracker::ofEntity(*closest, true));
    return true;
}

[[nodiscard]] std::optional<std::int32_t> hero(Ctx& ctx) {
    if (!ctx.mem.nearestVisiblePlayer.present()) {
        return std::nullopt;
    }
    const std::int32_t player = *ctx.mem.nearestVisiblePlayer.value;
    return ctx.world.heroLevel(player).has_value() ? std::optional<std::int32_t>{player} : std::nullopt;
}

bool giftCheck(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (state.t1 == 0) {
        // parity: the first gift waits the same thirty seconds as every later one at least.
        state.i0 = 600;
        state.t1 = 1;
    }
    if (!hero(ctx).has_value()) {
        return false;
    }
    if (state.i0 > 0) {
        --state.i0;
        return false;
    }
    return true;
}

void giftStart(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    state.i1 = 0;
    state.t0 = ctx.now;
    const std::int32_t player = *hero(ctx);
    ctx.mem.interactionTarget.set(player);
    ctx.mem.lookTarget.set(Tracker::ofEntity(player, true));
}

bool giftStill(Ctx& ctx, const Node& /*node*/, NodeState& state) { return hero(ctx).has_value() && state.i1 == 0; }

void giftTick(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    const std::int32_t player = *hero(ctx);
    ctx.mem.lookTarget.set(Tracker::ofEntity(player, true));
    const std::optional<std::array<double, 3>> at = look::feet(ctx, player);
    if (!at.has_value()) {
        return;
    }
    const BlockPos mine = look::blockPosition(ctx.self);
    const BlockPos theirs{static_cast<std::int32_t>(std::floor((*at)[0])),
                          static_cast<std::int32_t>(std::floor((*at)[1])),
                          static_cast<std::int32_t>(std::floor((*at)[2]))};
    const auto dx = static_cast<double>(mine.x - theirs.x);
    const auto dy = static_cast<double>(mine.y - theirs.y);
    const auto dz = static_cast<double>(mine.z - theirs.z);
    if ((dx * dx) + (dy * dy) + (dz * dz) < 25.0) {
        if (ctx.now - state.t0 > 20) {
            std::string table = "gameplay/hero_of_the_village/unemployed_gift";
            if (look::isBaby(ctx.self)) {
                table = "gameplay/hero_of_the_village/baby_gift";
            } else if (villagerData::hasJob(ctx.self)) {
                table = "gameplay/hero_of_the_village/" + ctx.self.profession + "_gift";
            }
            ctx.world.throwGift(ctx.self, table, player);
            state.i1 = 1;
        }
    } else {
        look::walkAndLook(ctx, Tracker::ofEntity(player, true), 0.5F, 5);
    }
}

void giftStop(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    state.i0 = 600 + ctx.random.nextInt(6001);
    ctx.mem.interactionTarget.erase();
    ctx.mem.walkTarget.erase();
    ctx.mem.lookTarget.erase();
}

[[nodiscard]] ServerEntity* partner(Ctx& ctx) {
    if (!ctx.mem.interactionTarget.present()) {
        return nullptr;
    }
    const std::int32_t id = *ctx.mem.interactionTarget.value;
    ServerEntity* other = look::mob(ctx, id);
    // parity: `BehaviorUtils.targetIsValid`: a villager, alive, and seen.
    if (other == nullptr || other->type().path() != "villager" || !look::alive(ctx, id) || !ctx.mem.visible(id)) {
        return nullptr;
    }
    return other;
}

bool tradeCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    return ctx.mem.visibleLivingEntities.present() && partner(ctx) != nullptr;
}

/// parity: `VillagerProfession.requestedItems`, which only the farmer has.
[[nodiscard]] bool requested(const ServerEntity& villager, std::string_view item) {
    return villager.profession == "farmer" &&
           (item == "wheat" || item == "wheat_seeds" || item == "beetroot_seeds" || item == "bone_meal");
}

void tradeStart(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    ServerEntity* other = partner(ctx);
    look::lockGaze(ctx, *other, 0.5F, 2);
    // parity: `figureOutWhatIAmWillingToTrade`, what they ask for that this one does not.
    state.i0 = other->profession == "farmer" && ctx.self.profession != "farmer" ? 1 : 0;
}

/// parity: `TradeWithVillager.throwHalfStack`.
template <typename Predicate>
void throwHalfStack(Ctx& ctx, const ServerEntity& target, Predicate&& predicate) {
    for (std::optional<ItemStack>& slot : ctx.state.inventory) {
        if (!slot.has_value() || !predicate(*slot)) {
            continue;
        }
        const std::int32_t max = std::max(server::registry::itemStacking::maxStackSize(slot->item), 1);
        std::int32_t count = 0;
        if (slot->count > max / 2) {
            count = slot->count / 2;
        } else if (slot->count > 24) {
            count = slot->count - 24;
        } else {
            continue;
        }
        ItemStack thrown = *slot;
        thrown.count = count;
        slot->count -= count;
        if (slot->count <= 0) {
            slot.reset();
        }
        ctx.world.throwItem(ctx.self, thrown, target.x, target.y, target.z);
        return;
    }
}

void tradeTick(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    ServerEntity* other = partner(ctx);
    if (other == nullptr || look::distanceSqr(ctx, other->entityId()) > 5.0) {
        return;
    }
    look::lockGaze(ctx, *other, 0.5F, 2);
    brain::gossip(ctx, *other);
    const bool farmer = ctx.self.profession == "farmer";
    const VillagerState& theirs = villagerBrain::stateOf(*other);
    const auto isFood = [](const ItemStack& stack) {
        const std::string& path = stack.item.path();
        return path == "bread" || path == "carrot" || path == "potato" || path == "beetroot";
    };
    if (villagerData::foodPoints(ctx.state) >= 24 && (farmer || villagerData::foodPoints(theirs) < 12)) {
        throwHalfStack(ctx, *other, isFood);
    }
    if (farmer && villagerData::countItem(ctx.state, "wheat") > 32) {
        throwHalfStack(ctx, *other, [](const ItemStack& stack) { return stack.item.path() == "wheat"; });
    }
    if (state.i0 == 1) {
        throwHalfStack(ctx, *other, [&ctx, other](const ItemStack& stack) {
            return requested(*other, stack.item.path()) && !requested(ctx.self, stack.item.path());
        });
    }
}

void tradeStop(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) { ctx.mem.interactionTarget.erase(); }

constexpr Ops kWork{.check = workCheck, .start = workStart, .canStillUse = workStill};
constexpr Ops kHarvest{.check = harvestCheck,
                       .start = harvestStart,
                       .canStillUse = harvestStill,
                       .tick = harvestTick,
                       .stop = harvestStop};
constexpr Ops kBonemeal{.check = bonemealCheck,
                        .start = bonemealStart,
                        .canStillUse = bonemealStill,
                        .tick = bonemealTick,
                        .stop = bonemealStop};
constexpr Ops kShow{
    .check = showCheck, .start = showStart, .canStillUse = showStill, .tick = showTick, .stop = showStop};
constexpr Ops kLookAndInteract{.trigger = setLookAndInteract};
constexpr Ops kGift{
    .check = giftCheck, .start = giftStart, .canStillUse = giftStill, .tick = giftTick, .stop = giftStop};
constexpr Ops kTrade{
    .check = tradeCheck, .start = tradeStart, .canStillUse = tradeCheck, .tick = tradeTick, .stop = tradeStop};

} // namespace

const Ops* ops::work(Kind kind) {
    switch (kind) {
    case Kind::kWorkAtPoi:
    case Kind::kWorkAtComposter:
        return &kWork;
    case Kind::kHarvestFarmland:
        return &kHarvest;
    case Kind::kUseBonemeal:
        return &kBonemeal;
    case Kind::kShowTrades:
        return &kShow;
    case Kind::kSetLookAndInteract:
        return &kLookAndInteract;
    case Kind::kGiveGiftToHero:
        return &kGift;
    case Kind::kTradeWithVillager:
        return &kTrade;
    default:
        return nullptr;
    }
}

} // namespace basalt::server::runtime::entity::villager
