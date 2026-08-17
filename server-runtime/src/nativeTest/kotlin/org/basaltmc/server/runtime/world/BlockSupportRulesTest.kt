package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * The survival rules a probe of one neighbour at a time cannot express, which is why each of these
 * is written by hand. A generated table answers the other fifty-odd classes.
 */
class BlockSupportRulesTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("support", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/support",
            Logger("support").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private fun state(
        name: String,
        props: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), props)) { name }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    @Test
    fun everyRuleThatNeedsMoreThanOneNeighbour() {
        world.state.getOrGenerateChunk(0, 0)

        fun check(
            label: String,
            build: (Int, Int, Int) -> Unit,
            self: Int,
        ) {
            val x = 1
            val y = 80
            val z = 1
            for (dy in -2..2) world.state.setBlockState(x, y + dy, z, Blocks.AIR.stateId)
            for (dx in -1..1) for (dz in -1..1) world.state.setBlockState(x + dx, y, z + dz, Blocks.AIR.stateId)
            build(x, y, z)
            world.state.setBlockState(x, y, z, self)
            assertTrue(NeighbourUpdates.isSupported(world, x, y, z, self), label)
        }
        check("kelp body over dirt", { x, y, z ->
            world.state.setBlockState(x, y - 1, z, Blocks.DIRT.stateId)
            world.state.setBlockState(x, y + 1, z, state("kelp", mapOf("age" to "0")))
        }, state("kelp_plant"))
        check("kelp body over kelp body", { x, y, z ->
            world.state.setBlockState(x, y - 1, z, state("kelp_plant"))
            world.state.setBlockState(x, y + 1, z, state("kelp", mapOf("age" to "0")))
        }, state("kelp_plant"))
        check("dripleaf stem under leaf", { x, y, z ->
            world.state.setBlockState(x, y - 1, z, state("clay"))
            world.state.setBlockState(x, y + 1, z, state("big_dripleaf", mapOf("facing" to "north")))
        }, state("big_dripleaf_stem", mapOf("facing" to "north")))
        check("moss carpet over bottom carpet", { x, y, z ->
            world.state.setBlockState(x, y - 1, z, state("pale_moss_carpet", mapOf("bottom" to "true")))
        }, state("pale_moss_carpet", mapOf("bottom" to "false", "north" to "low")))
        check("snow eight over snow", { x, y, z ->
            world.state.setBlockState(x, y - 1, z, state("snow", mapOf("layers" to "8")))
        }, state("snow", mapOf("layers" to "1")))
        check("tall seagrass upper", { x, y, z ->
            world.state.setBlockState(x, y - 1, z, state("tall_seagrass", mapOf("half" to "lower")))
        }, state("tall_seagrass", mapOf("half" to "upper")))
        check("ladder on stone", { x, y, z ->
            world.state.setBlockState(x, y, z + 1, Blocks.STONE.stateId)
        }, state("ladder", mapOf("facing" to "north")))
        check("weeping vines body", { x, y, z ->
            world.state.setBlockState(x, y + 1, z, state("weeping_vines_plant"))
        }, state("weeping_vines_plant"))
    }
}
