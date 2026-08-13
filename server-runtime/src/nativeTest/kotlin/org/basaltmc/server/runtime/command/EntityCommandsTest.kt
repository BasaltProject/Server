package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.command.commands.AttributeCommand
import org.basaltmc.server.runtime.command.commands.DamageCommand
import org.basaltmc.server.runtime.command.commands.RideCommand
import org.basaltmc.server.runtime.command.commands.RotateCommand
import org.basaltmc.server.runtime.command.commands.SummonCommand
import org.basaltmc.server.runtime.command.commands.SwingCommand
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.entity.Attribute
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
import kotlin.test.assertNull
import kotlin.test.assertTrue

private class EntityConsole : CommandSource {
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

class EntityCommandsTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/entity-commands",
            logger = Logger("EntityCommandsTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

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
        ).also { it.register(SummonCommand(players)) }
            .also { it.register(DamageCommand()) }
            .also { it.register(RideCommand()) }
            .also { it.register(RotateCommand()) }
            .also { it.register(SwingCommand()) }
            .also { it.register(AttributeCommand()) }

    private val source = EntityConsole()

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun summonPutsARealEntityWithItsProtocolType() {
        assertEquals(1, run("summon minecraft:pig 10 65 10"))
        val entity = world.state.entities().single()
        assertEquals(Identifier.vanilla("pig"), entity.type)
        assertEquals(checkNotNull(EntityTypes[Identifier.vanilla("pig")]).protocolId, entity.protocolType)
        assertEquals(10.5, entity.x)
    }

    @Test
    fun summoningAPlayerIsRefused() {
        run("summon minecraft:player")
        assertTrue(source.messages.any { "commands.summon.failed" in it })
    }

    @Test
    fun anUnknownEntityTypeIsRefusedByName() {
        run("summon minecraft:not_a_mob")
        assertTrue(source.messages.any { "commands.summon.failed" in it })
    }

    @Test
    fun damageTakesHealthAndKillsAtZero() {
        run("summon minecraft:cow 0 64 0")
        val entity = world.state.entities().single()
        run("damage cow 5")
        assertEquals(15.0, entity.health)
        run("damage cow 20")
        assertTrue(entity.removed)
    }

    @Test
    fun rotateFacingAPositionPointsTheEntityAtIt() {
        run("summon minecraft:cow 0 64 0")
        val entity = world.state.entities().single()
        // Both positions are block centres: a bare integer in a vec3 is centre-corrected, so the
        // target has to be one too or the entity ends up looking slightly off to the side.
        run("rotate cow facing 0 64 10")
        // Straight along +Z is yaw 0 in Minecraft's own convention. A tolerance because the exact
        // result is -0.0, which is equal to 0.0 as a number and not as a boxed Float.
        assertEquals(0f, entity.yaw, absoluteTolerance = 0.001f)
        run("rotate cow facing 10 64 0")
        // Straight along +X is -90: yaw grows clockwise from +Z.
        assertEquals(-90f, entity.yaw, absoluteTolerance = 0.001f)
    }

    @Test
    fun rideLinksAndUnlinksTheTwoEntities() {
        run("summon minecraft:pig 0 64 0")
        run("summon minecraft:cow 1 64 0")
        val pig = world.state.entities().first { it.type.path == "pig" }
        val cow = world.state.entities().first { it.type.path == "cow" }
        run("ride pig mount cow")
        assertEquals(cow.entityId, pig.vehicleId)
        assertTrue(pig.entityId in cow.passengerIds)
        run("ride pig dismount")
        assertNull(pig.vehicleId)
    }

    @Test
    fun attributeBaseValuesAreSetReadAndReset() {
        run("summon minecraft:cow 0 64 0")
        val cow = world.state.entities().single()
        run("attribute cow minecraft:max_health base set 40")
        assertEquals(40.0, cow.attributeValues[Attribute.MAX_HEALTH])
        run("attribute cow minecraft:max_health base reset")
        assertNull(cow.attributeValues[Attribute.MAX_HEALTH])
    }

    @Test
    fun anUnknownAttributeIsRefused() {
        run("summon minecraft:cow 0 64 0")
        run("attribute cow minecraft:nonsense base get")
        assertTrue(source.messages.any { "attribute.unknown" in it })
    }

    @Test
    fun swingReportsHowManyItAnimated() {
        run("summon minecraft:cow 0 64 0")
        assertEquals(1, run("swing cow"))
    }
}
