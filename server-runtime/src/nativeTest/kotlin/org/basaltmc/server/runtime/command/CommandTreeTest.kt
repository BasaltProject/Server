package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.TickLoop
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
import org.basaltmc.server.runtime.command.commands.TransferCommand
import org.basaltmc.server.runtime.command.commands.TriggerCommand
import org.basaltmc.server.runtime.command.commands.UnpublishCommand
import org.basaltmc.server.runtime.command.commands.WaypointCommand
import org.basaltmc.server.runtime.command.commands.WhitelistCommand
import org.basaltmc.server.runtime.command.commands.WorldBorderCommand
import org.basaltmc.server.runtime.config.AnimationsConfig
import org.basaltmc.server.runtime.config.CommandsConfig
import org.basaltmc.server.runtime.config.ConfigSnapshot
import org.basaltmc.server.runtime.config.LiveConfigs
import org.basaltmc.server.runtime.config.testServerList
import org.basaltmc.server.runtime.function.FunctionLibrary
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.server.runtime.timings.ReportSources
import org.basaltmc.server.runtime.timings.TimingsReport
import org.basaltmc.server.runtime.ui.UiConfigs
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.runtime.world.RealAdvancements
import org.basaltmc.server.runtime.world.RealDialogs
import org.basaltmc.server.runtime.world.RealEnchantments
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.RealRecipes
import org.basaltmc.server.runtime.world.RealTestInstances
import org.basaltmc.server.runtime.world.StructureTemplates
import org.basaltmc.server.world.terrain.VillageStructureGenerator
import org.basaltmc.wire.common.timings.TickDurations
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private class DumpSource : CommandSource {
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val position: Triple<Double, Double, Double> = Triple(0.0, 64.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) = Unit
}

/** A report nobody reads: the profiling commands need one to build their tree. */
private fun dumpReport(): TimingsReport =
    TimingsReport(
        Timings(),
        TickDurations(),
        ReportSources(
            versionName = "26.2",
            protocolVersion = 776,
            uptimeSeconds = { 0L },
            onlinePlayers = { 0 },
            maxPlayers = { 0 },
            loadedChunks = { 0 },
            regionCacheBytes = { 0L },
            chunkBytes = { 0L },
            chunkBlockEntities = { 0 },
            chunkScheduledTicks = { 0 },
            chunkGeneratedEntities = { 0 },
            worldgenCaches = { emptyList() },
            worldCount = { 0 },
            cpuPercent = { null },
            liveHeapBytes = { null },
            residentBytes = { null },
        ),
    )

/**
 * Builds the whole tree, checks that every completion the server can answer is one the client will
 * actually ask for, and writes the tree out as JSON.
 *
 * The dump is for the parity job: vanilla's own `commands.json` report is generated from the
 * operator's jar and cannot live in the repo, so comparing the two argument by argument is done by
 * hand against `server-datagen`'s output.
 */
class CommandTreeTest {
    /**
     * The datapack registries the client never gets a copy of, so it cannot resolve an entry of
     * one. parity: `RegistryDataLoader.SYNCHRONIZED_REGISTRIES` is what it does get, and none of
     * these are in it.
     */
    private val unsyncedRegistries =
        setOf(
            "minecraft:loot_table",
            "minecraft:item_modifier",
            "minecraft:predicate",
            "minecraft:function",
            "minecraft:advancement",
            "minecraft:recipe",
        )

