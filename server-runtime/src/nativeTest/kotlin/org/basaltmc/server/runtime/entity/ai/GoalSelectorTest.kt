package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `GoalSelector`, and the wandering a mob does with nothing else to do. */
@OptIn(ExperimentalUuidApi::class)
class GoalSelectorTest {
    private val noActions = NoMobActions

    private fun world(): WorldState {
        val chunks = mutableListOf<Chunk>()
        for (cx in -1..1) for (cz in -1..1) chunks += Chunk(cx, cz)
        chunks.forEach { chunk ->
            for (x in 0 until 16) for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
        }
        return WorldState(chunks)
    }

    private fun cow(world: WorldState): ServerEntity =
        world
            .spawnEntity(ServerEntity(1, Uuid.random(), Identifier.vanilla("cow"), 20, 8.5, 65.0, 8.5))
            .also { it.onGround = true }

    @Test
    fun aCowWithNothingElseToDoWandersOff() {
        val world = world()
        val cow = cow(world)
        val selector = assertNotNull(MobGoals.forType(cow.type))
        val random = LegacyRandom(7L)
        val startX = cow.x
        val startZ = cow.z

        repeat(400) { tick ->
            cow.tickCount++
            val context =
                MobContext(
                    world = world,
                    mob = cow,
                    random = random,
                    entities = listOf(cow),
                    players = emptyList(),
                    actions = noActions,
                    light = 15,
                    night = false,
                    speed = 0.25,
                )
            if ((cow.tickCount + cow.entityId) % 2 != 0) {
                selector.tickRunning(context, all = false)
            } else {
                selector.tick(context)
            }
            Navigation.tick(world, cow)
            MoveControl.tick(world, cow, 0.25)
            LookControl.tick(cow)
        }

        val moved = kotlin.math.abs(cow.x - startX) + kotlin.math.abs(cow.z - startZ)
        assertTrue(moved > 1.0, "the cow only moved $moved blocks in twenty seconds")
    }

    @Test
    fun aZombieGoesForWhoeverIsNearby() {
        val world = world()
        val zombie =
            world
                .spawnEntity(ServerEntity(2, Uuid.random(), Identifier.vanilla("zombie"), 151, 8.5, 65.0, 8.5))
                .also { it.onGround = true }
        val selector = assertNotNull(MobGoals.forType(zombie.type))
        val random = LegacyRandom(3L)
        val player =
            PlayerView(
                99,
                "player-uuid",
                16.5,
                65.0,
                8.5,
                0.6,
                1.8,
                targetable = true,
                heldItem = null,
                sneaking = false,
            )

        repeat(200) {
            zombie.tickCount++
            val context =
                MobContext(
                    world = world,
                    mob = zombie,
                    random = random,
                    entities = listOf(zombie),
                    players = listOf(player),
                    actions = noActions,
                    light = 0,
                    night = true,
                    speed = 0.23,
                )
            if ((zombie.tickCount + zombie.entityId) % 2 != 0) {
                selector.tickRunning(context, all = false)
            } else {
                selector.tick(context)
            }
            Navigation.tick(world, zombie)
            MoveControl.tick(world, zombie, 0.23)
            LookControl.tick(zombie)
        }

        assertTrue(zombie.x > 12.0, "the zombie stopped at ${zombie.x} instead of walking to the player")
    }

