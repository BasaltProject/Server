package org.basaltmc.server.runtime

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.command.CommandEnvironment
import org.basaltmc.server.runtime.command.commands.AdvancementCommand
import org.basaltmc.server.runtime.command.commands.AttributeCommand
import org.basaltmc.server.runtime.command.commands.BanCommand
import org.basaltmc.server.runtime.command.commands.BanIpCommand
import org.basaltmc.server.runtime.command.commands.BanListCommand
import org.basaltmc.server.runtime.command.commands.BossBarCommand
import org.basaltmc.server.runtime.command.commands.BossBarRegistry
import org.basaltmc.server.runtime.command.commands.CommandStorage
import org.basaltmc.server.runtime.command.commands.DamageCommand
import org.basaltmc.server.runtime.command.commands.DataAccess
import org.basaltmc.server.runtime.command.commands.DataCommand
import org.basaltmc.server.runtime.command.commands.DataPackCommand
import org.basaltmc.server.runtime.command.commands.DebugCommand
import org.basaltmc.server.runtime.command.commands.DialogCommand
import org.basaltmc.server.runtime.command.commands.EffectCommand
import org.basaltmc.server.runtime.command.commands.EnchantCommand
import org.basaltmc.server.runtime.command.commands.ExecuteCommand
import org.basaltmc.server.runtime.command.commands.FetchProfileCommand
import org.basaltmc.server.runtime.command.commands.FillBiomeCommand
import org.basaltmc.server.runtime.command.commands.ForceLoadCommand
import org.basaltmc.server.runtime.command.commands.FunctionCommand
import org.basaltmc.server.runtime.command.commands.FunctionRunner
import org.basaltmc.server.runtime.command.commands.FunctionSchedule
import org.basaltmc.server.runtime.command.commands.ItemCommand
import org.basaltmc.server.runtime.command.commands.JfrCommand
import org.basaltmc.server.runtime.command.commands.LocateCommand
import org.basaltmc.server.runtime.command.commands.LootCommand
import org.basaltmc.server.runtime.command.commands.PardonCommand
import org.basaltmc.server.runtime.command.commands.ParticleCommand
import org.basaltmc.server.runtime.command.commands.PerfCommand
import org.basaltmc.server.runtime.command.commands.PlaceCommand
import org.basaltmc.server.runtime.command.commands.PublishCommand
import org.basaltmc.server.runtime.command.commands.RandomCommand
import org.basaltmc.server.runtime.command.commands.RecipeCommand
import org.basaltmc.server.runtime.command.commands.ReloadCommand
import org.basaltmc.server.runtime.command.commands.RestartCommand
import org.basaltmc.server.runtime.command.commands.ReturnCommand
import org.basaltmc.server.runtime.command.commands.RideCommand
import org.basaltmc.server.runtime.command.commands.RotateCommand
import org.basaltmc.server.runtime.command.commands.SaveAllCommand
import org.basaltmc.server.runtime.command.commands.SaveControl
import org.basaltmc.server.runtime.command.commands.SaveOffCommand
import org.basaltmc.server.runtime.command.commands.SaveOnCommand
import org.basaltmc.server.runtime.command.commands.ScheduleCommand
import org.basaltmc.server.runtime.command.commands.ScoreboardCommand
import org.basaltmc.server.runtime.command.commands.SetIdleTimeoutCommand
import org.basaltmc.server.runtime.command.commands.SpawnPointCommand
import org.basaltmc.server.runtime.command.commands.SpectateCommand
import org.basaltmc.server.runtime.command.commands.SpreadPlayersCommand
import org.basaltmc.server.runtime.command.commands.StopwatchCommand
import org.basaltmc.server.runtime.command.commands.Stopwatches
import org.basaltmc.server.runtime.command.commands.SummonCommand
import org.basaltmc.server.runtime.command.commands.SwingCommand
import org.basaltmc.server.runtime.command.commands.TeamCommand
import org.basaltmc.server.runtime.command.commands.TeamMsgCommand
import org.basaltmc.server.runtime.command.commands.TestCommand
import org.basaltmc.server.runtime.command.commands.TickCommand
import org.basaltmc.server.runtime.command.commands.TimingsCommand
import org.basaltmc.server.runtime.command.commands.TransferCommand
import org.basaltmc.server.runtime.command.commands.TriggerCommand
import org.basaltmc.server.runtime.command.commands.UnpublishCommand
import org.basaltmc.server.runtime.command.commands.VersionCommand
import org.basaltmc.server.runtime.command.commands.WaypointCommand
import org.basaltmc.server.runtime.command.commands.WhitelistCommand
import org.basaltmc.server.runtime.command.commands.WorldBorderCommand
import org.basaltmc.server.runtime.command.registerVanillaCommands
import org.basaltmc.server.runtime.config.BasaltConfig
import org.basaltmc.server.runtime.config.LiveConfigs
import org.basaltmc.server.runtime.function.FunctionLibrary
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.server.runtime.timings.TimingsReport
import org.basaltmc.server.runtime.world.AdvancementTriggers
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.wire.common.placeholder.PlaceholderRegistry
import org.basaltmc.wire.common.timings.TickDurations
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.multiversion.ProtocolGate
import org.basaltmc.wire.text.LangTable
import kotlin.time.TimeMark

