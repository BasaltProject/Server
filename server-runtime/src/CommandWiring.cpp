#include <basalt/server/runtime/CommandWiring.hpp>
#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/VanillaGameData.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/EntitySelector.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/MessageArguments.hpp>
#include <basalt/server/runtime/command/VanillaCommands.hpp>
#include <basalt/server/runtime/command/commands/AccessCommands.hpp>
#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/server/runtime/command/commands/DataCommand.hpp>
#include <basalt/server/runtime/command/commands/DataPackCommand.hpp>
#include <basalt/server/runtime/command/commands/ExecuteCommand.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>
#include <basalt/server/runtime/command/commands/ProfilingCommands.hpp>
#include <basalt/server/runtime/command/commands/ReloadCommand.hpp>
#include <basalt/server/runtime/command/commands/ScoreCommands.hpp>
#include <basalt/server/runtime/command/commands/ScoreboardCommand.hpp>
#include <basalt/server/runtime/command/commands/SessionCommands.hpp>
#include <basalt/server/runtime/command/commands/TickCommand.hpp>
#include <basalt/server/runtime/command/commands/TimingsCommand.hpp>
#include <basalt/server/runtime/config/LiveConfigs.hpp>
#include <basalt/server/runtime/function/FunctionLibrary.hpp>
#include <basalt/server/runtime/timings/TimingsReport.hpp>
#include <basalt/server/runtime/world/CommandPredicates.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace cmd = command::commands;

template <typename Members>
[[nodiscard]] command::CommandEnvironment::TagMembers
tagLookup(const Members* tags,
          const std::set<wire::common::Identifier>& (*membersOf)(const Members&, std::string_view)) {
    if (tags == nullptr) {
        return {};
    }
    return [tags, membersOf](const wire::common::Identifier& tag) -> std::optional<std::set<wire::common::Identifier>> {
        const std::set<wire::common::Identifier>& members = membersOf(*tags, tag.toString());
        return members.empty() ? std::nullopt : std::optional{members};
    };
}

} // namespace

