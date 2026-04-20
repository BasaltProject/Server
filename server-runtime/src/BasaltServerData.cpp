#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/datapack/RegistryValidation.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/registry/PackRegistries.hpp>
#include <basalt/server/runtime/registry/SyncedTags.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <exception>
#include <random>

namespace basalt::server::runtime {
namespace {

[[nodiscard]] std::int64_t freshSeed() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    return static_cast<std::int64_t>(random());
}

/// parity: brigadier's `CommandSyntaxException.getMessage`.
[[nodiscard]] std::string messageOf(const command::CommandSyntaxError& error, const wire::text::LangTable& lang) {
    std::string message = wire::text::resolvePlainText(error.component, lang);
    if (error.cursor < 0 || !error.input.has_value()) {
        return message;
    }
    const std::string& input = *error.input;
    const auto cursor = std::min(static_cast<std::size_t>(error.cursor), input.size());
    constexpr std::size_t kContext = 10;
    message += " at position " + std::to_string(cursor) + ": ";
    if (cursor > kContext) {
        message += "...";
    }
    message += input.substr(cursor > kContext ? cursor - kContext : 0, std::min(cursor, kContext));
    return message + "<--[HERE]";
}

} // namespace

std::string BasaltServer::schedulePath() const {
    return worldRegistry_->defaultWorld().worldRoot() + "/scheduled_events.nbt";
}

void BasaltServer::saveSchedule() const {
    if (!wire::io::fileIO::writeBytes(
            schedulePath(), wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(functionSchedule_.toNbt())))) {
        logger_.warn([] { return std::string{"Could not save the scheduled functions."}; });
    }
}

VanillaGameData::Jar BasaltServer::dataJar() const {
    if (options_.datapacks != nullptr) {
        return VanillaGameData::jarOf(options_.datapacks->resources());
    }
    return VanillaGameData::jarOf(options_.serverJar);
}

std::unique_ptr<command::commands::FunctionRunner> BasaltServer::makeFunctionRunner(std::int32_t permissionLevel) {
    const std::optional<std::vector<std::uint8_t>> saved = wire::io::fileIO::readBytes(schedulePath());
    if (saved.has_value()) {
        const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*saved);
        if (const wire::nbt::NbtCompound* root = read.has_value() ? read->tag.get<wire::nbt::NbtCompound>() : nullptr;
            root != nullptr) {
            functionSchedule_.readNbt(*root);
        }
    }
    return std::make_unique<command::commands::FunctionRunner>(
        functions_,
        command::commands::FunctionRunner::Settings{
            .permissionLevel = permissionLevel, .commandLimit = [this](const command::CommandSource& source) {
                const world::LoadedWorld* world =
                    wired_.environment == nullptr ? nullptr : command::worldIn(source, *wired_.environment);
                return (world == nullptr ? worldRegistry_->defaultWorld() : *world)
                    .gameRules()
                    .number(world::gameRules::kMaxCommandSequenceLength);
            }});
}

void BasaltServer::loadFunctions() {
    const VanillaGameData::Jar jar = dataJar();
    // parity: `Commands.createCompilationContext`, the server's source at the function level.
    const command::ExecutionSource compilation{
        *consoleSource_,
        command::ExecutionSource::Changes{.permission = functionRunner_->permissionLevel(), .silent = true}};
    const function::CommandCheck check = [this, &compilation](const std::string& line) -> std::optional<std::string> {
        if (wired_.dispatcher == nullptr) {
            return std::nullopt;
        }
        const std::optional<command::CommandSyntaxError> error = wired_.dispatcher->check(compilation, line);
        return error.has_value() ? std::optional{messageOf(*error, options_.lang)} : std::nullopt;
    };
    static_cast<void>(functions_.load(jar.entriesUnder, jar.readText, check));
    functionRunner_->loaded();
}

