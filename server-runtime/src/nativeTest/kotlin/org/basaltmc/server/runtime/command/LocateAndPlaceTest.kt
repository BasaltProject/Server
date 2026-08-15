package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.commands.LocateCommand
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private class LocateConsole : CommandSource {
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

/** `/locate poi`, which reads the loaded chunks because this build keeps no point-of-interest index. */
class LocateAndPlaceTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/locate",
            logger = Logger("LocateTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
                worlds = mapOf("world" to world),
            ),
        ).also { it.register(LocateCommand()) }

    private val source = LocateConsole()

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    /** The registered command, not a fixture: what `/locate poi` completes with has to be its own list. */
    @Test
    fun poiNamesAreCompletedFromTheRegistry() {
        val result = dispatcher.suggest(source, "/locate poi she")

        assertEquals(listOf("minecraft:shepherd"), result.suggestions.map { it.text })
        assertEquals("/locate poi ".length, result.start)
    }

    @Test
    fun aWorkstationIsFoundWhereItWasPut() {
        val chunk = world.state.getOrGenerateChunk(0, 0)
        val loom = checkNotNull(Blocks.REGISTRY.byIdentifier(Identifier.vanilla("loom"))).stateId
        chunk.setBlockStateId(5, 70, 6, loom)

        // The command answers with the distance in blocks: (5,70,6) from (0,64,0) rounds to 10.
        assertEquals(10, run("locate poi minecraft:shepherd"))
        assertTrue(source.messages.any { "commands.locate.poi.success" in it }, source.messages.toString())
    }

    @Test
    fun anEmptyWorldReportsNotFoundRatherThanTheOrigin() {
        world.state.getOrGenerateChunk(0, 0)
        run("locate poi minecraft:armorer")
        assertTrue(source.messages.any { "commands.locate.poi.not_found" in it })
    }

    @Test
    fun aTypeNobodyRegisteredIsRefusedByName() {
        run("locate poi minecraft:not_a_poi")
        assertTrue(source.messages.any { "commands.locate.poi.not_found" in it })
    }
}
