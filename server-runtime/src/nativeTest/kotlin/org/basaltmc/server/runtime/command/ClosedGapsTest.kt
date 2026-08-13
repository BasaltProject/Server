package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.AttributeCommand
import org.basaltmc.server.runtime.command.commands.CommandStorage
import org.basaltmc.server.runtime.command.commands.DataAccess
import org.basaltmc.server.runtime.command.commands.DataCommand
import org.basaltmc.server.runtime.command.commands.ExecuteCommand
import org.basaltmc.server.runtime.command.commands.SummonCommand
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldBorder
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private class GapConsole : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 64.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

/** The gaps that were documented and are now closed: `if blocks`, storage on disk, modifiers, border damage. */
class ClosedGapsTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/closed-gaps",
            logger = Logger("ClosedGapsTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    private val scoreboard = Scoreboard()
    private val storagePath = "build/test-data/closed-gaps/command_storage.nbt"
    private val storage = CommandStorage(storagePath)
    private val players = PlayerRegistry()

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = players,
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
                worlds = mapOf("world" to world),
            ),
        ).also(::registerVanillaCommands)
            .also { it.register(DataCommand(storage)) }
            .also { it.register(SummonCommand(players)) }
            .also { it.register(AttributeCommand()) }
            .also {
                it.register(
                    org.basaltmc.server.runtime.command.commands
                        .ScoreboardCommand(scoreboard),
                )
            }.also { it.register(ExecuteCommand(scoreboard, null, DataAccess(storage))) }

    private val source = GapConsole()

    init {
        for (chunkX in -1..1) for (chunkZ in -1..1) world.state.getOrGenerateChunk(chunkX, chunkZ)
        platform.posix.remove(storagePath)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun ifBlocksComparesTwoRegionsAndCountsTheMatches() {
        run("fill 0 70 0 1 70 1 stone")
        run("clone 0 70 0 1 70 1 8 70 8")
        assertEquals(4, run("execute if blocks 0 70 0 1 70 1 8 70 8 all"))

        run("setblock 8 70 8 dirt")
        assertEquals(0, run("execute if blocks 0 70 0 1 70 1 8 70 8 all"))
    }

    @Test
    fun ifBlocksMaskedIgnoresAirInTheSource() {
        run("fill 0 75 0 1 75 1 stone")
        run("setblock 0 75 0 air")
        run("clone 0 75 0 1 75 1 12 75 12")
        run("setblock 12 75 12 bedrock")
        // The source's air is skipped, so the bedrock underneath it does not fail the comparison.
        assertEquals(3, run("execute if blocks 0 75 0 1 75 1 12 75 12 masked"))
    }

    @Test
    fun executeStoresIntoCommandStorage() {
        run("fill 0 80 0 2 80 0 stone")
        run("""execute store result storage basalt:out filled int 1 run fill 0 80 0 2 80 0 dirt""")
        assertEquals(3, (storage[Identifier.parse("basalt:out")]["filled"] as NbtInt).value)
    }

    @Test
    fun commandStorageSurvivesASaveAndLoad() {
        run("""data modify storage basalt:kept counter set value 9""")
        storage.save()
        val reloaded = CommandStorage(storagePath).also { it.load() }
        assertEquals(9, (reloaded[Identifier.parse("basalt:kept")]["counter"] as NbtInt).value)
    }

    @Test
    fun attributeModifiersApplyInVanillasOrder() {
        run("summon minecraft:cow 0 64 0")
        run("attribute cow minecraft:max_health base set 20")
        run("attribute cow minecraft:max_health modifier add basalt:flat 10 add_value")
        run("attribute cow minecraft:max_health modifier add basalt:half 0.5 add_multiplied_total")
        val cow = world.state.entities().single()
        // (20 + 10) * 1.5
        assertEquals(45.0, cow.attributeWithModifiers("minecraft:max_health"))
    }

    @Test
    fun addingTheSameModifierTwiceIsRefused() {
        run("summon minecraft:cow 0 64 0")
        run("attribute cow minecraft:max_health modifier add basalt:one 1 add_value")
        run("attribute cow minecraft:max_health modifier add basalt:one 1 add_value")
        assertTrue(source.messages.any { "modifier_already_present" in it })
    }

    @Test
    fun theBorderDamagesOnlyPastItsBuffer() {
        val border = WorldBorder()
        border.setSize(100.0)
        border.damageBuffer = 5.0
        border.damagePerBlock = 0.2
        assertEquals(0f, border.damageAt(0.0, 0.0))
        assertEquals(0f, border.damageAt(52.0, 0.0))
        // 60 blocks out is 10 past the border and 5 past the buffer: 5 * 0.2 is below the floor of 1.
        assertEquals(1f, border.damageAt(60.0, 0.0))
        assertEquals(4f, border.damageAt(75.0, 0.0))
    }
}