bool BasaltServer::reloadData(const std::vector<std::string>& selected) {
    try {
        if (options_.datapacks != nullptr) {
            std::shared_ptr<const datapack::ResourceStack> resources = options_.datapacks->open(selected);
            // parity: one element that does not load fails the reload, and nothing changes.
            if (const std::vector<datapack::ElementError> errors = datapack::invalidElements(
                    *resources, datapack::builtInPackIds(options_.datapacks->repository()), false);
                !errors.empty()) {
                datapack::logElementErrors(errors, logger_);
                return false;
            }
            vanillaData_.reload(VanillaGameData::jarOf(resources), logger_);
            options_.datapacks->commit(selected, std::move(resources));
        } else {
            vanillaData_.reload(dataJar(), logger_);
        }
        advancementTriggers_->reindex();
        loadFunctions();
        // parity: `PlayerAdvancements.reload`, everyone starting over with a reset.
        playerAdvancements_->reload();
        for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
            advancementTriggers_->grantAutomatic(player->name(), player->uuid().toString());
        }
        resyncAfterReload();
        return true;
    } catch (const std::exception& error) {
        logger_.warn([&error] { return std::string{"Failed to execute reload: "} + error.what(); });
        return false;
    }
}

void BasaltServer::tickFunctions() {
    if (wired_.dispatcher == nullptr || consoleSource_ == nullptr) {
        return;
    }
    // parity: `getGameLoopSender`, the server's source as a game master, heard by nobody.
    const command::ExecutionSource gameLoop{
        *consoleSource_,
        command::ExecutionSource::Changes{.permission = command::permissionLevel::kGamemasters, .silent = true}};
    try {
        functionRunner_->tick(*wired_.dispatcher, gameLoop);
        for (const command::commands::FunctionSchedule::Event& event :
             functionSchedule_.due(worldRegistry_->defaultWorld().gameTime())) {
            if (event.isTag) {
                functionRunner_->runTag(*wired_.dispatcher, gameLoop, event.id);
            } else if (const function::ServerFunction* function = functions_.get(event.id); function != nullptr) {
                static_cast<void>(functionRunner_->call(*wired_.dispatcher, gameLoop, *function));
            }
        }
    } catch (const std::exception& error) {
        logger_.error([&error] { return std::string{"A function failed: "} + error.what(); });
    }
}

void BasaltServer::resyncAfterReload() {
    // parity: `PlayerList.reloadResources`.
    const wire::protocol::configuration::TagsPacket tags{
        .groups = registry::syncedTags(registry::TagSources{.blocks = &vanillaData_.blockTags,
                                                            .items = &vanillaData_.itemTags,
                                                            .fluids = &vanillaData_.fluidTags,
                                                            .entityTypes = &vanillaData_.entityTags,
                                                            .timelines = &vanillaData_.timelineTags,
                                                            .timelineOrder = &vanillaData_.timelines})};
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(registry::kPlayTagsId,
                                                [&tags](wire::io::ByteBuffer& buffer) { tags.write(buffer); });
        }
    }
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        const std::set<wire::common::Identifier> known = playerProgress_->knownRecipes(player->uuid().toString());
        world::recipeBook::resend(*player, vanillaData_.recipes, {known.begin(), known.end()});
    }
}

void BasaltServer::addAdvancementHooks(world::AdvancementTriggers::Hooks& hooks) {
    playerAdvancements_ = std::make_unique<world::PlayerAdvancements>(vanillaData_.advancements, *playerProgress_,
                                                                      &vanillaData_.enchantments);
    playerProgress_->onChanged([this](std::string_view uuid, const wire::common::Identifier& advancement) {
        playerAdvancements_->changed(uuid, advancement);
    });
    hooks.awardLoot = [this](std::string_view uuid, const std::vector<wire::common::Identifier>& tables) {
        awardAdvancementLoot(uuid, tables);
    };
    hooks.runFunction = [this](std::string_view uuid, const wire::common::Identifier& id) {
        const std::optional<wire::common::Uuid> parsed = wire::common::Uuid::parse(uuid);
        player::PlayerSession* player = parsed.has_value() ? playerRegistry_.get(*parsed) : nullptr;
        const function::ServerFunction* function = functions_.get(id);
        if (player == nullptr || function == nullptr || wired_.dispatcher == nullptr) {
            return;
        }
        // parity: the player's own source, silent, as a game master.
        const command::PlayerCommandSource source{*player, live_->current()->groups};
        const command::ExecutionSource running{
            source,
            command::ExecutionSource::Changes{.permission = command::permissionLevel::kGamemasters, .silent = true}};
        static_cast<void>(functionRunner_->call(*wired_.dispatcher, running, *function));
    };
    hooks.flush = [this](std::string_view uuid, bool showAdvancements) {
        const std::optional<wire::common::Uuid> parsed = wire::common::Uuid::parse(uuid);
        const player::PlayerSession* player = parsed.has_value() ? playerRegistry_.get(*parsed) : nullptr;
        if (player == nullptr) {
            return;
        }
        if (const auto packet = playerAdvancements_->flush(uuid, showAdvancements); packet.has_value()) {
            sendAdvancementPacket(*player, wire::protocol::play::UpdateAdvancementsPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet->write(buffer); });
        }
    };
}

