#include <basalt/server/runtime/ShriekerSystem.hpp>
#include <basalt/server/runtime/WorldRegistry.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/ContainerLoot.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <cstdint>
#include <regex>
#include <utility>

namespace basalt::server::runtime {
namespace {

/// What a world may be called: it becomes a folder name and a row in every player's file.
[[nodiscard]] bool isUsableName(const std::string& name) {
    static const std::regex shape{"[a-zA-Z0-9_-]{1,64}"};
    return std::regex_match(name, shape);
}

[[nodiscard]] bool isNormal(config::WorldType type) {
    return type == config::WorldType::Normal || type == config::WorldType::Amplified ||
           type == config::WorldType::LargeBiomes || type == config::WorldType::Flat;
}

} // namespace

WorldRegistry::WorldRegistry(const config::BasaltConfig& config, const config::WorldsConfig& worlds,
                             world::VanillaWorldGeneratorFactory* generators, player::PlayerRegistry& players,
                             Hooks hooks, Options options, wire::common::Logger logger)
    : worldsConfig_(&worlds), generators_(generators), players_(&players), hooks_(std::move(hooks)),
      options_(std::move(options)), logger_(std::move(logger)), defaultName_(worlds.defaultWorld().name),
      scheduler_(config.regional, config.regionThreads) {
    // Overworlds first, and that order is the point: a Nether or an End that declares no seed of its
    // own takes the seed of the overworld it belongs to, which is what makes one set of dimensions
    // one world. parity: a vanilla save has a single seed for its three dimensions.
    std::vector<const config::WorldDeclaration*> ordered;
    ordered.reserve(worlds.worlds().size());
    for (const config::WorldDeclaration& declaration : worlds.worlds()) {
        ordered.push_back(&declaration);
    }
    std::ranges::stable_partition(ordered, [](const config::WorldDeclaration* declaration) {
        return declaration->dimension == config::WorldDimension::Overworld;
    });

    for (const config::WorldDeclaration* declaration : ordered) {
        std::optional<std::int64_t> inherited = declaration->seed;
        if (!inherited.has_value()) {
            const config::WorldDeclaration& overworld = worlds.overworldOf(*declaration);
            if (const auto found = metadata_.find(overworld.name); found != metadata_.end()) {
                inherited = found->second.seed;
            }
        }
        metadata_.insert_or_assign(declaration->name, world::WorldMetadata::loadOrCreate(
                                                          options_.worldsRoot + "/" + declaration->name, inherited,
                                                          world::WorldMetadata::kDefaultSpawn, logger_));
    }
    for (const config::WorldDeclaration& declaration : worlds.worlds()) {
        owned_.push_back(buildWorld(declaration, metadata_.at(declaration.name)));
    }
}

WorldRegistry::~WorldRegistry() = default;

std::unique_ptr<world::LoadedWorld> WorldRegistry::buildWorld(const config::WorldDeclaration& declaration,
                                                              world::WorldMetadata metadata) {
    world::LoadedWorldOptions options;
    options.timings = options_.timings;
    options.scheduler = &scheduler_;
    // The default world's own Nether and End: the one its portals lead to.
    const config::WorldDeclaration* linked = worldsConfig_->portalTarget(declaration, declaration.dimension);
    options.vanillaKey =
        worldsConfig_->overworldOf(declaration).isDefault && linked != nullptr && linked->name == declaration.name;
    options.generationWorkers = options_.generationMetrics ? 1 : world::ChunkGenerationQueue::defaultWorkers();
    if (generators_ != nullptr && isNormal(declaration.type)) {
        world::VanillaWorldGeneratorFactory::Generators built = generators_->create(declaration, metadata);
        // The one generation itself uses, so `/place structure` and `/locate structure` act on the
        // same villages the world already has.
        options.villageGenerator = built.villages;
        options.campGenerator = built.camps;
        options.structureLocators = built.locators;
        options.structurePlacers = built.placers;
        options.strongholdGenerator = built.strongholds;
        options.spawnChunkFinder = built.spawnChunk;
        options.pipeline = built.pipeline;
        options.spawnMobs = built.spawnMobs;
        built_.insert_or_assign(declaration.name, std::move(built));
    }
    std::unique_ptr<world::LoadedWorld> world =
        std::make_unique<world::LoadedWorld>(declaration, worldsConfig_->settingsFor(declaration), std::move(metadata),
                                             options_.worldsRoot, logger_, std::move(options));
    // parity: `SculkShriekerBlockEntity.tryGetPlayer`, which only the runtime can answer.
    world->state().setPlayerCheck([players = players_, holder = world.get()](const wire::common::Uuid& entity) {
        return ShriekerSystem::playerBehind(*holder, *players, entity) != nullptr;
    });
    if (options_.lootTables != nullptr) {
        world->state().setLootUnpacker([tables = options_.lootTables, triggers = options_.triggers, players = players_,
                                        holder = world.get()](std::int32_t x, std::int32_t y, std::int32_t z,
                                                              const std::optional<wire::common::Uuid>& player) {
            const player::PlayerSession* session =
                player.has_value() && players != nullptr ? players->get(*player) : nullptr;
            const std::optional<wire::common::Identifier> rolled = world::containerLoot::unpack(
                *holder, *tables, x, y, z, player,
                session == nullptr
                    ? std::nullopt
                    : std::optional{world::containerLoot::Opener{
                          .name = session->name(),
                          .luck = session->luck(),
                          .mainhand = session->inventory.heldItem().has_value()
                                          ? wire::common::Identifier::parse(session->inventory.heldItem()->id)
                                                .value_or(wire::common::Identifier::vanilla("air"))
                                          : std::optional<wire::common::Identifier>{}}});
            // parity: `unpackLootTable` fires `GENERATE_LOOT` for the player who opened it.
            if (rolled.has_value() && session != nullptr && triggers != nullptr) {
                triggers->onContainerLoot(session->name(), player->toString(), *rolled);
            }
        });
    }
    if (options_.jukeboxes != nullptr) {
        world->state().setSongOutput([songs = options_.jukeboxes](const std::string& song) { return songs->of(song); });
    }
    return world;
}

std::map<std::string, world::LoadedWorld*> WorldRegistry::worlds() const {
    std::map<std::string, world::LoadedWorld*> all;
    for (const std::unique_ptr<world::LoadedWorld>& world : owned_) {
        all.insert_or_assign(world->name(), world.get());
    }
    return all;
}

world::LoadedWorld& WorldRegistry::defaultWorld() const {
    for (const std::unique_ptr<world::LoadedWorld>& world : owned_) {
        if (world->name() == defaultName_) {
            return *world;
        }
    }
    return *owned_.front();
}

std::int64_t WorldRegistry::worldSeed() const { return defaultWorld().metadata().seed; }

world::LoadedWorld* WorldRegistry::createWorld(const std::string& name, config::WorldDimension dimension,
                                               config::WorldType type, std::optional<std::int64_t> seed) {
    if (!isUsableName(name)) {
        return nullptr;
    }
    for (const std::unique_ptr<world::LoadedWorld>& world : owned_) {
        if (world->name() == name) {
            return nullptr;
        }
    }
    const config::WorldDeclaration declaration{
        .name = name, .dimension = dimension, .type = type, .seed = seed, .isDefault = false};
    wire::io::fileIO::createDirectories(options_.worldsRoot + "/" + name);
    world::WorldMetadata metadata = world::WorldMetadata::loadOrCreate(
        options_.worldsRoot + "/" + name, seed.has_value() ? seed : std::optional{worldSeed()},
        world::WorldMetadata::kDefaultSpawn, logger_);
    const std::int64_t madeWith = metadata.seed;
    owned_.push_back(buildWorld(declaration, std::move(metadata)));
    world::LoadedWorld* made = owned_.back().get();
    logger_.info([&name, dimension, type, madeWith] {
        return "Made world '" + name + "' (" + std::string{config::dimensionName(dimension)} + ", " +
               std::string{config::worldTypeName(type)} + ", seed " + std::to_string(madeWith) + ").";
    });
    made->saveDirty();
    // Next turn, not now: this is usually called from inside a plugin's own handler, and firing an
    // event there would re-enter the Lua state that is already running. One turn later it is an
    // ordinary event like any other.
    if (hooks_.worldLoaded) {
        scheduler_.global().schedule(0, [this, name] { hooks_.worldLoaded(name); });
    }
    return made;
}

void WorldRegistry::onConfigsReloaded() {
    if (hooks_.rebuildCommands) {
        hooks_.rebuildCommands();
    }
    for (const player::PlayerSession* player : players_->snapshot()) {
        if (player->connection().sendCommandTree) {
            player->connection().sendCommandTree();
        }
    }
}

} // namespace basalt::server::runtime