WiredCommands buildCommands(const CommandWiring& wiring) {
    command::CommandEnvironment::Options options;
    options.pluginEvents = wiring.pluginEvents;
    if (wiring.live != nullptr) {
        config::LiveConfigs* live = wiring.live;
        options.groups = [live]() -> const config::GroupsConfig& { return live->current()->groups; };
        options.commands = [live]() -> const config::CommandsConfig& { return live->current()->commands; };
    }
    options.worldSeed = wiring.worldSeed;
    options.defaultWorldName = wiring.defaultWorldName;
    options.maxPlayers = wiring.maxPlayers;
    options.stopServer = wiring.stopServer;
    options.dropBrokenBlock = wiring.dropBrokenBlock;
    options.summonGolem = wiring.summonGolem;
    options.killOutright = wiring.killOutright;
    options.lang = wiring.lang;
    if (wiring.playerStore != nullptr) {
        options.playerRepository = &wiring.playerStore->players();
        options.access = &wiring.playerStore->access();
        options.progressRepository = &wiring.playerStore->progress();
    }
    options.worlds = wiring.worlds;
    options.placeholders = wiring.placeholders;
    options.timings = wiring.timings;
    options.scoreboard = wiring.scoreboard;
    options.teams = wiring.teams;
    options.progress = wiring.progress;
    options.advancementTriggers = wiring.advancementTriggers;
    if (wiring.gameData != nullptr) {
        VanillaGameData* data = wiring.gameData;
        options.timelines = &data->timelines;
        options.advancements = &data->advancements;
        options.blockTagMembers =
            [data](const wire::common::Identifier& tag) -> std::optional<std::set<wire::common::Identifier>> {
            const std::set<wire::common::Identifier>& members =
                world::realBlockTags::membersOf(data->blockTags, tag.toString());
            return members.empty() ? std::nullopt : std::optional{members};
        };
        options.itemTagMembers =
            [data](const wire::common::Identifier& tag) -> std::optional<std::set<wire::common::Identifier>> {
            const std::set<wire::common::Identifier>& members =
                world::realItemTags::membersOf(data->itemTags, tag.toString());
            return members.empty() ? std::nullopt : std::optional{members};
        };
        options.entityTagMembers =
            [data](const wire::common::Identifier& tag) -> std::optional<std::set<wire::common::Identifier>> {
            const std::set<wire::common::Identifier>& members =
                world::realEntityTypeTags::membersOf(data->entityTags, tag);
            return members.empty() ? std::nullopt : std::optional{members};
        };
        options.predicate = [data](const wire::common::JsonValue& predicate, const command::CommandTarget* self,
                                   world::LoadedWorld* world,
                                   const std::array<double, 3>& origin) -> std::optional<bool> {
            std::optional<server::world::loot::EntityFacts> facts;
            if (self != nullptr && self->player() != nullptr) {
                facts = world::triggerFacts::ofPlayer(*self->player());
            } else if (self != nullptr && self->entity() != nullptr) {
                facts = world == nullptr ? world::triggerFacts::ofEntity(*self->entity())
                                         : world::triggerFacts::ofEntity(*world, *self->entity());
            } else if (self != nullptr) {
                facts = server::world::loot::EntityFacts{.type = self->type(), .scoreName = self->uuid()};
            }
            return world::commandPredicates::test(data->lootTables, predicate, world, origin, std::move(facts));
        };
    }

    if (wiring.players == nullptr) {
        return {};
    }
    WiredCommands wired;
    wired.environment = std::make_unique<command::CommandEnvironment>(*wiring.players, std::move(options));
    if (wiring.gameData != nullptr) {
        // parity: `ComponentUtils.updateForEntity`, run as the entity a loot function names.
        const command::CommandEnvironment* environment = wired.environment.get();
        wiring.gameData->lootTables.useTextResolver(
            [environment](const wire::nbt::NbtTag& tag, std::string_view holder) -> std::optional<wire::nbt::NbtTag> {
                const command::ConsoleCommandSource console;
                command::StringReader reader{std::string{holder}};
                const command::Parsed<command::EntitySelector> selector =
                    command::EntitySelector::parse(reader, console, *environment);
                if (!selector.has_value()) {
                    return std::nullopt;
                }
                const std::vector<command::CommandTarget> found = selector->select(console, *environment, false);
                if (found.empty()) {
                    return std::nullopt;
                }
                const command::CommandTarget& self = found.front();
                command::ExecutionSource::Changes changes{};
                changes.entity = self;
                changes.position = std::array<double, 3>{self.x(), self.y(), self.z()};
                changes.player = self.player();
                const command::ExecutionSource source{console, changes};
                const wire::text::Component resolved = command::messageArguments::resolveContents(
                    wire::text::componentNbt::deserialize(tag), source, *environment, &self);
                return wire::text::componentNbt::serialize(resolved);
            });
    }
    if (wiring.commandStorage != nullptr) {
        wired.data = std::make_unique<command::DataAccess>(*wiring.commandStorage);
    }
    wired.dispatcher = std::make_unique<command::CommandDispatcher>(*wired.environment);
    command::CommandDispatcher* dispatcher = wired.dispatcher.get();

    command::VanillaCommandHooks hooks;
    hooks.players = wiring.players;
    hooks.scoreboard = wiring.scoreboard;
    hooks.teams = wiring.teams;
    hooks.progress = wiring.progress;
    hooks.functionRunner = wiring.functionRunner;
    hooks.blockTests = wiring.blockTests;
    if (wiring.gameData != nullptr) {
        hooks.advancements = &wiring.gameData->advancements;
        hooks.recipes = &wiring.gameData->recipes;
        hooks.templates = &wiring.gameData->structureTemplates;
        hooks.enchantments = &wiring.gameData->enchantments;
        hooks.dialogs = &wiring.gameData->dialogs;
        hooks.lootTables = &wiring.gameData->lootTables;
        hooks.testInstances = &wiring.gameData->testInstances;
    }
    const std::int64_t seed = wiring.worldSeed;
    hooks.worldSeed = [seed] { return seed; };
    hooks.randomSequences = wiring.randomSequences;
    hooks.nowMillis = wiring.nowMillis;
    hooks.nowEpochSeconds = wiring.nowEpochSeconds;
    hooks.nextDouble = wiring.nextDouble;
    hooks.nextStructureSeed = wiring.nextSeed;
    hooks.nextSoundSeed = wiring.nextSeed;
    hooks.setIdleTimeout = wiring.setIdleTimeout;
    hooks.whitelistState = wiring.whitelistEnforced;
    hooks.setWhitelistState = wiring.setWhitelistEnforced;
    registerVanillaCommands(*dispatcher, hooks);

    // A missing dependency means the command is simply not registered, which is how a fixture gets the ones it can run.
    const auto add = [&dispatcher](std::shared_ptr<const command::Command> command) {
        static_cast<void>(dispatcher->registerCommand(std::move(command)));
    };
    if (wiring.live != nullptr) {
        add(std::make_shared<cmd::ReloadCommand>(*wiring.live, wiring.onConfigsReloaded, wiring.datapacks));
    }
    if (wiring.requestRestart) {
        add(std::make_shared<cmd::RestartCommand>(wiring.requestRestart));
    }
    if (wiring.protocolGate != nullptr) {
        const wire::multiversion::ProtocolGate* gate = wiring.protocolGate;
        add(std::make_shared<cmd::VersionCommand>([gate] { return gate->supported(); }));
    }
    if (wiring.bossBars != nullptr) {
        add(std::make_shared<cmd::BossBarCommand>(*wiring.bossBars));
    }
    if (wiring.scoreboard != nullptr) {
        add(std::make_shared<cmd::ScoreboardCommand>(*wiring.scoreboard));
        add(std::make_shared<cmd::TriggerCommand>(*wiring.scoreboard));
        cmd::ExecuteCommandHooks execute;
        execute.bossBars = wiring.bossBars;
        execute.functions = wiring.functionRunner;
        execute.players = wiring.players;
        execute.data = wired.data.get();
        add(std::make_shared<cmd::ExecuteCommand>(*wiring.scoreboard, execute));
    }
    if (wiring.timings != nullptr && wiring.timingsReport != nullptr && wiring.nowNanos) {
        wire::common::Timings& timings = *wiring.timings;
        const timings::TimingsReport& report = *wiring.timingsReport;
        const std::function<std::int64_t()> nowNanos = wiring.nowNanos;
        TickLoop* tickLoop = wiring.tickLoop;
        cmd::FunctionRunner* runner = wiring.functionRunner;
        add(std::make_shared<cmd::DebugCommand>(
            timings, report, nowNanos,
            tickLoop == nullptr ? std::function<std::int64_t()>{} : [tickLoop] { return tickLoop->ticksRun(); },
            runner == nullptr ? std::function<std::optional<std::int32_t>(const command::CommandContext&,
                                                                          const wire::common::Identifier&)>{}
                              : [runner](const command::CommandContext& context,
                                         const wire::common::Identifier& id) -> std::optional<std::int32_t> {
                const command::Parsed<std::int32_t> ran = runner->run(context.dispatcher(), context.source(), id);
                return ran.has_value() ? std::optional{*ran} : std::nullopt;
            }));
        add(std::make_shared<cmd::PerfCommand>(timings, report, nowNanos));
        add(std::make_shared<cmd::JfrCommand>(timings));
        if (wiring.tickDurations != nullptr) {
            add(std::make_shared<cmd::TimingsCommand>(timings, *wiring.tickDurations, report, nowNanos));
            if (tickLoop != nullptr) {
                add(std::make_shared<cmd::TickCommand>(*tickLoop, *wiring.tickDurations, nowNanos));
            }
        }
    }
    const std::int32_t port = wiring.port;
    add(std::make_shared<cmd::PublishCommand>([port] { return port; }));
    add(std::make_shared<cmd::UnpublishCommand>());
    if (wiring.stopwatches != nullptr) {
        add(std::make_shared<cmd::StopwatchCommand>(*wiring.stopwatches));
    }
    if (wiring.fetchMojangUuid) {
        add(std::make_shared<cmd::FetchProfileCommand>(wiring.fetchMojangUuid));
    }
    if (wiring.functions != nullptr && wiring.functionRunner != nullptr) {
        add(std::make_shared<cmd::FunctionCommand>(*wiring.functionRunner, wired.data.get()));
        add(std::make_shared<cmd::ReturnCommand>(*wiring.functionRunner));
        if (wiring.functionSchedule != nullptr) {
            add(std::make_shared<cmd::ScheduleCommand>(*wiring.functions, *wiring.functionSchedule,
                                                       [](const command::CommandContext& context) -> std::int64_t {
                                                           const world::LoadedWorld* world = command::worldIn(
                                                               context.source(), context.environment());
                                                           return world == nullptr ? 0 : world->gameTime();
                                                       }));
        }
    }
    if (wiring.datapacks.packs != nullptr) {
        add(std::make_shared<cmd::DataPackCommand>(wiring.datapacks));
    }
    if (wiring.commandStorage != nullptr) {
        add(std::make_shared<cmd::DataCommand>(*wiring.commandStorage));
    }
    if (wiring.whitelistEnforced && wiring.setWhitelistEnforced && wiring.nowEpochSeconds) {
        add(std::make_shared<cmd::WhitelistCommand>(wiring.whitelistEnforced, wiring.setWhitelistEnforced,
                                                    wiring.nowEpochSeconds));
    }
    if (wiring.saveControl != nullptr) {
        add(std::make_shared<cmd::SaveAllCommand>(*wiring.saveControl));
        add(std::make_shared<cmd::SaveOffCommand>(*wiring.saveControl));
        add(std::make_shared<cmd::SaveOnCommand>(*wiring.saveControl));
    }
    if (wiring.setIdleTimeout) {
        add(std::make_shared<cmd::SetIdleTimeoutCommand>(wiring.setIdleTimeout));
    }
    return wired;
}

} // namespace basalt::server::runtime
