#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/VillagerWorldSystem.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace reg = server::registry;
using entity::ServerEntity;
using entity::villager::BlockPos;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SoundSource;

/// parity: `ItemEntity.setDefaultPickUpDelay`.
constexpr std::int32_t kPickUpDelay = 10;
/// parity: `ItemEntity`'s own constructor, a fifth of a block a tick upwards.
constexpr double kPoppedUpwards = 0.2;
/// parity: the villager's eye, where a thrown item leaves from, less the hand's three tenths.
constexpr double kVillagerEye = 1.62;
constexpr double kThrowDrop = 0.3;
/// parity: `BellBlockEntity`'s alarm reach.
constexpr double kBellAlarmRange = 32.0;

[[nodiscard]] std::map<std::string, std::string> propertiesAt(world::LoadedWorld& world, const BlockPos& pos,
                                                              std::int32_t& stateId) {
    const std::optional<std::int32_t> state = world.state().blockState(pos.x, pos.y, pos.z);
    stateId = state.value_or(0);
    return state.has_value() ? reg::blockStateDefinitions::propertiesOf(*state) : std::map<std::string, std::string>{};
}

/// parity: `BedBlock.getConnectedDirection` from the head, which is the foot.
[[nodiscard]] std::optional<BlockPos> footOf(world::LoadedWorld& world, const BlockPos& head) {
    std::int32_t stateId = 0;
    const std::map<std::string, std::string> properties = propertiesAt(world, head, stateId);
    const auto facing = properties.find("facing");
    if (facing == properties.end()) {
        return std::nullopt;
    }
    BlockPos foot = head;
    if (facing->second == "north") {
        foot.z += 1;
    } else if (facing->second == "south") {
        foot.z -= 1;
    } else if (facing->second == "west") {
        foot.x += 1;
    } else {
        foot.x -= 1;
    }
    return foot;
}

} // namespace

VillagerWorldSystem::VillagerWorldSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                                         NaturalSpawningSystem& spawning, RaidSystem& raids,
                                         world::RealLootTables& lootTables, world::RealVillagerTrades* trades,
                                         std::function<std::int64_t()> nextSeed, AddEffect addEffect,
                                         LaunchFirework launchFirework)
    : players_(&players), broadcaster_(&broadcaster), spawning_(&spawning), raids_(&raids), lootTables_(&lootTables),
      trades_(trades), nextSeed_(std::move(nextSeed)), addEffect_(std::move(addEffect)),
      launchFirework_(std::move(launchFirework)) {}

std::vector<PlayerSession*> VillagerWorldSystem::watchers() const {
    std::vector<PlayerSession*> found;
    if (world_ == nullptr) {
        return found;
    }
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world_->name()) {
            found.push_back(player);
        }
    }
    return found;
}

PlayerSession* VillagerWorldSystem::playerById(std::int32_t playerId) const {
    for (PlayerSession* player : players_->snapshot()) {
        if (player->entityId() == playerId) {
            return player;
        }
    }
    return nullptr;
}

void VillagerWorldSystem::announce(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                                   bool neighbours) {
    const std::vector<PlayerSession*> seen = watchers();
    broadcaster_->announceBlock(seen, world::BlockChange{.x = x, .y = y, .z = z, .stateId = stateId});
    if (neighbours) {
        broadcaster_->notifyAround(*world_, seen, x, y, z, std::nullopt);
    } else {
        broadcaster_->reshapeAround(*world_, seen, x, y, z);
    }
}

std::optional<entity::villager::RaidView> VillagerWorldSystem::raidAt(const ServerEntity& villager) {
    if (world_ == nullptr) {
        return std::nullopt;
    }
    const BlockPos pos{static_cast<std::int32_t>(std::floor(villager.x)),
                       static_cast<std::int32_t>(std::floor(villager.y)),
                       static_cast<std::int32_t>(std::floor(villager.z))};
    return raids_->viewAt(*world_, pos.x, pos.y, pos.z);
}