    @Test
    fun aSquidPushesItselfAboutInTheWater() {
        val chunks = mutableListOf<Chunk>()
        for (cx in -1..1) for (cz in -1..1) chunks += Chunk(cx, cz)
        val water = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("water")))
        chunks.forEach { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    chunk.setBlockStateId(x, 60, z, Blocks.STONE.stateId)
                    for (y in 61..70) chunk.setBlockStateId(x, y, z, water)
                }
            }
        }
        val world = WorldState(chunks)
        val squid =
            world.spawnEntity(ServerEntity(3, Uuid.random(), Identifier.vanilla("squid"), 90, 8.5, 65.0, 8.5))
        val selector = assertNotNull(MobGoals.forType(squid.type))
        val random = LegacyRandom(11L)
        val startX = squid.x
        val startZ = squid.z

        repeat(200) {
            squid.tickCount++
            val context =
                MobContext(
                    world = world,
                    mob = squid,
                    random = random,
                    entities = listOf(squid),
                    players = emptyList(),
                    actions = noActions,
                    light = 15,
                    night = false,
                    speed = 0.7,
                )
            selector.tick(context)
            Navigation.tick(world, squid)
            MoveControl.tick(world, squid, 0.7)
            LookControl.tick(squid)
        }

        val moved = kotlin.math.abs(squid.x - startX) + kotlin.math.abs(squid.z - startZ)
        assertTrue(moved > 1.0, "the squid only drifted $moved blocks")
        assertTrue(squid.y > 61.0, "it sank to ${squid.y} instead of swimming")
    }

    @Test
    fun aSlimeHopsRatherThanWalks() {
        val world = world()
        val slime =
            world
                .spawnEntity(ServerEntity(4, Uuid.random(), Identifier.vanilla("slime"), 106, 8.5, 65.0, 8.5))
                .also { it.onGround = true }
        val selector = assertNotNull(MobGoals.forType(slime.type))
        val random = LegacyRandom(5L)
        var jumps = 0

        repeat(200) {
            slime.tickCount++
            val wasOnGround = slime.onGround
            val context =
                MobContext(
                    world = world,
                    mob = slime,
                    random = random,
                    entities = listOf(slime),
                    players = emptyList(),
                    actions = noActions,
                    light = 15,
                    night = false,
                    speed = 0.3,
                )
            selector.tick(context)
            Navigation.tick(world, slime)
            MoveControl.tick(world, slime, 0.3)
            if (wasOnGround && slime.velocityY > 0.0) jumps++
        }

        // parity: ten to thirty ticks between hops, plus the time in the air: a handful in ten seconds.
        assertTrue(jumps >= 3, "the slime hopped $jumps times in ten seconds")
        // parity: a small slime moves at 0.3, which is a hop of a couple of blocks and not a sprint.
        val covered = kotlin.math.abs(slime.x - 8.5) + kotlin.math.abs(slime.z - 8.5)
        assertTrue(covered < 20.0, "the slime covered $covered blocks in ten seconds")
    }

    @Test
    fun aCreeperWithNobodyToReachStillWanders() {
        val world = world()
        val creeper =
            world
                .spawnEntity(ServerEntity(5, Uuid.random(), Identifier.vanilla("creeper"), 32, 8.5, 65.0, 8.5))
                .also { it.onGround = true }
        val selector = assertNotNull(MobGoals.forType(creeper.type))
        val random = LegacyRandom(13L)
        // A player far below, which is what a mob on a platform sees: a target it cannot path to.
        val player =
            PlayerView(
                99,
                "player-uuid",
                8.5,
                20.0,
                8.5,
                0.6,
                1.8,
                targetable = true,
                heldItem = null,
                sneaking = false,
            )
        val startX = creeper.x
        val startZ = creeper.z

        repeat(400) {
            creeper.tickCount++
            val context =
                MobContext(
                    world = world,
                    mob = creeper,
                    random = random,
                    entities = listOf(creeper),
                    players = listOf(player),
                    actions = noActions,
                    light = 15,
                    night = false,
                    speed = 0.25,
                )
            selector.tick(context)
            Navigation.tick(world, creeper)
            MoveControl.tick(world, creeper, 0.25)
            LookControl.tick(creeper)
        }

        val moved = kotlin.math.abs(creeper.x - startX) + kotlin.math.abs(creeper.z - startZ)
        assertTrue(moved > 1.0, "the creeper only moved $moved blocks with an unreachable player")
    }
}
