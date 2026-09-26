#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/registry/PackRegistries.hpp>
#include <basalt/server/runtime/registry/SyncedTags.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <exception>

namespace basalt::server::runtime {
namespace {

/// parity: brigadier's `CommandSyntaxException.getMessage`, the message and where it stopped.
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
            vanillaData_.reload(VanillaGameData::jarOf(resources), logger_);
            options_.datapacks->commit(selected, std::move(resources));
        } else {
            vanillaData_.reload(dataJar(), logger_);
        }
        advancementTriggers_->reindex();
        loadFunctions();
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

std::function<void(std::vector<wire::protocol::configuration::RegistryDataPacket>&)>
BasaltServer::packRegistries() const {
    datapack::DataPacks* packs = options_.datapacks;
    if (packs == nullptr) {
        return {};
    }
    return [packs](std::vector<wire::protocol::configuration::RegistryDataPacket>& packets) {
        std::vector<std::string> builtIn;
        for (const datapack::Pack* pack : packs->repository().availablePacks()) {
            if (pack->source != datapack::PackSource::World) {
                builtIn.push_back(pack->id);
            }
        }
        registry::applyPackRegistries(packets, *packs->resources(), builtIn);
    };
}

void BasaltServer::resyncAfterReload() {
    // parity: `PlayerList.reloadResources`: the tags to everyone, then each player's recipe book.
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

} // namespace basalt::server::runtime
