package org.basaltmc.server.runtime.command

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
import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

private class Console : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(4.0, 70.0, -8.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

/** `/setworldspawn`, `/tellraw`, `/version` and the argument types they brought with them. */
class PlayerAndMessageCommandsTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 8, 64, 8),
            worldsRoot = "build/test-data/player-message-commands",
            logger = Logger("PlayerAndMessageCommandsTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    private val environment =
        CommandEnvironment(
            players = PlayerRegistry(),
            groupsProvider = { testGroups() },
            worldSeed = 0L,
            maxPlayers = 20,
            worlds = mapOf("world" to world),
            itemTagMembers = { tag ->
                if (tag == Identifier.vanilla("planks")) setOf(Identifier.vanilla("oak_planks")) else null
            },
        )

    private val dispatcher = CommandDispatcher(environment).also(::registerVanillaCommands)
    private val source = Console()

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun setWorldSpawnMovesTheWorldSpawnAndPersistsIt() {
        assertEquals(1, run("setworldspawn 100 70 -30"))
        assertEquals(Triple(100.5, 70.0, -29.5), world.spawn)
        world.saveMetadata()
        val reloaded = WorldMetadata.read("build/test-data/player-message-commands/world")
        checkNotNull(reloaded)
        assertEquals(100, reloaded.spawnX)
        assertEquals(-30, reloaded.spawnZ)
    }

    @Test
    fun setWorldSpawnWithNoPositionUsesTheRunnersOwn() {
        run("setworldspawn")
        assertEquals(Triple(4.5, 70.0, -7.5), world.spawn)
    }

    @Test
    fun relativeBlockCoordinatesResolveAgainstTheRunner() {
        run("setworldspawn ~ ~1 ~")
        assertEquals(Triple(4.5, 71.0, -7.5), world.spawn)
    }

    @Test
    fun theComponentArgumentReadsJsonAndRefusesAnythingElse() {
        val parsed =
            ComponentArgument.parse(StringReader("""{"text":"hi","color":"red"}"""), source, environment)
        assertEquals("hi", PlainTextSerializer.serialize(parsed))
        assertFailsWith<CommandSyntaxException> {
            ComponentArgument.parse(StringReader("not json"), source, environment)
        }
    }

    @Test
    fun versionReportsTheTargetedGameVersion() {
        assertEquals(1, run("version"))
        assertContains(source.messages.last(), "26.2")
    }

    @Test
    fun anAngleWrapsIntoTheRangeVanillaUses() {
        run("setworldspawn 0 64 0 540 0")
        // 540 degrees is 180, which vanilla stores as -180.
        assertEquals(-180f, world.spawnYaw)
    }
}
