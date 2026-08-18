package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.scoreboard.Objective
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.text.Component
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

private class SelectorConsole : CommandSource {
    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 64.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) = Unit
}

@OptIn(ExperimentalUuidApi::class)
class EntitySelectorTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/entity-selector",
            logger = Logger("EntitySelectorTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    private val scoreboard = Scoreboard()
    private val teams = Teams()

    private val environment =
        CommandEnvironment(
            players = PlayerRegistry(),
            groupsProvider = { testGroups() },
            worldSeed = 0L,
            maxPlayers = 20,
            worlds = mapOf("world" to world),
            scoreboard = scoreboard,
            teams = teams,
            entityTagMembers = { tag ->
                if (tag == Identifier.vanilla("cows")) setOf(Identifier.vanilla("cow")) else null
            },
        )

    private val source = SelectorConsole()

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private var nextId = 1

    private fun summon(
        type: String,
        x: Double,
        z: Double,
        customName: String? = null,
        tag: String? = null,
    ): ServerEntity {
        val entity =
            ServerEntity(
                entityId = nextId++,
                uuid = Uuid.random(),
                type = Identifier.vanilla(type),
                protocolType = 0,
                x = x,
                y = 64.0,
                z = z,
            )
        entity.customName = customName
        tag?.let { entity.tags += it }
        world.state.spawnEntity(entity)
        return entity
    }

    private fun select(selector: String): List<CommandTarget> =
        EntitySelector
            .parse(StringReader(selector), source, environment)
            .select(source, environment, playersOnly = false)

    @Test
    fun typeKeepsOnlyThatType() {
        summon("cow", 1.0, 0.0)
        summon("pig", 2.0, 0.0)
        assertEquals(listOf("cow"), select("@e[type=cow]").map { it.type.path })
    }

    @Test
    fun aNegatedTypeKeepsEverythingElse() {
        summon("cow", 1.0, 0.0)
        summon("pig", 2.0, 0.0)
        assertEquals(listOf("pig"), select("@e[type=!cow]").map { it.type.path })
    }

    @Test
    fun aTypeTagResolvesThroughTheTagTable() {
        summon("cow", 1.0, 0.0)
        summon("pig", 2.0, 0.0)
        assertEquals(listOf("cow"), select("@e[type=#minecraft:cows]").map { it.type.path })
    }

    @Test
    fun distanceBoundsBothWays() {
        summon("cow", 3.0, 0.0)
        summon("cow", 30.0, 0.0)
        assertEquals(1, select("@e[distance=..5]").size)
        assertEquals(1, select("@e[distance=10..]").size)
        assertEquals(2, select("@e[distance=0..100]").size)
    }

    @Test
    fun limitAndSortPickTheNearestFirst() {
        summon("cow", 30.0, 0.0)
        summon("cow", 3.0, 0.0)
        val nearest = select("@e[limit=1,sort=nearest]").single()
        assertEquals(3.0, nearest.x)
        val furthest = select("@e[limit=1,sort=furthest]").single()
        assertEquals(30.0, furthest.x)
    }

    @Test
    fun nSelectsTheNearestOfAnything() {
        summon("cow", 30.0, 0.0)
        summon("pig", 2.0, 0.0)
        assertEquals("pig", select("@n").single().type.path)
    }

    @Test
    fun nameMatchesTheCustomNameAndCanBeNegated() {
        summon("cow", 1.0, 0.0, customName = "Bessie")
        summon("cow", 2.0, 0.0)
        assertEquals(1, select("@e[name=Bessie]").size)
        assertEquals(1, select("@e[name=!Bessie]").size)
    }

    @Test
    fun tagMatchesAndAnEmptyTagMeansUntagged() {
        summon("cow", 1.0, 0.0, tag = "friendly")
        summon("cow", 2.0, 0.0)
        assertEquals(1, select("@e[tag=friendly]").size)
        assertEquals(1, select("@e[tag=]").size)
    }

    @Test
    fun scoresReadTheObjectiveByScoreboardName() {
        val cow = summon("cow", 1.0, 0.0)
        summon("cow", 2.0, 0.0)
        scoreboard.add(Objective("points", "dummy", Component.text("points")))
        scoreboard.setScore(cow.uuid.toString(), "points", 7)
        assertEquals(1, select("@e[scores={points=5..10}]").size)
        assertEquals(0, select("@e[scores={points=8..}]").size)
    }

    @Test
    fun teamMatchesMembershipAndItsAbsence() {
        val cow = summon("cow", 1.0, 0.0)
        summon("cow", 2.0, 0.0)
        val red = checkNotNull(teams.add("red"))
        teams.join(red, cow.uuid.toString())
        assertEquals(1, select("@e[team=red]").size)
        assertEquals(1, select("@e[team=]").size)
    }

    @Test
    fun nbtComparesAgainstTheEntityView() {
        summon("cow", 1.0, 0.0, customName = "Bessie")
        summon("cow", 2.0, 0.0)
        assertEquals(1, select("""@e[nbt={CustomName:"Bessie"}]""").size)
    }

    @Test
    fun anchorsMoveWhereDistanceIsMeasuredFrom() {
        summon("cow", 50.0, 0.0)
        assertEquals(0, select("@e[distance=..5]").size)
        assertEquals(1, select("@e[x=50,y=64,z=0,distance=..1]").size)
    }

    @Test
    fun aBoxKeepsWhatIsInsideIt() {
        summon("cow", 2.0, 2.0)
        summon("cow", 20.0, 0.0)
        assertEquals(1, select("@e[dx=5,dy=5,dz=5]").size)
    }

    @Test
    fun rotationRangesWrapAround() {
        val cow = summon("cow", 1.0, 0.0)
        cow.yaw = 170f
        assertEquals(1, select("@e[y_rotation=160..-160]").size)
        assertEquals(0, select("@e[y_rotation=-90..90]").size)
    }

    @Test
    fun anUnknownOptionIsASyntaxError() {
        val failure = assertFailsWith<CommandSyntaxException> { select("@e[nonsense=1]") }
        assertTrue("argument.entity.options.unknown" in failure.component.toString())
    }

    @Test
    fun twoPositiveTypesCanNeverMatchSoTheyAreRefused() {
        val failure = assertFailsWith<CommandSyntaxException> { select("@e[type=cow,type=pig]") }
        assertTrue("argument.entity.options.inapplicable" in failure.component.toString())
        // The negated form removes candidates instead of fixing one, so any number of them is fine.
        summon("sheep", 1.0, 0.0)
        assertEquals(1, select("@e[type=!cow,type=!pig]").size)
    }

    @Test
    fun aSwappedRangeIsRefused() {
        val failure = assertFailsWith<CommandSyntaxException> { select("@e[distance=10..1]") }
        assertTrue("argument.range.swapped" in failure.component.toString())
    }

    @Test
    fun aNegativeDistanceIsRefused() {
        val failure = assertFailsWith<CommandSyntaxException> { select("@e[distance=-5]") }
        assertTrue("argument.entity.options.distance.negative" in failure.component.toString())
    }

    @Test
    fun limitBelowOneIsRefused() {
        val failure = assertFailsWith<CommandSyntaxException> { select("@e[limit=0]") }
        assertTrue("argument.entity.options.limit.toosmall" in failure.component.toString())
    }

    @Test
    fun anUnclosedBracketIsRefused() {
        val failure = assertFailsWith<CommandSyntaxException> { select("@e[type=cow") }
        assertTrue("argument.entity.options.unterminated" in failure.component.toString())
    }

    @Test
    fun aBareWordIsAPlayerNameAndNeverAnEntity() {
        summon("cow", 1.0, 0.0, customName = "Bessie")
        assertEquals(0, select("Bessie").size)
    }

    /** parity: `@e` is every entity, and what is lying on the floor is one of them. */
    @Test
    fun droppedItemsAreEntitiesToo() {
        world.state.spawnItem(
            org.basaltmc.server.runtime.world.WorldState
                .DroppedItem(
                    entityId = nextId++,
                    uuid = Uuid.random(),
                    item = Identifier.vanilla("diamond"),
                    count = 3,
                    x = 5.0,
                    y = 64.0,
                    z = 5.0,
                ),
        )
        summon("cow", 6.0, 6.0)

        val items = select("@e[type=item]")

        assertEquals(1, items.size, "the dropped diamond")
        assertEquals(Identifier.vanilla("item"), items.single().type)
        assertEquals(2, select("@e").size, "and @e sees both it and the cow")
    }

    /** A selector that filters by distance reaches an item the same way it reaches a mob. */
    @Test
    fun anItemIsFilteredLikeAnythingElse() {
        world.state.spawnItem(
            org.basaltmc.server.runtime.world.WorldState
                .DroppedItem(
                    entityId = nextId++,
                    uuid = Uuid.random(),
                    item = Identifier.vanilla("diamond"),
                    count = 1,
                    x = 40.0,
                    y = 64.0,
                    z = 0.0,
                ),
        )

        assertTrue(select("@e[type=item,distance=..10]").isEmpty(), "forty blocks away is out of range")
        assertEquals(1, select("@e[type=item,distance=..50]").size)
    }
}
