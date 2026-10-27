#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/ShriekerSystem.hpp>
#include <basalt/server/runtime/WorldRegistry.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/ContainerLoot.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <regex>
#include <utility>

namespace basalt::server::runtime {
namespace {

[[nodiscard]] bool isUsableName(const std::string& name) {
    static const std::regex shape{"[a-zA-Z0-9_-]{1,64}"};
    return std::regex_match(name, shape);
}

[[nodiscard]] bool isNormal(config::WorldType type) {
    return type == config::WorldType::Normal || type == config::WorldType::Amplified ||
           type == config::WorldType::LargeBiomes || type == config::WorldType::Flat;
}

// Current dynamic dimensions live under the primary save. Existing flat saves
// and configured dimensions retain their locations; never migrate data implicitly.
[[nodiscard]] std::string storageRoot(const config::WorldsConfig& config, const std::string& root,
                                      const std::string& primary, const std::string& name) {
    const bool configured = std::ranges::any_of(config.worlds(), [&name](const auto& world) { return world.name == name; });
    if (configured || std::filesystem::exists(std::filesystem::path{root} / name)) return root;
    return (std::filesystem::path{root} / primary / "dimensions" / "minecraft").string();
}

} // namespace

WorldRegistry::WorldRegistry(const config::BasaltConfig& config, const config::WorldsConfig& worlds,
                             world::VanillaWorldGeneratorFactory* generators, player::PlayerRegistry& players,
                             Hooks hooks, Options options, wire::common::Logger logger)
    : worldsConfig_(&worlds), generators_(generators), players_(&players), hooks_(std::move(hooks)),
      options_(std::move(options)), logger_(std::move(logger)), defaultName_(worlds.defaultWorld().name),
      scheduler_(config.regional, config.regionThreads) {
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
        const config::WorldSettings settings = worlds.settingsFor(*declaration);
        metadata_.insert_or_assign(declaration->name,
                                   world::WorldMetadata::loadOrCreate(options_.worldsRoot + "/" + declaration->name,
                                                                      inherited, world::WorldMetadata::kDefaultSpawn,
                                                                      logger_, &settings.rules.gameRules));
    }
    for (const config::WorldDeclaration& declaration : worlds.worlds()) {
        owned_.push_back(buildWorld(declaration, metadata_.at(declaration.name)));
    }
}

WorldRegistry::~WorldRegistry() {
    // Join world workers while their scheduler and generator dependencies are
    // still alive, including worlds retired with retained plugin views.
    retiredWorlds_.clear();
    owned_.clear();
}

std::unique_ptr<world::LoadedWorld> WorldRegistry::buildWorld(const config::WorldDeclaration& declaration,
                                                              world::WorldMetadata metadata) {
    world::LoadedWorldOptions options;
    options.timings = options_.timings;
    options.scheduler = &scheduler_;
    const config::WorldDeclaration* linked = worldsConfig_->portalTarget(declaration, declaration.dimension);
    options.vanillaKey =
        worldsConfig_->overworldOf(declaration).isDefault && linked != nullptr && linked->name == declaration.name;
    options.generationWorkers = options_.generationMetrics ? 1 : world::ChunkGenerationQueue::defaultWorkers();
    config::WorldSettings settings = worldsConfig_->settingsFor(declaration);
    if (generators_ != nullptr && isNormal(declaration.type)) {
        world::VanillaWorldGeneratorFactory::Generators built =
            generators_->create(declaration, metadata, settings.rules.generateStructures);
        // The same generator, so `/place structure` and `/locate structure` act on the villages the world has.
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
    std::unique_ptr<world::LoadedWorld> world = std::make_unique<world::LoadedWorld>(
        declaration, std::move(settings), std::move(metadata),
        storageRoot(*worldsConfig_, options_.worldsRoot, defaultName_, declaration.name), logger_, std::move(options));
    world->setEvents(options_.events);
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
        if (!unloaded_.contains(world->name())) {
            all.insert_or_assign(world->name(), world.get());
        }
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
    for (auto it = owned_.begin(); it != owned_.end(); ++it) {
        if ((*it)->name() != name) continue;
        if (!unloaded_.erase(name)) return nullptr;
        retiredWorlds_.push_back(std::move(*it));
        owned_.erase(it);
        if (auto built = built_.find(name); built != built_.end()) {
            retiredGenerators_.push_back(std::move(built->second));
            built_.erase(built);
        }
        break;
    }
    const config::WorldDeclaration declaration{
        .name = name, .dimension = dimension, .type = type, .seed = seed, .isDefault = false};
    const std::string root = storageRoot(*worldsConfig_, options_.worldsRoot, defaultName_, name);
    wire::io::fileIO::createDirectories(root + "/" + name);
    const config::WorldSettings settings = worldsConfig_->settingsFor(declaration);
    world::WorldMetadata metadata = world::WorldMetadata::loadOrCreate(
        root + "/" + name, seed.has_value() ? seed : std::optional{worldSeed()},
        world::WorldMetadata::kDefaultSpawn, logger_, &settings.rules.gameRules);
    const std::int64_t madeWith = metadata.seed;
    owned_.push_back(buildWorld(declaration, std::move(metadata)));
    world::LoadedWorld* made = owned_.back().get();
    logger_.info([&name, dimension, type, madeWith] {
        return "Made world '" + name + "' (" + std::string{config::dimensionName(dimension)} + ", " +
               std::string{config::worldTypeName(type)} + ", seed " + std::to_string(madeWith) + ").";
    });
    made->saveDirty();
    // Next turn, not now: called from inside a plugin handler, firing here would re-enter the running Lua state.
    if (hooks_.worldLoaded) {
        scheduler_.global().schedule(0, [this, name] { hooks_.worldLoaded(name); });
    }
    return made;
}

bool WorldRegistry::unloadWorld(const std::string& name, const bool save) {
    if (name == defaultName_ || unloaded_.contains(name)) {
        return false;
    }
    for (const std::unique_ptr<world::LoadedWorld>& world : owned_) {
        if (world->name() != name) {
            continue;
        }
        for (const player::PlayerSession* player : players_->snapshot()) {
            if (player->worldName == name) {
                return false;
            }
        }
        if (options_.events != nullptr &&
            !options_.events->allowed(&server::plugins::pluginEventTypes::worldUnload,
                                      [&](server::plugins::PluginEvent& event) { event.set("world", name); })) {
            return false;
        }
        world->stopGenerating(5000, save);
        if (save) {
            world->saveDirty();
            world->flushRegions();
            world->saveMetadata();
        }
        world->retire();
        lastChunkSetVersion_.erase(name);
        unloaded_.insert(name);
        logger_.info([&name] { return "Unloaded world '" + name + "'."; });
        return true;
    }
    return false;
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