    /** Every command this server registers, which is what the client is actually sent. */
    private fun fullDispatcher(): CommandDispatcher {
        val library = FunctionLibrary(root = "build/test-data/dump/datapacks")
        val runner = FunctionRunner(library)
        val dispatcher =
            CommandDispatcher(testEnvironment())
                .also(::registerVanillaCommands)
                .also { it.register(RestartCommand({})) }
                .also { it.register(BossBarCommand(BossBarRegistry())) }
                .also { it.register(ScoreboardCommand(Scoreboard())) }
                .also { it.register(TriggerCommand(Scoreboard())) }
                .also { it.register(ExecuteCommand(Scoreboard(), BossBarRegistry())) }
                .also {
                    it.register(
                        JfrCommand(
                            org.basaltmc.wire.common.timings
                                .Timings(),
                        ),
                    )
                }.also { it.register(PublishCommand { 25565 }) }
                .also { it.register(FetchProfileCommand { null }) }
                .also {
                    it.register(
                        FunctionCommand(
                            library,
                            runner,
                            DataAccess(CommandStorage("build/test-data/dump/storage.dat")),
                        ),
                    )
                }.also { it.register(ReturnCommand(runner)) }
                .also { it.register(ScheduleCommand(library, FunctionSchedule())) }
                .also { it.register(DataPackCommand(library)) }
                .also { it.register(SummonCommand(PlayerRegistry())) }
                .also { it.register(DamageCommand()) }
                .also { it.register(RideCommand()) }
                .also { it.register(RotateCommand()) }
                .also { it.register(SwingCommand()) }
                .also { it.register(AttributeCommand()) }
                .also { it.register(ParticleCommand()) }
                .also { it.register(AdvancementCommand(RealAdvancements(null), PlayerProgress())) }
                .also { it.register(RecipeCommand(PlayerProgress(), RealRecipes(null))) }
                .also { it.register(LocateCommand()) }
                .also { it.register(WaypointCommand()) }
                .also { it.register(PlaceCommand(StructureTemplates(null))) }
                .also { it.register(EnchantCommand(RealEnchantments(null))) }
                .also { it.register(DialogCommand(RealDialogs(null))) }
                .also { it.register(TestCommand(RealTestInstances(null), runner)) }
                .also { it.register(ItemCommand()) }
                .also { it.register(LootCommand(RealLootTables(null)) { 0L }) }
                .also { it.register(FillBiomeCommand()) }
                .also { it.register(DataCommand(CommandStorage("build/test-data/dump/storage.dat"))) }
                .also { it.register(SpawnPointCommand()) }
                .also { it.register(RandomCommand { 0L }) }
                .also { it.register(TransferCommand()) }
                .also { it.register(BanCommand()) }
                .also { it.register(BanIpCommand()) }
                .also { it.register(BanListCommand()) }
                .also { it.register(PardonCommand(ip = false)) }
                .also { it.register(PardonCommand(ip = true)) }
                .also { it.register(WhitelistCommand(enforced = { false }, setEnforced = {})) }
                .also { it.register(WorldBorderCommand()) }
                .also { it.register(ForceLoadCommand()) }
                .also { it.register(SpreadPlayersCommand()) }
                .also { it.register(SpectateCommand()) }
                .also { it.register(TeamCommand(Teams())) }
                .also { it.register(TeamMsgCommand(Teams())) }
                .also { it.register(SaveAllCommand(SaveControl {})) }
                .also { it.register(SaveOffCommand(SaveControl {})) }
                .also { it.register(SaveOnCommand(SaveControl {})) }
                .also { it.register(SetIdleTimeoutCommand {}) }
                // The four the real server registers with runtime objects. They belong in the dump
                // for the same reason as the rest: what is not here cannot be compared to vanilla.
                .also { it.register(TickCommand(TickLoop(), TickDurations()) { 0L }) }
                .also { it.register(PerfCommand(Timings(), dumpReport()) { 0L }) }
                .also { it.register(DebugCommand(Timings(), dumpReport(), { 0L }, { 0L }, runner)) }
                .also { it.register(UnpublishCommand()) }
                .also { it.register(StopwatchCommand(Stopwatches { 0L })) }
                .also { it.register(EffectCommand()) }
                .also {
                    it.register(
                        ReloadCommand(
                            LiveConfigs(
                                ConfigSnapshot(
                                    groups = testGroups(),
                                    serverList = testServerList(),
                                    commands = CommandsConfig(emptyMap(), emptyList(), emptyMap()),
                                    animations = AnimationsConfig(emptyMap()),
                                    ui = UiConfigs(),
                                ),
                            ),
                            {},
                        ),
                    )
                }
        return dispatcher
    }

