package org.basaltmc.server.world

import org.basaltmc.server.registry.Blocks
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * The precomputed per-state table [Chunk.HeightmapType.matches] reads has to answer exactly what
 * the predicate chain it replaced answers, for every state id, or heightmaps silently shift.
 */
class HeightmapTypeTableTest {
    @Test
    fun `table agrees with the predicate chain for every block state`() {
        val chunk = Chunk(0, 0)
        for (type in Chunk.HeightmapType.entries) {
            for (stateId in 0 until Blocks.stateCount) {
                assertEquals(
                    expectedMatch(type, stateId),
                    type.matches(stateId),
                    "$type disagreed on state id $stateId",
                )
            }
        }
        // Keeps the unused-chunk warning honest: heightAt is the only caller that matters.
        assertEquals(WorldHeight.MIN_Y, chunk.heightAt(0, 0, Chunk.HeightmapType.WORLD_SURFACE))
    }

    private fun expectedMatch(
        type: Chunk.HeightmapType,
        stateId: Int,
    ): Boolean {
        val nonColliding = isNonColliding(stateId)
        return when (type) {
            Chunk.HeightmapType.WORLD_SURFACE -> !Blocks.isAir(stateId)
            Chunk.HeightmapType.OCEAN_FLOOR ->
                !Blocks.isAir(stateId) && stateId != Blocks.WATER.stateId && !nonColliding
            Chunk.HeightmapType.MOTION_BLOCKING ->
                Blocks.hasFluid(stateId) || (!Blocks.isAir(stateId) && !nonColliding)
            Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES ->
                !Blocks.isLeavesState(stateId) &&
                    (Blocks.hasFluid(stateId) || (!Blocks.isAir(stateId) && !nonColliding))
        }
    }

    private fun isNonColliding(stateId: Int): Boolean =
        Blocks.isGlowLichenState(stateId) ||
            Blocks.isVineState(stateId) ||
            Blocks.isCocoaState(stateId) ||
            Blocks.isPaleHangingMossState(stateId) ||
            Blocks.isMangrovePropaguleState(stateId) ||
            Blocks.isLeafLitterState(stateId) ||
            Blocks.isDoublePlantState(stateId) ||
            Blocks.isWildflowersState(stateId) ||
            Blocks.isPinkPetalsState(stateId) ||
            Blocks.isKelpState(stateId) ||
            Blocks.isSeaPickleState(stateId) ||
            Blocks.isCoralPlantState(stateId) ||
            NON_COLLIDING_NAMES.any {
                Blocks.REGISTRY
                    .byIdentifier(
                        org.basaltmc.wire.common.Identifier
                            .vanilla(it),
                    )?.stateId ==
                    stateId
            }

    private companion object {
        val NON_COLLIDING_NAMES =
            listOf(
                "short_grass",
                "tall_grass",
                "bush",
                "dandelion",
                "poppy",
                "azure_bluet",
                "oxeye_daisy",
                "cornflower",
                "orange_tulip",
                "red_tulip",
                "pink_tulip",
                "white_tulip",
                "seagrass",
                "tall_seagrass",
                "kelp",
                "kelp_plant",
                "sea_pickle",
                "tube_coral",
                "brain_coral",
                "bubble_coral",
                "fire_coral",
                "horn_coral",
                "tube_coral_fan",
                "brain_coral_fan",
                "bubble_coral_fan",
                "fire_coral_fan",
                "horn_coral_fan",
                "tube_coral_wall_fan",
                "brain_coral_wall_fan",
                "bubble_coral_wall_fan",
                "fire_coral_wall_fan",
                "horn_coral_wall_fan",
            )
    }
}