/**
 * Builds and registers every vanilla command, plus the ones tied to server-wide state.
 *
 * Pure construction-time wiring: nothing here runs per tick. Extracted out of [BasaltServer]'s
 * constructor so that class stays about what a server *is*, not about assembling the command tree.
 */
internal fun buildCommands(
    playerRegistry: PlayerRegistry,
    pluginEventsProvider: () -> PluginEventBus?,
    live: LiveConfigs,
    worldSeed: Long,
    config: BasaltConfig,
    stopServer: () -> Unit,
    lang: LangTable,
    defaultWorldName: String,
    playerStore: PlayerStore,
    worlds: Map<String, LoadedWorld>,
    placeholders: PlaceholderRegistry,
    timings: Timings,
    vanillaData: VanillaGameData,
    scoreboard: Scoreboard,
    teams: Teams,
    playerProgress: PlayerProgress,
    advancementTriggers: AdvancementTriggers,
    onConfigsReloaded: () -> Unit,
    functions: FunctionLibrary,
    requestRestart: () -> Unit,
    protocolGate: ProtocolGate,
    bossBars: BossBarRegistry,
    commandStorage: CommandStorage,
    functionRunner: FunctionRunner,
    timingsReport: TimingsReport,
    serverClock: TimeMark,
    tickLoop: TickLoop,
    stopwatches: Stopwatches,
    fetchMojangUuid: (String) -> String?,
    functionSchedule: FunctionSchedule,
    serverJar: ServerJarAssets?,
    whitelistEnforced: () -> Boolean,
    setWhitelistEnforced: (Boolean) -> Unit,
    tickDurations: TickDurations,
    saveControl: SaveControl,
    setIdleTimeoutMinutes: (Int) -> Unit,
): CommandDispatcher =
    CommandDispatcher(
        CommandEnvironment(
            players = playerRegistry,
            pluginEventsProvider = pluginEventsProvider,
            groupsProvider = { live.current.groups },
            worldSeed = worldSeed,
            maxPlayers = config.maxPlayers,
            stopServer = stopServer,
            lang = lang,
            defaultWorldName = defaultWorldName,
            playerRepository = playerStore.players,
            access = playerStore.access,
            worlds = worlds,
            placeholders = placeholders,
            commandsProvider = { live.current.commands },
            timings = timings,
            blockTagMembers = { tag -> vanillaData.blockTags.membersOf(tag.toString()).ifEmpty { null } },
            itemTagMembers = { tag -> vanillaData.itemTags.membersOf(tag.toString()).ifEmpty { null } },
            entityTagMembers = { tag -> vanillaData.entityTags.membersOf(tag).ifEmpty { null } },
            scoreboard = scoreboard,
            timelines = vanillaData.timelines,
            teams = teams,
            advancements = vanillaData.advancements,
            progress = playerProgress,
            advancementTriggers = advancementTriggers,
        ),
    ).also(::registerVanillaCommands)
        .also { it.register(ReloadCommand(live, onConfigsReloaded, functions::reload)) }
        .also { it.register(RestartCommand(requestRestart)) }
        .also { it.register(VersionCommand { protocolGate.supported }) }
        .also { it.register(BossBarCommand(bossBars)) }
        .also { it.register(ScoreboardCommand(scoreboard)) }
        .also { it.register(TriggerCommand(scoreboard)) }
        .also {
            it.register(
                ExecuteCommand(scoreboard, bossBars, DataAccess(commandStorage), functionRunner, playerRegistry),
            )
        }.also {
            it.register(
                DebugCommand(
                    timings,
                    timingsReport,
                    nowNanos = { serverClock.elapsedNow().inWholeNanoseconds },
                    ticksRun = { tickLoop.ticksRun },
                    functions = functionRunner,
                ),
            )
        }.also { it.register(PerfCommand(timings, timingsReport) { serverClock.elapsedNow().inWholeNanoseconds }) }
        .also { it.register(JfrCommand(timings)) }
        .also { it.register(PublishCommand { config.port }) }
        .also { it.register(UnpublishCommand()) }
        .also { it.register(StopwatchCommand(stopwatches)) }
        .also { it.register(EffectCommand()) }
        .also { it.register(FetchProfileCommand(fetchMojangUuid)) }
        .also { it.register(FunctionCommand(functions, functionRunner, DataAccess(commandStorage))) }
        .also { it.register(ReturnCommand(functionRunner)) }
        .also { it.register(ScheduleCommand(functions, functionSchedule)) }
        .also { it.register(DataPackCommand(functions)) }
        .also { it.register(SummonCommand(playerRegistry)) }
        .also { it.register(DamageCommand(serverJar)) }
        .also { it.register(RideCommand()) }
        .also { it.register(RotateCommand()) }
        .also { it.register(SwingCommand()) }
        .also { it.register(AttributeCommand()) }
        .also { it.register(ParticleCommand()) }
        .also { it.register(AdvancementCommand(vanillaData.advancements, playerProgress)) }
        .also { it.register(RecipeCommand(playerProgress, vanillaData.recipes)) }
        .also { it.register(LocateCommand()) }
        .also { it.register(WaypointCommand(serverJar)) }
        .also { it.register(PlaceCommand(vanillaData.structureTemplates)) }
        .also { it.register(EnchantCommand(vanillaData.enchantments)) }
        .also { it.register(DialogCommand(vanillaData.dialogs)) }
        .also { it.register(TestCommand(vanillaData.testInstances, functionRunner)) }
        .also { it.register(ItemCommand()) }
        .also { it.register(LootCommand(vanillaData.lootTables) { worldSeed }) }
        .also { it.register(FillBiomeCommand()) }
        .also { it.register(DataCommand(commandStorage)) }
        .also { it.register(SpawnPointCommand()) }
        .also { it.register(RandomCommand { worldSeed }) }
        .also { it.register(TransferCommand()) }
        .also { it.register(BanCommand()) }
        .also { it.register(BanIpCommand()) }
        .also { it.register(BanListCommand()) }
        .also { it.register(PardonCommand(ip = false)) }
        .also { it.register(PardonCommand(ip = true)) }
        .also {
            it.register(
                WhitelistCommand(
                    enforced = whitelistEnforced,
                    setEnforced = setWhitelistEnforced,
                ),
            )
        }.also { it.register(WorldBorderCommand()) }
        .also { it.register(ForceLoadCommand()) }
        .also { it.register(SpreadPlayersCommand()) }
        .also { it.register(SpectateCommand()) }
        .also { it.register(TickCommand(tickLoop, tickDurations) { serverClock.elapsedNow().inWholeNanoseconds }) }
        .also { it.register(TeamCommand(teams)) }
        .also { it.register(TeamMsgCommand(teams)) }
        .also { it.register(SaveAllCommand(saveControl)) }
        .also { it.register(SaveOffCommand(saveControl)) }
        .also { it.register(SaveOnCommand(saveControl)) }
        .also { it.register(SetIdleTimeoutCommand(setIdleTimeoutMinutes)) }
        .also {
            it.register(
                TimingsCommand(
                    timings = timings,
                    tickDurations = tickDurations,
                    report = timingsReport,
                    nowNanos = { serverClock.elapsedNow().inWholeNanoseconds },
                ),
            )
        }
