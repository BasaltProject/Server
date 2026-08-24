package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals

class WorldStateGeneratedBlockWriteTest {
    @Test
    fun crossChunkDecorationWaitsForTheTargetChunk() {
        val world = WorldState(emptyList())
        val source = Chunk(0, 0)
        source.setDecorationBlockStateId(16, 70, 3, Blocks.OAK_LOG.stateId)

        world.install(source)
        assertEquals(Blocks.AIR.stateId, source.getBlockStateId(0, 70, 3))
        val target = world.install(Chunk(1, 0))
        assertEquals(Blocks.OAK_LOG.stateId, target.getBlockStateId(0, 70, 3))
    }

    @Test
    fun crossChunkDecorationAlsoAppliesWhenTheTargetIsAlreadyLoaded() {
        val world = WorldState(emptyList())
        val target = world.install(Chunk(1, 0))
        val source = Chunk(0, 0)
        source.setDecorationBlockStateId(16, 70, 3, Blocks.OAK_LOG.stateId)

        world.install(source)

        assertEquals(Blocks.OAK_LOG.stateId, target.getBlockStateId(0, 70, 3))
    }

    @Test
    fun deferredDecorationDoesNotReplaceRealTargetTerrain() {
        val world = WorldState(emptyList())
        val source = Chunk(0, 0)
        source.setDecorationBlockStateId(16, 70, 3, Blocks.OAK_LOG.stateId)
        world.install(source)
        val target = Chunk(1, 0)
        target.setBlockStateId(0, 70, 3, Blocks.STONE.stateId)

        world.install(target)

        assertEquals(Blocks.STONE.stateId, target.getBlockStateId(0, 70, 3))
    }

    @Test
    fun deferredTemplateWriteDeliberatelyReplacesRealTargetTerrain() {
        val world = WorldState(emptyList())
        val source = Chunk(0, 0)
        source.setDecorationBlockStateId(
            16,
            70,
            3,
            Blocks.OAK_LOG.stateId,
            replaceAnyExternalState = true,
        )
        world.install(source)
        val target = Chunk(1, 0)
        target.setBlockStateId(0, 70, 3, Blocks.STONE.stateId)

        world.install(target)

        assertEquals(Blocks.OAK_LOG.stateId, target.getBlockStateId(0, 70, 3))
    }

    @Test
    fun deferredDecorationCarriesItsBlockEntityToTheTarget() {
        val world = WorldState(emptyList())
        val source = Chunk(0, 0)
        val data = NbtCompound().apply { putInt("test", 42) }
        val beeNestStateId =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("bee_nest"))?.stateId ?: error("missing bee nest")
        source.setDecorationBlockStateId(16, 70, 3, beeNestStateId)
        source.setDecorationBlockEntity(
            16,
            70,
            3,
            beeNestStateId,
            Identifier.vanilla("beehive"),
            data,
        )
        world.install(source)

        val target = world.install(Chunk(1, 0))

        assertEquals(beeNestStateId, target.getBlockStateId(0, 70, 3))
        assertEquals(data, target.blockEntityAt(0, 70, 3)?.data)
    }

    @Test
    fun initialChunksAlsoHandOffCrossChunkDecoration() {
        val source = Chunk(0, 0)
        val target = Chunk(1, 0)
        source.setDecorationBlockStateId(16, 70, 3, Blocks.OAK_LOG.stateId)

        WorldState(listOf(source, target))

        assertEquals(Blocks.OAK_LOG.stateId, target.getBlockStateId(0, 70, 3))
    }

    @Test
    fun deferredDecorationCarriesItsScheduledTickOnlyWithTheAcceptedBlock() {
        val flower = Identifier.vanilla("closed_eyeblossom")
        val flowerStateId = Blocks.REGISTRY.byIdentifier(flower)?.stateId ?: error("missing closed eyeblossom")
        val world = WorldState(emptyList())
        val source = Chunk(0, 0)
        source.setDecorationBlockStateId(16, 70, 3, flowerStateId)
        source.scheduleDecorationBlockTick(16, 70, 3, flowerStateId, flower, delay = 1)
        world.install(source)

        val target = world.install(Chunk(1, 0))

        assertEquals(flowerStateId, target.getBlockStateId(0, 70, 3))
        assertEquals(1, target.scheduledBlockTicks().single().delay)
        assertEquals(flower, target.scheduledBlockTicks().single().type)

        val blockedWorld = WorldState(emptyList())
        val blockedSource = Chunk(0, 0)
        blockedSource.setDecorationBlockStateId(16, 70, 3, flowerStateId)
        blockedSource.scheduleDecorationBlockTick(16, 70, 3, flowerStateId, flower, delay = 1)
        blockedWorld.install(blockedSource)
        val blockedTarget = Chunk(1, 0).also { it.setBlockStateId(0, 70, 3, Blocks.STONE.stateId) }

        blockedWorld.install(blockedTarget)

        assertEquals(Blocks.STONE.stateId, blockedTarget.getBlockStateId(0, 70, 3))
        assertEquals(emptyList(), blockedTarget.scheduledBlockTicks())
    }
}