    @Test
    fun buildsTheWholeTreeAndKeepsEveryProviderReachable() {
        val dispatcher = fullDispatcher()
        val packet = dispatcher.treeFor(DumpSource())
        val json = StringBuilder()
        json.append("[\n")
        packet.nodes.forEachIndexed { index, node ->
            json.append(
                "  {\"i\":$index,\"kind\":\"${node.kind}\",\"name\":${quote(node.name)}," +
                    "\"parser\":${quote(node.parser?.let { describe(it) })},\"parserId\":${node.parser?.registryId}," +
                    "\"suggestions\":${quote(node.suggestionId)}," +
                    "\"executable\":${node.executable},\"redirect\":${node.redirect}," +
                    "\"children\":${node.children.joinToString(",", "[", "]")}}",
            )
            if (index != packet.nodes.lastIndex) json.append(',')
            json.append('\n')
        }
        json.append("]\n")
        FileIO.createDirectories("build/test-data/dump")
        FileIO.writeText("build/test-data/dump/tree.json", json.toString())
        assertTrue(packet.nodes.size > 100)

        // The bug this guards: an argument that knows its values but is declared with a parser the
        // client cannot complete, and without a provider name either. The client then never asks,
        // so the player is shown the syntax and nothing else.
        val unreachable =
            dispatcher.unreachableSuggestions().map { "${it.first}: ${it.second}" }
        assertTrue(unreachable.isEmpty(), "arguments whose completions nobody will ask for: $unreachable")
    }

    /**
     * The client crash of 2026-08-16: `/item modify ... <modifier>` was declared as a
     * `minecraft:resource` over `minecraft:item_modifier`, and `/loot` the same over
     * `minecraft:loot_table`. Both are datapack registries, which the client never receives, so
     * `ResourceArgument` threw `Registry minecraft:item_modifier not found` while building the tree
     * and the connection died with "network protocol error" before anyone could play.
     *
     * `minecraft:resource` and `minecraft:resource_or_tag` are the two parsers that look their
     * registry up; `resource_key` and `resource_or_tag_key` only build a key and are safe.
     */
    @Test
    fun noArgumentAsksTheClientForARegistryItDoesNotHave() {
        val dispatcher = fullDispatcher()
        val offenders = mutableListOf<String>()
        for (node in dispatcher.treeFor(DumpSource()).nodes) {
            val parser = node.parser
            val registry =
                when (parser) {
                    is ArgumentParser.Resource -> parser.registry
                    is ArgumentParser.ResourceOrTag -> parser.registry
                    else -> null
                } ?: continue
            if (registry in unsyncedRegistries) offenders += "${node.name}: $registry"
        }
        assertTrue(offenders.isEmpty(), "arguments the client cannot resolve: $offenders")
    }

    /** The line from the bug report, answered end to end by the same tree the client is sent. */
    @Test
    fun answersTheStructuresOfPlaceStructure() {
        val dispatcher =
            CommandDispatcher(
                testEnvironment(),
            ).also { it.register(PlaceCommand(StructureTemplates(null))) }
        val source = DumpSource()

        val all = dispatcher.suggest(source, "/place structure ")
        assertEquals(VillageStructureGenerator.STRUCTURE_IDS.size, all.suggestions.size)

        // Typed halfway, and with no namespace: the same match rule vanilla uses.
        val typed = dispatcher.suggest(source, "/place structure plains")
        assertEquals(listOf("minecraft:village_plains"), typed.suggestions.map { it.text })
        assertEquals("/place structure ".length, typed.start)
    }

    private fun quote(value: String?): String = if (value == null) "null" else "\"$value\""

    private fun describe(parser: org.basaltmc.wire.protocol.play.ArgumentParser): String =
        parser.toString().replace('"', '\'')
}