void BasaltServer::awardAdvancementLoot(std::string_view uuid, const std::vector<wire::common::Identifier>& tables) {
    const std::optional<wire::common::Uuid> parsed = wire::common::Uuid::parse(uuid);
    player::PlayerSession* player = parsed.has_value() ? playerRegistry_.get(*parsed) : nullptr;
    if (player == nullptr) {
        return;
    }
    const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
    const auto found = worlds.find(player->worldName);
    if (found == worlds.end()) {
        return;
    }
    world::LoadedWorld& world = *found->second;
    // parity: `ADVANCEMENT_REWARD`: the player as this entity, where they stand.
    const server::world::loot::LootContext context =
        world::triggerFacts::ofPlayerAt(vanillaData_.lootTables, world, *player, &playerRegistry_);
    random::RandomSource random = random::RandomSource::legacy(freshSeed());
    bool changes = false;
    for (const wire::common::Identifier& id : tables) {
        const server::world::loot::LootTable* table = vanillaData_.lootTables.byId(id);
        if (table == nullptr) {
            continue;
        }
        for (const server::world::loot::ItemStack& stack :
             table->generate(random, context.luck, std::nullopt, context)) {
            const std::int32_t left = player->inventory.add(storage::StoredItemStack{
                .id = stack.item.toString(), .count = stack.count, .components = stack.components});
            if (left == 0) {
                const float pitch = (((random.nextFloat() - random.nextFloat()) * 0.7F) + 1.0F) * 2.0F;
                for (const player::PlayerSession* heard : playerRegistry_.snapshot()) {
                    if (heard->worldName == player->worldName) {
                        player::playSoundFor(*heard, "entity.item.pickup", wire::protocol::play::SoundSource::Players,
                                             player->x, player->y, player->z, 0.2F, pitch, freshSeed());
                    }
                }
                changes = true;
                continue;
            }
            changes |= left < stack.count;
            // parity: `Player.drop(stack, false)` with no pickup delay, straight at their feet.
            world::DroppedItem dropped{.entityId = playerRegistry_.allocateEntityId(),
                                       .uuid = wire::common::Uuid::random(),
                                       .item = stack.item,
                                       .count = left,
                                       .x = player->x,
                                       .y = player->y + world::fluids::eyeHeight(player->sneaking) - 0.3,
                                       .z = player->z};
            dropped.components = stack.components;
            dropped.pickupDelay = 0;
            static_cast<void>(world.state().spawnItem(std::move(dropped)));
        }
    }
    if (changes && player->connection().sendInventoryAll) {
        player->connection().sendInventoryAll();
    }
}

void BasaltServer::sendAdvancementPacket(const player::PlayerSession& player, std::int32_t id,
                                         const std::function<void(wire::io::ByteBuffer&)>& write) const {
    if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(id, write);
    }
}

void BasaltServer::flushAdvancements() {
    if (playerAdvancements_ == nullptr) {
        return;
    }
    for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
        const auto packet = playerAdvancements_->flush(player->uuid().toString(), true);
        if (packet.has_value()) {
            sendAdvancementPacket(*player, wire::protocol::play::UpdateAdvancementsPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet->write(buffer); });
        }
    }
}

} // namespace basalt::server::runtime
