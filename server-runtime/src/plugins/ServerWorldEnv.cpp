#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/VillagerWorldSystem.hpp>
#include <basalt/server/runtime/command/BlockEditor.hpp>
#include <basalt/server/runtime/player/PlayerDigSpeed.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/ServerWorldEnv.hpp>
#include <basalt/server/runtime/world/Bells.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/WorldBorder.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace basalt::server::runtime::plugins {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using server::plugins::PluginBorder;
using server::plugins::PluginDrop;
using server::plugins::PluginParticle;
using wire::common::Identifier;

constexpr double kLevelEventRange = 64.0;
constexpr double kSoundBaseRange = 16.0;
constexpr double kParticleRange = 32.0;
constexpr double kTicksPerSecond = 20.0;
constexpr std::array<registry::BlockFace, 6> kFaces{registry::BlockFace::Down,  registry::BlockFace::Up,
                                                    registry::BlockFace::North, registry::BlockFace::South,
                                                    registry::BlockFace::West,  registry::BlockFace::East};

[[nodiscard]] play::SoundSource sourceOf(const std::string_view category) {
    constexpr std::array<std::pair<std::string_view, play::SoundSource>, 14> kSources{{
        {"master", play::SoundSource::Master},
        {"music", play::SoundSource::Music},
        {"record", play::SoundSource::Records},
        {"records", play::SoundSource::Records},
        {"weather", play::SoundSource::Weather},
        {"block", play::SoundSource::Blocks},
        {"blocks", play::SoundSource::Blocks},
        {"hostile", play::SoundSource::Hostile},
        {"neutral", play::SoundSource::Neutral},
        {"player", play::SoundSource::Players},
        {"players", play::SoundSource::Players},
        {"ambient", play::SoundSource::Ambient},
        {"voice", play::SoundSource::Voice},
        {"ui", play::SoundSource::Ui},
    }};
    for (const auto& [name, source] : kSources) {
        if (name == category) {
            return source;
        }
    }
    return play::SoundSource::Master;
}