void VillagerWorldSystem::entityEvent(ServerEntity& mob, std::int8_t event) {
    const wire::protocol::play::EntityEventPacket packet{.entityId = mob.entityId(), .eventId = event};
    for (PlayerSession* player : watchers()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void VillagerWorldSystem::sound(ServerEntity& mob, std::string_view sound, float volume, float pitch) {
    broadcaster_->sendEffects(
        watchers(), static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
        static_cast<std::int32_t>(std::floor(mob.z)), reg::blocks::air().stateId,
        {world::blockEffect::Sound{
            .name = std::string{sound}, .source = SoundSource::Neutral, .volume = volume, .pitch = pitch}});
}

void VillagerWorldSystem::blockSound(ServerEntity& /*mob*/, const BlockPos& pos, std::string_view sound) {
    broadcaster_->sendEffects(watchers(), pos.x, pos.y, pos.z, reg::blocks::air().stateId,
                              {world::blockEffect::Sound{.name = std::string{sound}, .source = SoundSource::Blocks}});
}

void VillagerWorldSystem::levelEvent(ServerEntity& /*mob*/, std::int32_t event, const BlockPos& pos,
                                     std::int32_t data) {
    broadcaster_->sendEffects(watchers(), pos.x, pos.y, pos.z, reg::blocks::air().stateId,
                              {world::blockEffect::Effect{.type = event, .data = data}});
}

bool VillagerWorldSystem::setBlock(ServerEntity& /*mob*/, const BlockPos& pos, std::int32_t stateId) {
    if (world_ == nullptr || !world_->state().setBlockState(pos.x, pos.y, pos.z, stateId)) {
        return false;
    }
    announce(pos.x, pos.y, pos.z, stateId, true);
    return true;
}

void VillagerWorldSystem::destroyBlock(ServerEntity& /*mob*/, const BlockPos& pos) {
    if (world_ == nullptr) {
        return;
    }
    const std::optional<std::int32_t> was = world_->state().blockState(pos.x, pos.y, pos.z);
    if (!was.has_value() || reg::blocks::isAir(*was)) {
        return;
    }
    // parity: `Level.destroyBlock`: the break particles, the drops, then air.
    broadcaster_->sendEffects(watchers(), pos.x, pos.y, pos.z, *was,
                              {world::blockEffect::Effect{.type = 2001, .data = *was}});
    broadcaster_->dropBrokenBlock(*world_, pos.x, pos.y, pos.z, *was, true);
    if (world_->state().setBlockState(pos.x, pos.y, pos.z, reg::blocks::air().stateId)) {
        announce(pos.x, pos.y, pos.z, reg::blocks::air().stateId, true);
    }
}

void VillagerWorldSystem::setDoorOpen(ServerEntity& /*mob*/, const BlockPos& pos, bool open) {
    if (world_ == nullptr) {
        return;
    }
    std::int32_t stateId = 0;
    const std::map<std::string, std::string> properties = propertiesAt(*world_, pos, stateId);
    const auto current = properties.find("open");
    if (current == properties.end() || (current->second == "true") == open) {
        return;
    }
    const std::optional<world::blockInteractions::BlockToggle> toggled =
        world::blockInteractions::toggle(world_->state(), pos.x, pos.y, pos.z);
    if (!toggled.has_value()) {
        return;
    }
    for (const world::BlockChange& change : toggled->changes) {
        announce(change.x, change.y, change.z, change.stateId, false);
    }
    if (!toggled->sound.empty()) {
        broadcaster_->sendEffects(watchers(), pos.x, pos.y, pos.z, stateId,
                                  {world::blockEffect::Sound{.name = toggled->sound, .source = SoundSource::Blocks}});
    }
}

void VillagerWorldSystem::ringBell(ServerEntity& /*mob*/, const BlockPos& pos) {
    if (world_ == nullptr) {
        return;
    }
    std::int32_t stateId = 0;
    const std::map<std::string, std::string> properties = propertiesAt(*world_, pos, stateId);
    const auto facing = properties.find("facing");
    // parity: `BellBlock.attemptToRing` with no direction swings the way the bell faces.
    std::int32_t direction = 2;
    if (facing != properties.end()) {
        direction = facing->second == "south" ? 3 : facing->second == "west" ? 4 : facing->second == "east" ? 5 : 2;
    }
    broadcaster_->sendEffects(
        watchers(), pos.x, pos.y, pos.z, stateId,
        {world::blockEffect::Event{.b0 = 1, .b1 = direction},
         world::blockEffect::Sound{.name = "block.bell.use", .source = SoundSource::Blocks, .volume = 2.0F}});
    alertVillagers(*world_, pos.x, pos.y, pos.z);
}

void VillagerWorldSystem::alertVillagers(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (ServerEntity* entity : world.state().entities()) {
        if (entity == nullptr || entity->removed || entity->type().path() != "villager") {
            continue;
        }
        const double dx = x + 0.5 - entity->x;
        const double dy = y + 0.5 - entity->y;
        const double dz = z + 0.5 - entity->z;
        if ((dx * dx) + (dy * dy) + (dz * dz) < kBellAlarmRange * kBellAlarmRange) {
            entity::villager::villagerBrain::heardBell(*entity, world.gameTime());
        }
    }
}

std::vector<world::trades::Offer>* VillagerWorldSystem::offers(ServerEntity& mob) {
    if (world_ == nullptr || trades_ == nullptr) {
        return nullptr;
    }
    return &entity::villagers::offersOf(*trades_, mob, *world_);
}

std::optional<std::int32_t> VillagerWorldSystem::composterAdd(ServerEntity& /*mob*/, const BlockPos& pos,
                                                              const Identifier& item) {
    if (world_ == nullptr) {
        return std::nullopt;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed_ ? nextSeed_() : 0);
    if (const std::optional<world::BlockChange> change =
            world::composters::addItem(world_->state(), pos.x, pos.y, pos.z, item, random);
        change.has_value()) {
        announce(change->x, change->y, change->z, change->stateId, true);
    }
    return world::composters::levelAt(world_->state(), pos.x, pos.y, pos.z);
}

void VillagerWorldSystem::composterExtract(ServerEntity& /*mob*/, const BlockPos& pos) {
    if (world_ == nullptr) {
        return;
    }
    const std::optional<world::BlockChange> change = world::composters::empty(world_->state(), pos.x, pos.y, pos.z);
    if (!change.has_value()) {
        return;
    }
    announce(change->x, change->y, change->z, change->stateId, true);
    random::RandomSource random = random::RandomSource::legacy(nextSeed_ ? nextSeed_() : 0);
    // parity: `ComposterBlock.extractProduce`.
    const auto offset = [&random] { return (static_cast<double>(random.nextFloat()) - 0.5) * 0.7; };
    const double x = pos.x + 0.5 + offset();
    const double y = pos.y + 1.01 + offset();
    const double z = pos.z + 0.5 + offset();
    spawnItem(server::world::loot::ItemStack{.item = Identifier::vanilla("bone_meal"), .count = 1}, x, y, z,
              (random.nextDouble() * 0.2) - 0.1, kPoppedUpwards, (random.nextDouble() * 0.2) - 0.1);
    broadcaster_->sendEffects(
        watchers(), pos.x, pos.y, pos.z, change->stateId,
        {world::blockEffect::Sound{.name = "block.composter.empty", .source = SoundSource::Blocks}});
}

bool VillagerWorldSystem::growCrop(ServerEntity& /*mob*/, const BlockPos& pos) {
    if (world_ == nullptr) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed_ ? nextSeed_() : 0);
    const std::optional<world::BlockChange> change =
        world::boneMeal::apply(world_->state(), pos.x, pos.y, pos.z, random);
    if (!change.has_value()) {
        return false;
    }
    announce(change->x, change->y, change->z, change->stateId, true);
    return true;
}

void VillagerWorldSystem::spawnItem(const server::world::loot::ItemStack& stack, double x, double y, double z,
                                    double velocityX, double velocityY, double velocityZ,
                                    std::optional<wire::common::Uuid> thrower) {
    if (world_ == nullptr || reg::items::registry().byName(stack.item) == nullptr || stack.count <= 0) {
        return;
    }
    world::DroppedItem dropped{.entityId = players_->allocateEntityId(),
                               .uuid = wire::common::Uuid::random(),
                               .item = stack.item,
                               .count = stack.count,
                               .x = x,
                               .y = y,
                               .z = z,
                               .velocityX = velocityX,
                               .velocityY = velocityY,
                               .velocityZ = velocityZ};
    dropped.pickupDelay = kPickUpDelay;
    dropped.components = stack.components;
    dropped.thrower = thrower;
    static_cast<void>(world_->state().spawnItem(std::move(dropped)));
}

void VillagerWorldSystem::throwItem(ServerEntity& mob, const server::world::loot::ItemStack& stack, double towardsX,
                                    double towardsY, double towardsZ) {
    // parity: `BehaviorUtils.throwItem`.
    double dx = towardsX - mob.x;
    double dy = towardsY - mob.y;
    double dz = towardsZ - mob.z;
    const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    if (length > 1.0e-4) {
        dx /= length;
        dy /= length;
        dz /= length;
    } else {
        dx = 0.0;
        dy = 0.0;
        dz = 0.0;
    }
    constexpr double kThrow = static_cast<double>(0.3F);
    spawnItem(stack, mob.x, mob.y + kVillagerEye - kThrowDrop, mob.z, dx * kThrow, dy * kThrow, dz * kThrow,
              mob.uuid());
}

void VillagerWorldSystem::dropItem(ServerEntity& mob, const server::world::loot::ItemStack& stack, float offsetY) {
    random::RandomSource random = random::RandomSource::legacy(nextSeed_ ? nextSeed_() : 0);
    spawnItem(stack, mob.x, mob.y + static_cast<double>(offsetY), mob.z, (random.nextDouble() * 0.2) - 0.1,
              kPoppedUpwards, (random.nextDouble() * 0.2) - 0.1);
}

void VillagerWorldSystem::throwGift(ServerEntity& mob, std::string_view lootTable, std::int32_t targetId) {
    if (world_ == nullptr) {
        return;
    }
    const server::world::loot::LootTable* table = lootTables_->byId(Identifier::vanilla(std::string{lootTable}));
    PlayerSession* target = playerById(targetId);
    if (table == nullptr || target == nullptr) {
        return;
    }
    server::world::loot::LootContext context;
    lootTables_->prepareAt(context, *world_, mob.x, mob.y, mob.z);
    context.hasEntity = true;
    random::RandomSource scratch = random::RandomSource::legacy(nextSeed_ ? nextSeed_() : 0);
    random::RandomSource& random =
        table->randomSequence.has_value() ? world_->randomSequences().of(*table->randomSequence) : scratch;
    for (const server::world::loot::ItemStack& stack : table->generate(random, 0.0F, std::nullopt, context)) {
        throwItem(mob, stack, target->x, target->y, target->z);
    }
}

void VillagerWorldSystem::setSleeping(ServerEntity& mob, std::optional<BlockPos> bed) {
    if (world_ == nullptr) {
        return;
    }
    const auto setOccupied = [this](const BlockPos& pos, bool occupied) {
        std::int32_t stateId = 0;
        static_cast<void>(propertiesAt(*world_, pos, stateId));
        const reg::blockStateDefinitions::Definition* definition = nullptr;
        if (const Identifier* block = reg::blockStateDefinitions::blockOf(stateId); block != nullptr) {
            definition = reg::blockStateDefinitions::of(*block);
            if (definition != nullptr && block->path().ends_with("_bed")) {
                const std::optional<std::int32_t> next =
                    reg::blockStateDefinitions::stateIdOf(*block, {{"occupied", occupied ? "true" : "false"}}, stateId);
                if (next.has_value() && *next != stateId && world_->state().setBlockState(pos.x, pos.y, pos.z, *next)) {
                    announce(pos.x, pos.y, pos.z, *next, true);
                }
            }
        }
    };
    const auto sendSleep = [this, &mob](bool sleeping, std::optional<BlockPos> at) {
        std::optional<wire::protocol::play::BlockPos> wirePos;
        if (at.has_value()) {
            wirePos = wire::protocol::play::BlockPos{.x = at->x, .y = at->y, .z = at->z};
        }
        const SetEntityDataPacket packet{
            .entityId = mob.entityId(),
            .entries = {
                SetEntityDataPacket::Entry{
                    .index = SetEntityDataPacket::kPoseIndex,
                    .value = SetEntityDataPacket::PoseValue{.pose = sleeping ? wire::protocol::play::Pose::Sleeping
                                                                             : wire::protocol::play::Pose::Standing}},
                SetEntityDataPacket::Entry{.index = SetEntityDataPacket::kSleepingPosIndex,
                                           .value = SetEntityDataPacket::OptionalBlockPosValue{.pos = wirePos}}}};
        for (PlayerSession* player : watchers()) {
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
    };
    if (bed.has_value()) {
        // parity: `LivingEntity.startSleeping`: onto the bed, and the bed taken.
        mob.x = bed->x + 0.5;
        mob.y = bed->y + 0.6875;
        mob.z = bed->z + 0.5;
        mob.velocityX = 0.0;
        mob.velocityY = 0.0;
        mob.velocityZ = 0.0;
        entity::ai::navigation::stop(mob);
        mob.sleeping = true;
        mob.homePosition.reset();
        setOccupied(*bed, true);
        if (const std::optional<BlockPos> foot = footOf(*world_, *bed); foot.has_value()) {
            setOccupied(*foot, true);
        }
        mob.sleepingAt = std::array<std::int32_t, 3>{bed->x, bed->y, bed->z};
        sendSleep(true, bed);
        return;
    }
    if (!mob.sleeping) {
        return;
    }
    mob.sleeping = false;
    if (mob.sleepingAt.has_value()) {
        const BlockPos head{(*mob.sleepingAt)[0], (*mob.sleepingAt)[1], (*mob.sleepingAt)[2]};
        setOccupied(head, false);
        const std::optional<BlockPos> foot = footOf(*world_, head);
        if (foot.has_value()) {
            setOccupied(*foot, false);
        }
        // parity: `BedBlock.findStandUpPosition`, a free spot beside the bed, else on top of it.
        std::optional<BlockPos> stand;
        for (const BlockPos& part : {head, foot.value_or(head)}) {
            for (const auto& [dx, dz] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
                if (!stand.has_value() &&
                    entity::pathFinder::canStandAt(world_->state(), part.x + dx, part.y, part.z + dz)) {
                    stand = BlockPos{part.x + dx, part.y, part.z + dz};
                }
            }
        }
        if (stand.has_value()) {
            mob.x = stand->x + 0.5;
            mob.y = stand->y;
            mob.z = stand->z + 0.5;
        } else {
            mob.x = head.x + 0.5;
            mob.y = head.y + 1.1;
            mob.z = head.z + 0.5;
        }
    }
    mob.sleepingAt.reset();
    sendSleep(false, std::nullopt);
}

void VillagerWorldSystem::villagerDataChanged(ServerEntity& mob) {
    if (!mob.sourceNbt.has_value()) {
        return;
    }
    std::vector<SetEntityDataPacket::Entry> entries;
    for (const SetEntityDataPacket::Entry& entry : entity::generatedEntityMetadata(mob.type().path(), *mob.sourceNbt)) {
        if (std::holds_alternative<SetEntityDataPacket::VillagerDataValue>(entry.value)) {
            entries.push_back(entry);
        }
    }
    if (entries.empty()) {
        return;
    }
    const SetEntityDataPacket packet{.entityId = mob.entityId(), .entries = std::move(entries)};
    for (PlayerSession* player : watchers()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void VillagerWorldSystem::unhappyChanged(ServerEntity& mob, std::int32_t counter) {
    // parity: `AbstractVillager.DATA_UNHAPPY_COUNTER`, index eighteen.
    const SetEntityDataPacket packet{
        .entityId = mob.entityId(),
        .entries = {SetEntityDataPacket::Entry{.index = 18, .value = SetEntityDataPacket::IntValue{.value = counter}}}};
    for (PlayerSession* player : watchers()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void VillagerWorldSystem::addEffect(ServerEntity& mob, std::string_view effect, std::int32_t duration,
                                    std::int32_t amplifier) {
    if (addEffect_ && world_ != nullptr) {
        addEffect_(*world_, watchers(), mob,
                   world::ActiveEffect{.effect = Identifier::vanilla(std::string{effect}),
                                       .amplifier = amplifier,
                                       .remainingTicks = duration});
    }
}

void VillagerWorldSystem::mainHandChanged(ServerEntity& mob) {
    using wire::protocol::play::SetEquipmentPacket;
    const std::optional<server::world::loot::ItemStack>& held = entity::villager::villagerBrain::stateOf(mob).mainHand;
    const reg::Item* item = held.has_value() ? reg::items::registry().byName(held->item) : nullptr;
    SetEquipmentPacket::Slot slot{.type = SetEquipmentPacket::SlotType::MainHand};
    if (item != nullptr) {
        slot.stack = wire::protocol::play::WireItemStack{.itemId = item->protocolId, .count = held->count};
    }
    for (PlayerSession* player : watchers()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEquipmentPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetEquipmentPacket{.entityId = mob.entityId(), .slots = {slot}}.write(buffer);
            });
        }
    }
}

bool VillagerWorldSystem::summonGolem(ServerEntity& mob) {
    if (world_ == nullptr) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed_ ? nextSeed_() : 0);
    return spawning_->summonVillageGolem(*world_, mob, random);
}

ServerEntity* VillagerWorldSystem::spawnChild(ServerEntity& mob, ServerEntity& partner) {
    if (world_ == nullptr) {
        return nullptr;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed_ ? nextSeed_() : 0);
    const auto typeOf = [](const ServerEntity& villager) {
        const wire::nbt::NbtCompound* data =
            villager.sourceNbt.has_value() ? villager.sourceNbt->getCompound("VillagerData") : nullptr;
        const std::string_view type = data == nullptr ? std::string_view{} : data->getString("type");
        return type.empty() ? std::string{"minecraft:plains"} : std::string{type};
    };
    // parity: `Villager.getBreedOffspring`.
    const double roll = random.nextDouble();
    std::string type = roll < 0.75 ? typeOf(mob) : typeOf(partner);
    if (roll < 0.5) {
        const std::optional<Identifier> biome = world::naturalSpawner::biomeAt(
            *world_, static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
            static_cast<std::int32_t>(std::floor(mob.z)));
        wire::nbt::NbtCompound byBiome;
        world::villagerSpawnFinalizer::beforeCommon("villager", biome.has_value() ? biome->path() : "plains",
                                                    world::MobSpawnReason::Natural, random, byBiome);
        if (const wire::nbt::NbtCompound* data = byBiome.getCompound("VillagerData"); data != nullptr) {
            type = std::string{data->getString("type", "minecraft:plains")};
        }
    }
    return spawning_->spawnMob(*world_, Identifier::vanilla("villager"), mob.x, mob.y, mob.z, 0.0F, "breeding",
                               [&type](ServerEntity& child) {
                                   child.profession = "none";
                                   child.villagerLevel = 1;
                                   child.age = -24000;
                                   wire::nbt::NbtCompound data;
                                   data.putString("type", type);
                                   data.putString("profession", "minecraft:none");
                                   data.putInt("level", 1);
                                   wire::nbt::NbtCompound source;
                                   source.set("VillagerData", wire::nbt::NbtTag::ofCompound(std::move(data)));
                                   source.putBoolean("IsBaby", true);
                                   child.sourceNbt = std::move(source);
                                   entity::villager::villagerBrain::stateOf(child).dataFinalized = true;
                               });
}

void VillagerWorldSystem::launchFirework(ServerEntity& mob, std::int32_t colour, std::int32_t flight) {
    // parity: `new FireworkRocketEntity(level, body, x, eyeY, z, firework)`.
    if (launchFirework_ && world_ != nullptr) {
        launchFirework_(*world_, entity::fireworkRockets::celebrationRocket(colour, flight), mob.x,
                        mob.y + entity::mobBreathing::eyeHeightOf(mob), mob.z, mob.entityId());
    }
}

std::optional<std::int32_t> VillagerWorldSystem::heroLevel(std::int32_t playerId) {
    const PlayerSession* player = playerById(playerId);
    if (player == nullptr) {
        return std::nullopt;
    }
    const auto found = player->effects.find(world::mobEffects::heroOfTheVillage());
    return found == player->effects.end() ? std::nullopt : std::optional<std::int32_t>{found->second.amplifier};
}

std::optional<std::string> VillagerWorldSystem::playerMainHand(std::int32_t playerId) {
    const PlayerSession* player = playerById(playerId);
    if (player == nullptr || !player->inventory.heldItem().has_value()) {
        return std::nullopt;
    }
    return player->inventory.heldItem()->id;
}

bool VillagerWorldSystem::canSeeSky(ServerEntity& /*mob*/, const BlockPos& pos) {
    // parity: `Level.canSeeSky`, full sky light at the block.
    return world_ != nullptr && world::worldLight::skyLight(world_->state(), pos.x, pos.y, pos.z) >= 15;
}

std::int32_t VillagerWorldSystem::motionBlockingHeight(ServerEntity& /*mob*/, std::int32_t x, std::int32_t z) {
    const server::world::Chunk* chunk = world_ == nullptr ? nullptr : world_->state().chunkIfLoaded(x >> 4, z >> 4);
    return chunk == nullptr ? 0 : chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::MotionBlocking);
}

bool VillagerWorldSystem::mobGriefing(ServerEntity& mob) {
    return world_ != nullptr &&
           entity::mobOverrides::griefing(mob, world_->gameRules().get(world::gameRules::kMobGriefing));
}

std::int64_t VillagerWorldSystem::currentDay(ServerEntity& /*mob*/) {
    // parity: the `overworld_day` timeline's period count, whole days on the overworld clock.
    constexpr std::int64_t kDay = 24000;
    return world_ == nullptr ? 0 : world_->clock().totalTicks() / kDay;
}

void VillagerWorldSystem::itemsNear(ServerEntity& mob, double horizontal, double vertical, std::vector<ItemView>& out) {
    out.clear();
    if (world_ == nullptr) {
        return;
    }
    constexpr double kItemHalf = 0.125;
    const double half = mob.width() / 2.0;
    for (const world::DroppedItem& item :
         world_->state().nearbyItems(mob.x, mob.y, mob.z, std::max(horizontal, vertical) + mob.height() + 1.0)) {
        // parity: the item's quarter-block box against the mob's grown by the reach.
        const bool inside =
            item.x + kItemHalf > mob.x - half - horizontal && item.x - kItemHalf < mob.x + half + horizontal &&
            item.z + kItemHalf > mob.z - half - horizontal && item.z - kItemHalf < mob.z + half + horizontal &&
            item.y + (2.0 * kItemHalf) > mob.y - vertical && item.y < mob.y + mob.height() + vertical;
        if (!inside) {
            continue;
        }
        out.push_back(ItemView{
            .id = item.entityId,
            .stack =
                server::world::loot::ItemStack{.item = item.item, .count = item.count, .components = item.components},
            .x = item.x,
            .y = item.y,
            .z = item.z,
            .pickable = item.canBePickedUpByMob()});
    }
}

std::int32_t VillagerWorldSystem::takeItem(ServerEntity& mob, std::int32_t itemId, std::int32_t count) {
    if (world_ == nullptr) {
        return 0;
    }
    std::optional<world::DroppedItem> removed = world_->state().removeItem(itemId);
    if (!removed.has_value()) {
        return 0;
    }
    const std::int32_t taken = std::min(count, removed->count);
    if (taken < removed->count) {
        removed->count -= taken;
        static_cast<void>(world_->state().spawnItem(*removed));
    }
    const wire::protocol::play::TakeItemEntityPacket packet{
        .collectedEntityId = itemId, .collectorEntityId = mob.entityId(), .pickupItemCount = taken};
    for (PlayerSession* player : watchers()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::TakeItemEntityPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
    return taken;
}

} // namespace basalt::server::runtime
