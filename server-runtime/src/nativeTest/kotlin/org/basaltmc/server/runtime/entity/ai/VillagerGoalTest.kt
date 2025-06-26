package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: the villager's schedule, read by the goals that walk it about. */
@OptIn(ExperimentalUuidApi::class)
class VillagerGoalTest {
    private val noActions = NoMobActions

    private fun world(): WorldState {
        val chunks = mutableListOf<Chunk>()
        for (cx in -1..1) for (cz in -1..1) chunks += Chunk(cx, cz)
        chunks.forEach { chunk ->
            for (x in 0 until 16) for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
        }
        return WorldState(chunks)
    }

    private fun villager(world: WorldState): ServerEntity =
        world
            .spawnEntity(ServerEntity(1, Uuid.random(), Identifier.vanilla("villager"), 149, 8.5, 65.0, 8.5))
            .also { it.onGround = true }

    private fun run(
        world: WorldState,
        villager: ServerEntity,
        dayTime: Long,
        ticks: Int = 400,
    ) {
        val selector = assertNotNull(MobGoals.forType(villager.type))
        val random = LegacyRandom(11L)
        repeat(ticks) {
            villager.tickCount++
            val context =
                MobContext(
                    world = world,
                    mob = villager,
                    random = random,
                    entities = listOf(villager),
                    players = emptyList(),
                    actions = noActions,
                    light = 15,
                    night = dayTime >= 12000,
                    dayTime = dayTime,
                    speed = 0.5,
                )
            if ((villager.tickCount + villager.entityId) % 2 != 0) {
                selector.tickRunning(context, all = false)
            } else {
                selector.tick(context)
            }
            Navigation.tick(world, villager)
            MoveControl.tick(world, villager, 0.5)
            LookControl.tick(villager)
        }
    }

    @Test
    fun aVillagerWithNoVillageAroundStillWandersOff() {
        val world = world()
        val villager = villager(world)
        val startX = villager.x
        val startZ = villager.z

        run(world, villager, dayTime = 6000)

        val moved = abs(villager.x - startX) + abs(villager.z - startZ)
        assertTrue(moved > 1.0, "the villager only moved $moved blocks with nowhere to be")
    }

    @Test
    fun aVillagerWalksToItsBedAtNight() {
        val world = world()
        val villager = villager(world)
        val bed = assertNotNull(Blocks.stateIdOf(Identifier.vanilla("red_bed"), emptyMap()))
        world.setBlockState(2, 65, 2, bed)

        run(world, villager, dayTime = 13000)

        val distance = abs(villager.x - 2.5) + abs(villager.z - 2.5)
        assertTrue(distance < 3.0, "the villager stopped $distance blocks from the bed it should be in")
    }
}