[[nodiscard]] double distanceSquared(const player::PlayerSession& player, double x, double y, double z) {
    const double dx = player.x - x;
    const double dy = player.y - y;
    const double dz = player.z - z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

template <typename Packet>
void sendTo(const std::vector<player::PlayerSession*>& players, const Packet& packet) {
    for (const player::PlayerSession* player : players) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(Packet::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

[[nodiscard]] std::optional<play::ParticleOptions> optionsOf(const reg::ParticleType& type,
                                                             const PluginParticle& particle) {
    using Shape = play::ParticleOptionShape;
    const auto number = [&particle](std::size_t at, double fallback = 0.0) {
        return at < particle.data.size() ? particle.data[at] : fallback;
    };
    const auto whole = [&number](std::size_t at, std::int32_t fallback = 0) {
        return static_cast<std::int32_t>(number(at, fallback));
    };
    switch (type.optionShape) {
    case Shape::None:
        return std::nullopt;
    case Shape::BlockState:
        return play::ParticleOptions{play::ParticleOptions::BlockState{whole(0)}};
    case Shape::Color:
        return play::ParticleOptions{play::ParticleOptions::Color{whole(0, -1)}};
    case Shape::Spell:
        return play::ParticleOptions{play::ParticleOptions::Spell{whole(0, -1), static_cast<float>(number(1, 1.0))}};
    case Shape::Dust:
        return play::ParticleOptions{
            play::ParticleOptions::Dust{whole(0, 0xFF0000), static_cast<float>(number(1, 1.0))}};
    case Shape::DustTransition:
        return play::ParticleOptions{play::ParticleOptions::DustTransition{whole(0, 0xFF0000), whole(1, 0xFFFFFF),
                                                                           static_cast<float>(number(2, 1.0))}};
    case Shape::Item: {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(particle.item);
        const reg::Item* item = id.has_value() ? reg::items::registry().byName(*id) : nullptr;
        return play::ParticleOptions{play::ParticleOptions::Item{item == nullptr ? 0 : item->protocolId, 1, {}}};
    }
    case Shape::Trail:
        return play::ParticleOptions{
            play::ParticleOptions::Trail{number(0), number(1), number(2), whole(3, 0xFFFFFF), whole(4, 20)}};
    case Shape::Number:
        return play::ParticleOptions{play::ParticleOptions::Number{static_cast<float>(number(0))}};
    case Shape::Delay:
        return play::ParticleOptions{play::ParticleOptions::Delay{whole(0)}};
    case Shape::Geyser:
        return play::ParticleOptions{play::ParticleOptions::Geyser{whole(0)}};
    case Shape::GeyserBase:
        return play::ParticleOptions{play::ParticleOptions::GeyserBase{whole(0), static_cast<float>(number(1))}};
    case Shape::Vibration:
        return play::ParticleOptions{
            play::ParticleOptions::Vibration{play::BlockPos{whole(0), whole(1), whole(2)}, whole(3, 20)}};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Identifier> parsed(const std::string_view text) {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{text});
    return id.has_value() ? std::optional<Identifier>{*id} : std::nullopt;
}

} // namespace

std::vector<player::PlayerSession*> ServerWorldEnv::watchers() const {
    std::vector<player::PlayerSession*> found;
    if (!players_) {
        return found;
    }
    for (player::PlayerSession* player : players_()) {
        if (player->worldName == world_->name()) {
            found.push_back(player);
        }
    }
    return found;
}

void ServerWorldEnv::sound(const std::string_view sound, const std::string_view category, const double x,
                           const double y, const double z, const float volume, const float pitch,
                           const std::int64_t seed) {
    const double range = kSoundBaseRange * static_cast<double>(std::max(1.0F, volume));
    for (const player::PlayerSession* player : watchers()) {
        if (distanceSquared(*player, x, y, z) <= range * range) {
            player::playSoundFor(*player, sound, sourceOf(category), x, y, z, volume, pitch, seed);
        }
    }
}

void ServerWorldEnv::particle(const PluginParticle& particle) {
    const std::optional<Identifier> id = parsed(particle.type);
    const reg::ParticleType* type = id.has_value() ? reg::particleTypes::of(*id) : nullptr;
    if (type == nullptr) {
        return;
    }
    play::LevelParticlesPacket packet{.particleTypeId = type->protocolId,
                                      .x = particle.x,
                                      .y = particle.y,
                                      .z = particle.z,
                                      .offsetX = particle.offsetX,
                                      .offsetY = particle.offsetY,
                                      .offsetZ = particle.offsetZ,
                                      .count = particle.count,
                                      .overrideLimiter = particle.force,
                                      .options = optionsOf(*type, particle)};
    packet.maxSpeedX = particle.speed[0];
    packet.maxSpeedY = particle.speed[1];
    packet.maxSpeedZ = particle.speed[2];
    packet.randomization =
        static_cast<play::LevelParticlesPacket::Randomization>(std::clamp(particle.randomization, 0, 2));
    const double range = particle.force ? 512.0 : kParticleRange;
    for (const player::PlayerSession* player : watchers()) {
        if (!particle.receiver.empty() && player->uuid().toString() != particle.receiver) {
            continue;
        }
        if (distanceSquared(*player, particle.x, particle.y, particle.z) <= range * range) {
            player::showParticleFor(*player, type->id.toString(), packet);
        }
    }
}

void ServerWorldEnv::levelEvent(const std::int32_t type, const std::int32_t x, const std::int32_t y,
                                const std::int32_t z, const std::int32_t data, const bool global) {
    for (const player::PlayerSession* player : watchers()) {
        if (!player->connection().sendLevelEvent) {
            continue;
        }
        if (global || distanceSquared(*player, x + 0.5, y + 0.5, z + 0.5) <= kLevelEventRange * kLevelEventRange) {
            player->connection().sendLevelEvent(type, x, y, z, data);
        }
    }
}

void ServerWorldEnv::blockEvent(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                const std::int32_t first, const std::int32_t second) {
    const std::optional<std::int32_t> state = world_->state().blockState(x, y, z);
    if (!state.has_value()) {
        return;
    }
    if (first == 1 && world::bells::ringAt(world_->state(), x, y, z)) {
        VillagerWorldSystem::alertVillagers(*world_, x, y, z);
    }
    for (const player::PlayerSession* player : watchers()) {
        if (player->connection().sendBlockEvent &&
            distanceSquared(*player, x + 0.5, y + 0.5, z + 0.5) <= kLevelEventRange * kLevelEventRange) {
            player->connection().sendBlockEvent(x, y, z, *state, first, second);
        }
    }
}

bool ServerWorldEnv::sculk(const std::string_view action, const std::int32_t x, const std::int32_t y,
                           const std::int32_t z, const std::int32_t sourceX, const std::int32_t sourceY,
                           const std::int32_t sourceZ, const std::int32_t value, const std::string_view player) {
    return hooks_->sculk && hooks_->sculk(*world_, action, {x, y, z}, {sourceX, sourceY, sourceZ}, value, player);
}

std::string ServerWorldEnv::creakingHeart(const std::string_view action, const std::int32_t x, const std::int32_t y,
                                          const std::int32_t z) {
    return hooks_->creakingHeart ? hooks_->creakingHeart(*world_, action, {x, y, z}) : std::string{};
}

PluginBorder ServerWorldEnv::border() {
    const world::WorldBorder& border = world_->border();
    return PluginBorder{.centerX = border.centerX(),
                        .centerZ = border.centerZ(),
                        .size = border.size(),
                        .targetSize = border.targetSize(),
                        .lerpTicks = border.lerpTimeTicks(),
                        .damagePerBlock = border.damagePerBlock,
                        .damageBuffer = border.damageBuffer,
                        .warningBlocks = border.warningBlocks,
                        .warningSeconds = border.warningSeconds};
}

void ServerWorldEnv::setBorder(const PluginBorder& wanted, const double seconds) {
    world::WorldBorder& border = world_->border();
    const std::vector<player::PlayerSession*> players = watchers();
    const double centerX =
        std::clamp(wanted.centerX, -world::WorldBorder::kMaxCenterCoordinate, world::WorldBorder::kMaxCenterCoordinate);
    const double centerZ =
        std::clamp(wanted.centerZ, -world::WorldBorder::kMaxCenterCoordinate, world::WorldBorder::kMaxCenterCoordinate);
    if (centerX != border.centerX() || centerZ != border.centerZ()) {
        border.center(centerX, centerZ);
        sendTo(players, play::SetBorderCenterPacket{.centerX = centerX, .centerZ = centerZ});
    }
    const double size = std::clamp(wanted.size, 1.0, world::WorldBorder::kMaxSize);
    if (size != border.targetSize() || (seconds <= 0.0 && border.lerpTimeTicks() > 0)) {
        const double old = border.size();
        if (seconds > 0.0) {
            const auto ticks = static_cast<std::int64_t>(seconds * kTicksPerSecond);
            border.moveSize(size, ticks);
            sendTo(players, play::SetBorderLerpSizePacket{.oldSize = old, .newSize = size, .lerpTimeTicks = ticks});
        } else {
            border.setSize(size);
            sendTo(players, play::SetBorderSizePacket{.size = size});
        }
    }
    border.damagePerBlock = std::max(wanted.damagePerBlock, 0.0);
    border.damageBuffer = std::max(wanted.damageBuffer, 0.0);
    if (wanted.warningBlocks != border.warningBlocks) {
        border.warningBlocks = std::max(wanted.warningBlocks, 0);
        sendTo(players, play::SetBorderWarningDistancePacket{.warningBlocks = border.warningBlocks});
    }
    if (wanted.warningSeconds != border.warningSeconds) {
        border.warningSeconds = std::max(wanted.warningSeconds, 0);
        sendTo(players, play::SetBorderWarningDelayPacket{.warningSeconds = border.warningSeconds});
    }
}

std::int32_t ServerWorldEnv::playerCount() { return static_cast<std::int32_t>(watchers().size()); }

std::int32_t ServerWorldEnv::blockEntityCount() {
    std::int32_t total = 0;
    for (const server::world::Chunk* chunk : world_->state().chunks()) {
        total += static_cast<std::int32_t>(chunk->blockEntities().size());
    }
    return total;
}

std::optional<std::vector<PluginDrop>>
ServerWorldEnv::blockDrops(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                           const std::string_view tool,
                           const std::vector<std::pair<std::string, std::int32_t>>& enchantments, const bool byEntity) {
    world::RealLootTables* tables = hooks_->lootTables;
    const std::optional<std::int32_t> stateId = world_->state().blockState(x, y, z);
    if (tables == nullptr || !stateId.has_value() || *stateId <= 0) {
        return std::nullopt;
    }
    const Identifier* block = reg::blockStateDefinitions::blockOf(*stateId);
    const server::world::loot::LootTable* table = block == nullptr ? nullptr : tables->forBlock(*block);
    if (table == nullptr) {
        return std::vector<PluginDrop>{};
    }
    server::world::loot::LootContext context{
        .blockState = reg::blockStateDefinitions::propertiesOf(*stateId), .block = *block, .hasEntity = byEntity};
    const std::optional<Identifier> toolId = parsed(tool);
    context.tool = toolId.value_or(Identifier::vanilla("air"));
    for (const auto& [enchantment, level] : enchantments) {
        context.toolEnchantments.emplace(enchantment, level);
    }
    const world::realBlockTags::TagMembers* blockTags = hooks_->blockTags;
    const world::realBlockTags::TagMembers* itemTags = hooks_->itemTags;
    context.tags = [blockTags, itemTags](std::string_view registry, const Identifier& id, std::string_view tag) {
        const world::realBlockTags::TagMembers* source = registry == "block" ? blockTags : itemTags;
        return source != nullptr && (registry == "block" || registry == "item") &&
               world::realBlockTags::contains(*source, tag, id);
    };
    tables->prepareAt(context, *world_, x + 0.5, y + 0.5, z + 0.5);
    random::RandomSource random = random::RandomSource::legacy(
        std::chrono::steady_clock::now().time_since_epoch().count() ^ (static_cast<std::int64_t>(x) * 3129871) ^
        (static_cast<std::int64_t>(z) * 116129781));
    std::vector<PluginDrop> out;
    for (const server::world::loot::ItemStack& stack : table->generate(random, 0.0F, std::nullopt, context)) {
        out.push_back(PluginDrop{
            .item = stack.item.toString(),
            .count = stack.count,
            .components = stack.components.empty()
                              ? std::string{}
                              : wire::nbt::snbt::stringify(wire::nbt::NbtTag::ofCompound(stack.components))});
    }
    return out;
}

bool ServerWorldEnv::breakBlock(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                const std::string_view tool,
                                const std::vector<std::pair<std::string, std::int32_t>>& enchantments,
                                const bool drops) {
    const std::optional<std::int32_t> old = world_->state().blockState(x, y, z);
    if (!old.has_value() || *old == reg::blocks::air().stateId || !hooks_->environment) {
        return false;
    }
    if (drops && world_->gameRules().get(world::gameRules::kBlockDrops) && hooks_->nextEntityId) {
        const std::optional<std::vector<PluginDrop>> rolled = blockDrops(x, y, z, tool, enchantments, true);
        for (const PluginDrop& drop : rolled.value_or(std::vector<PluginDrop>{})) {
            const std::optional<Identifier> item = parsed(drop.item);
            if (!item.has_value() || reg::items::registry().byName(*item) == nullptr) {
                continue;
            }
            static_cast<void>(world_->state().spawnItem(world::DroppedItem{.entityId = hooks_->nextEntityId(),
                                                                           .uuid = wire::common::Uuid::random(),
                                                                           .item = *item,
                                                                           .count = drop.count,
                                                                           .x = x + 0.5,
                                                                           .y = y + 0.5 - 0.125,
                                                                           .z = z + 0.5,
                                                                           .velocityY = 0.2}));
        }
    }
    std::int32_t left = reg::blocks::air().stateId;
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*old);
    if (const auto logged = properties.find("waterlogged"); logged != properties.end() && logged->second == "true") {
        left = reg::blockStateDefinitions::stateIdOf(Identifier::vanilla("water"), {{"level", "0"}}).value_or(left);
    }
    command::BlockEditor editor{hooks_->environment(), *world_};
    const bool changed = editor.set(x, y, z, left);
    editor.flush();
    return changed;
}

bool ServerWorldEnv::boneMeal(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const std::optional<std::int32_t> existing = world_->state().blockState(x, y, z);
    if (!existing.has_value() || !world::boneMeal::grows(world_->state(), *existing)) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(world_->gameTime() ^ (x * 31 + z));
    std::optional<world::BlockChange> grown = world::boneMeal::apply(world_->state(), x, y, z, random);
    if (!grown.has_value()) {
        return true;
    }
    constexpr std::int32_t kGrowthSparkles = 15;
    grown->effects.emplace_back(
        world::blockEffect::Effect{.type = world::blockEffect::Effect::kPlantGrowth, .data = kGrowthSparkles});
    if (BlockChangeBroadcaster* blocks = hooks_->blocks ? hooks_->blocks() : nullptr; blocks != nullptr) {
        blocks->announceBlock(watchers(), *grown);
    }
    return true;
}

std::int32_t ServerWorldEnv::signalFrom(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                        const std::int32_t face, const bool direct) {
    if (face < 0 || face > 5 || !world_->state().isLoaded(x >> 4, z >> 4)) {
        return 0;
    }
    if (direct) {
        const std::optional<std::int32_t> state = world_->state().blockState(x, y, z);
        return state.has_value() ? world::redstone::directSignalOf(*state, kFaces[static_cast<std::size_t>(face)]) : 0;
    }
    return world::redstone::signalFrom(world_->state(), world_->name(), x, y, z,
                                       kFaces[static_cast<std::size_t>(face)]);
}

double ServerWorldEnv::temperature(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const std::optional<Identifier> biome = world::naturalSpawner::biomeAt(*world_, x, y, z);
    return biome.has_value() ? static_cast<double>(
                                   server::world::terrain::biomeTemperature::temperatureOf(biome->toString(), x, y, z))
                             : 0.8;
}

double ServerWorldEnv::humidity(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const std::optional<Identifier> biome = world::naturalSpawner::biomeAt(*world_, x, y, z);
    return biome.has_value() ? static_cast<double>(biomeDownfall(biome->path())) : 0.4;
}

double ServerWorldEnv::breakSpeed(const std::string_view uuid, const std::int32_t x, const std::int32_t y,
                                  const std::int32_t z) {
    const std::optional<std::int32_t> state = world_->state().blockState(x, y, z);
    const Identifier* block = state.has_value() ? reg::blockStateDefinitions::blockOf(*state) : nullptr;
    if (block == nullptr || !players_) {
        return 0.0;
    }
    for (const player::PlayerSession* player : players_()) {
        if (player->uuid().toString() == uuid) {
            return static_cast<double>(player::destroyProgressPerTickFor(
                *player, *block, reg::blocks::hardnessOf(*block), hooks_->blockTags, false));
        }
    }
    return 0.0;
}

bool ServerWorldEnv::survives(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                              const std::int32_t stateId) {
    return !world_->state().isLoaded(x >> 4, z >> 4) ||
           world::blockSurvival::isSupported(world_->state(), x, y, z, stateId);
}

} // namespace basalt::server::runtime::plugins
