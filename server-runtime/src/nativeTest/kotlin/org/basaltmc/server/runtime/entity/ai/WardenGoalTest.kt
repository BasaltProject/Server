package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.Wardens
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: a warden goes after what it has heard, and ignores what it has not. */
@OptIn(ExperimentalUuidApi::class)
class WardenGoalTest {
    private fun world(): WorldState {
        val chunks = mutableListOf<Chunk>()
        for (cx in -1..1) for (cz in -1..1) chunks += Chunk(cx, cz)
        chunks.forEach { chunk ->
            for (x in 0 until 16) for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
        }
        return WorldState(chunks)
    }

    private fun warden(world: WorldState): ServerEntity =
        world
            .spawnEntity(ServerEntity(1, Uuid.random(), Wardens.TYPE, 143, 14.5, 65.0, 8.5))
            .also { it.onGround = true }

    private fun player() =
        PlayerView(
            99,
            "player-uuid",
            8.5,
            65.0,
            8.5,
            0.6,
            1.8,
            targetable = true,
            heldItem = null,
            sneaking = false,
        )

    private fun run(
        world: WorldState,
        warden: ServerEntity,
        ticks: Int,
    ) {
        val selector = assertNotNull(MobGoals.forType(warden.type))
        val random = LegacyRandom(5L)
        repeat(ticks) {
            warden.tickCount++
            val context =
                MobContext(
                    world = world,
                    mob = warden,
                    random = random,
                    entities = listOf(warden),
                    players = listOf(player()),
                    actions = NoMobActions,
                    light = 15,
                    night = false,
                    speed = 0.3,
                )
            if ((warden.tickCount + warden.entityId) % 2 != 0) {
                selector.tickRunning(context, all = false)
            } else {
                selector.tick(context)
            }
            Navigation.tick(world, warden)
            MoveControl.tick(world, warden, 0.3)
            LookControl.tick(warden)
        }
    }

    @Test
    fun aWardenThatHasHeardNothingNeverTargetsAnybody() {
        val world = world()
        val warden = warden(world)

        run(world, warden, ticks = 200)

        assertNull(warden.targetId, "it has heard nothing, so there is nobody to go after")
    }

    @Test
    fun aWardenComesAfterWhatItHasHeardEnoughOf() {
        val world = world()
        val warden = warden(world)
        repeat(3) { Wardens.anger(warden, suspectId = 99, amount = Wardens.HEARD) }

        run(world, warden, ticks = 100)

        assertTrue(abs(warden.x - 8.5) < 5.0, "it stopped ${abs(warden.x - 8.5)} blocks away instead of closing in")
    }
}
