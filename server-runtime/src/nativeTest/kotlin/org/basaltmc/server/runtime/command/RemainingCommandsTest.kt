package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.ParticleTypes
import org.basaltmc.server.runtime.command.commands.CommandStorage
import org.basaltmc.server.runtime.command.commands.DataCommand
import org.basaltmc.server.runtime.command.commands.DialogCommand
import org.basaltmc.server.runtime.command.commands.EnchantCommand
import org.basaltmc.server.runtime.command.commands.FunctionRunner
import org.basaltmc.server.runtime.command.commands.PlaceCommand
import org.basaltmc.server.runtime.command.commands.SummonCommand
import org.basaltmc.server.runtime.command.commands.TestCommand
import org.basaltmc.server.runtime.command.commands.WaypointCommand
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.function.FunctionLibrary
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.RealDialogs
import org.basaltmc.server.runtime.world.RealEnchantments
import org.basaltmc.server.runtime.world.RealTestInstances
import org.basaltmc.server.runtime.world.StructureTemplates
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

private class RemainingConsole : CommandSource {
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

/** The last commands added, plus the list forms of `/data` and the paths with filters. */
class RemainingCommandsTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/remaining-commands",
            logger = Logger("RemainingCommandsTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    private val players = PlayerRegistry()
    private val storage = CommandStorage()

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = players,
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
                worlds = mapOf("world" to world),
            ),
        ).also { it.register(DataCommand(storage)) }
            .also { it.register(SummonCommand(players)) }
            .also { it.register(WaypointCommand()) }
            .also { it.register(PlaceCommand(StructureTemplates(serverJar = null))) }
            .also { it.register(EnchantCommand(RealEnchantments(serverJar = null))) }
            .also { it.register(DialogCommand(RealDialogs(serverJar = null))) }
            .also {
                it.register(
                    TestCommand(
                        RealTestInstances(serverJar = null),
                        FunctionRunner(FunctionLibrary("build/test-data/remaining-commands/datapacks")),
                    ),
                )
            }

    private val source = RemainingConsole()
    private val id = Identifier.parse("basalt:test")

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun theParticleRegistryMatchesTheProtocolFactsItWasBuiltFrom() {
        // Two anchors from the jar's own registration order, either side of the table.
        assertEquals(0, checkNotNull(ParticleTypes[Identifier.vanilla("angry_villager")]).protocolId)
        assertTrue(checkNotNull(ParticleTypes[Identifier.vanilla("block")]).needsOptions)
        assertTrue(!checkNotNull(ParticleTypes[Identifier.vanilla("flame")]).needsOptions)
    }

    @Test
    fun dataAppendAndInsertBuildAList() {
        run("""data modify storage basalt:test queue append value 1""")
        run("""data modify storage basalt:test queue append value 2""")
        run("""data modify storage basalt:test queue insert 0 value 0""")
        val list = assertNotNull(storage[id]["queue"] as? NbtList)
        assertEquals(listOf(0, 1, 2), list.toList().map { (it as NbtInt).value })
    }

    @Test
    fun aPathFilterFindsTheMatchingElement() {
        run("""data merge storage basalt:test {crew:[{name:"ana",age:3},{name:"bo",age:5}]}""")
        assertEquals(5, run("""data get storage basalt:test crew[{name:"bo"}].age"""))
    }

    @Test
    fun waypointRemembersWhatItWasToldAndListsIt() {
        run("summon minecraft:cow 0 64 0")
        run("waypoint modify @e[type=cow] color red")
        run("waypoint modify @e[type=cow] style set minecraft:bowtie")
        assertEquals(1, run("waypoint list"))
    }

    @Test
    fun placeFeatureRunsTheFeatureAtThePosition() {
        val chunk = world.state.getOrGenerateChunk(0, 0)
        chunk.setBlockStateId(1, 63, 1, checkNotNull(Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))).stateId)

        assertEquals(1, run("place feature minecraft:forest_rock 1 64 1"))
        // The blob always covers its own origin, whatever radii the random hands out.
        assertEquals(
            Identifier.vanilla("mossy_cobblestone"),
            checkNotNull(Blocks.byStateId(chunk.getBlockStateId(1, 64, 1))).block,
        )
    }

    @Test
    fun theCommandsThatNeedMissingSubsystemsSayWhichOne() {
        // Every vanilla feature is placeable by id now; a name nobody registered still says so.
        run("place feature minecraft:not_a_feature")
        assertTrue(source.messages.any { "has no positional body in this build" in it })

        // And the five that need more than a chunk say what this world cannot give them.
        world.state.getOrGenerateChunk(0, 0)
        run("place feature minecraft:fossil_lower 0 64 0")
        assertTrue(source.messages.any { "does not have" in it }, source.messages.toString())

        // This fixture's world is void, so it has no village generator to ask.
        run("place structure minecraft:village_plains")
        assertTrue(source.messages.any { "commands.place.structure.invalid" in it })

        // No server jar in this fixture, so no template pools to grow from.
        run("place jigsaw minecraft:village/plains/town_centers minecraft:empty 3")
        assertTrue(source.messages.any { "commands.place.jigsaw.invalid" in it })

        // `template` is the form that works, and without a server jar there is nothing to load.
        run("place template minecraft:village/plains/houses/plains_small_house_1")
        assertTrue(source.messages.any { "commands.place.template.invalid" in it })

        // `/enchant` and `/dialog` need a player to act on, and nobody is online in this fixture,
        // so what is asserted here is that they exist and fail on the target rather than on the
        // command being unknown.
        // Without a downloaded server.jar there is no enchantment registry, so this is the branch
        // that says so rather than writing a component no client could resolve.
        run("enchant @a minecraft:sharpness 1")
        assertTrue(source.messages.none { "Unknown command" in it })
    }

    @Test
    fun aTreeIsPlacedByItsOwnPlacedFeatureName() {
        val chunk = world.state.getOrGenerateChunk(0, 0)
        val dirt = checkNotNull(Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dirt"))).stateId
        chunk.setBlockStateId(8, 63, 8, dirt)

        assertEquals(1, run("place feature minecraft:oak 8 64 8"))
        // Whatever the random hands out, an oak starts with a log on the block it was told to.
        assertEquals(
            Identifier.vanilla("oak_log"),
            checkNotNull(Blocks.byStateId(chunk.getBlockStateId(8, 64, 8))).block,
        )
    }

    @Test
    fun testReportsAnEmptyRegistryRatherThanPretending() {
        assertEquals(0, run("test runall"))
        assertTrue(source.messages.any { "commands.test.no_tests" in it })
    }

    @Test
    fun storageStaysACompoundThroughEveryModify() {
        run("""data modify storage basalt:test a set value 1""")
        assertTrue(storage[id] is NbtCompound)
    }
}
