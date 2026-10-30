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
      scheduler_(std::make_shared<sched::RegionScheduler>(config.regional, config.regionThreads)) {
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
    owned_.clear();
}

std::shared_ptr<world::LoadedWorld> WorldRegistry::buildWorld(const config::WorldDeclaration& declaration,
                                                              world::WorldMetadata metadata) {
    world::LoadedWorldOptions options;
    options.timings = options_.timings;
    options.scheduler = scheduler_.get();
    const config::WorldDeclaration* linked = worldsConfig_->portalTarget(declaration, declaration.dimension);
    options.vanillaKey =
        worldsConfig_->overworldOf(declaration).isDefault && linked != nullptr && linked->name == declaration.name;
    options.generationWorkers = options_.generationMetrics ? 1 : world::ChunkGenerationQueue::defaultWorkers();
    config::WorldSettings settings = worldsConfig_->settingsFor(declaration);
    std::shared_ptr<world::VanillaWorldGeneratorFactory::Generators> generation;
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
        generation = std::make_shared<world::VanillaWorldGeneratorFactory::Generators>(std::move(built));
    }
    auto* raw = new world::LoadedWorld(declaration, std::move(settings), std::move(metadata),
                                       storageRoot(*worldsConfig_, options_.worldsRoot, defaultName_, declaration.name),
                                       logger_, std::move(options));
    std::shared_ptr<world::LoadedWorld> world{raw, [scheduler = scheduler_, generation](auto* held) mutable {
                                                  delete held;
                                                  generation.reset();
                                                  scheduler.reset();
                                              }};
    storagePaths_.insert_or_assign(
        declaration.name, std::filesystem::weakly_canonical(std::filesystem::absolute(raw->worldRoot())).string());
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
    const std::lock_guard lock{worldsMutex_};
    std::map<std::string, world::LoadedWorld*> all;
    for (const std::shared_ptr<world::LoadedWorld>& world : owned_) {
        if (!unloaded_.contains(world->name())) {
            all.insert_or_assign(world->name(), world.get());
        }
    }
    return all;
}

std::map<std::string, std::shared_ptr<world::LoadedWorld>> WorldRegistry::worldsForLogin() const {
    const std::lock_guard lock{worldsMutex_};
    std::map<std::string, std::shared_ptr<world::LoadedWorld>> snapshot;
    for (const auto& world : owned_) {
        if (unloading_.contains(world->name()) || unloaded_.contains(world->name()))
            continue;
        auto& count = loginPins_[world->name()];
        if (!count)
            count = std::make_shared<std::atomic<std::size_t>>(0);
        ++*count;
        snapshot.emplace(world->name(),
                         std::shared_ptr<world::LoadedWorld>{world.get(), [held = world, count](auto*) mutable {
                                                                 --*count;
                                                                 held.reset();
                                                             }});
    }
    return snapshot;
}

world::LoadedWorld& WorldRegistry::defaultWorld() const {
    const std::lock_guard lock{worldsMutex_};
    for (const std::shared_ptr<world::LoadedWorld>& world : owned_) {
        if (world->name() == defaultName_) {
            return *world;
        }
    }
    return *owned_.front();
}

std::int64_t WorldRegistry::worldSeed() const { return defaultWorld().metadata().seed; }

world::LoadedWorld* WorldRegistry::createWorld(const std::string& name, config::WorldDimension dimension,
                                               config::WorldType type, std::optional<std::int64_t> seed) {
    const std::lock_guard lock{worldsMutex_};
    if (!isUsableName(name) || unloading_.contains(name)) {
        return nullptr;
    }
    for (const auto& held : owned_)
        if (held->name() == name)
            return nullptr;
    unloaded_.erase(name);
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
        scheduler_->global().schedule(0, [this, name] { hooks_.worldLoaded(name); });
    }
    if (hooks_.rebuildCommands)
        hooks_.rebuildCommands();
    return made;
}

bool WorldRegistry::unloadWorld(const std::string& name, const bool save) {
    const std::lock_guard lock{worldsMutex_};
    if (name == defaultName_ || unloaded_.contains(name) || unloading_.contains(name))
        return false;
    const auto found =
        std::find_if(owned_.begin(), owned_.end(), [&name](const auto& world) { return world->name() == name; });
    if (found == owned_.end())
        return false;
    auto world = *found;
    if (const auto pins = loginPins_.find(name); pins != loginPins_.end() && pins->second->load() != 0)
        return false;
    unloading_.insert(name);

    struct UnloadingGuard {
        std::set<std::string>& names;
        const std::string& name;

        ~UnloadingGuard() { names.erase(name); }
    } guard{unloading_, name};

    for (const auto* player : players_->snapshot())
        if (player->worldName == name)
            return false;
    if (options_.events != nullptr &&
        !options_.events->allowed(&server::plugins::pluginEventTypes::worldUnload,
                                  [&](server::plugins::PluginEvent& event) { event.set("world", name); }))
        return false;
    world->stopGenerating(5000, save);
    if (save) {
        world->saveDirty();
        world->flushRegions();
        world->saveMetadata();
    }
    // Detach before cancellation callbacks can reenter world APIs.
    std::erase(owned_, world);
    unloaded_.insert(name);
    metadata_.erase(name);
    lastChunkSetVersion_.erase(name);
    world->retire();
    for (auto* region : scheduler_->regionsOf(name))
        region->owner().cancelPending();
    if (hooks_.rebuildCommands)
        hooks_.rebuildCommands();
    logger_.info([&name] { return "Unloaded world '" + name + "'."; });
    return true;
}

bool WorldRegistry::deleteWorld(const std::string& name) {
    const std::lock_guard lock{worldsMutex_};
    namespace fs = std::filesystem;
    if (!isUsableName(name) || name == defaultName_ || !unloaded_.contains(name) || unloading_.contains(name))
        return false;
    const auto record = storagePaths_.find(name);
    if (record == storagePaths_.end())
        return false;
    for (const auto& held : owned_)
        if (held->name() == name)
            return false;
    const auto inside = [](const fs::path& root, const fs::path& target) {
        auto r = root.begin(), t = target.begin();
        for (; r != root.end(); ++r, ++t)
            if (t == target.end() || *r != *t)
                return false;
        return true;
    };
    std::error_code error;
    const fs::path root = fs::weakly_canonical(fs::absolute(options_.worldsRoot), error);
    if (error)
        return false;
    const fs::path declared = fs::path{storageRoot(*worldsConfig_, options_.worldsRoot, defaultName_, name)} / name;
    if (fs::is_symlink(fs::symlink_status(declared, error)))
        return false;
    if (error && error != std::errc::no_such_file_or_directory)
        return false;
    error.clear();
    const fs::path target = fs::weakly_canonical(fs::absolute(declared), error);
    if (error || target != fs::path{record->second} || target == root || !inside(root, target))
        return false;
    for (const auto& held : owned_) {
        const fs::path active = fs::weakly_canonical(fs::absolute(held->worldRoot()), error);
        if (error || inside(target, active))
            return false;
        // Filesystem identity also catches case-folded aliases on macOS/Windows.
        std::error_code identityError;
        if (fs::equivalent(target, active, identityError))
            return false;
        if (identityError && identityError != std::errc::no_such_file_or_directory)
            return false;
    }
    fs::remove_all(target, error); // Does not follow symlinks inside the world.
    if (error)
        return false;
    storagePaths_.erase(record);
    loginPins_.erase(name);
    unloaded_.erase(name);
    metadata_.erase(name);
    lastChunkSetVersion_.erase(name);
    logger_.info([&name] { return "Deleted unloaded world '" + name + "'."; });
    return true;
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
