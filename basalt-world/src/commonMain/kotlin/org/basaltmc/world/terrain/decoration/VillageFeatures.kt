package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.world.terrain.decoration.tree.TreeFeature

/**
 * The real placed features the desert/savanna/snowy/taiga village `decor`/`trees` pools reference
 * through their `feature_pool_element` entries, dispatched by name from
 * [org.basaltmc.world.structure.FeaturePoolElement.place]. Every pipeline below mirrors the real
 * `placed_feature/<name>.json` modifier list (count + trapezoid random offset, no in-square
 * spread: a village feature element scatters around its own piece position, not the chunk corner).
 *
 * // parity: net.minecraft.world.level.levelgen.feature.BlockPileFeature for the five piles.
 * `mayPlaceOn`'s real `isFaceSturdy` test keeps this package's established "not air" stand-in for
 * a solid face (collision shapes unmodeled); `dirt_path`'s real coin flip is kept. Hay's
 * `rotated_block_provider` draws nothing (fixed Y axis, and axis properties aren't modeled).
 */
public object VillageFeatures {
    public fun place(
        feature: String,
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        random: WorldgenRandom,
    ) {
        when (feature) {
            "minecraft:pile_hay" -> placeBlockPile(chunk, random, x, y, z) { state("hay_block") }
            "minecraft:pile_melon" -> placeBlockPile(chunk, random, x, y, z) { state("melon") }
            "minecraft:pile_snow" -> placeBlockPile(chunk, random, x, y, z) { state("snow") }
            // parity: pile_ice's real weighted_state_provider: blue_ice weight 1, packed_ice weight 5.
            "minecraft:pile_ice" ->
                placeBlockPile(chunk, random, x, y, z) { r ->
                    if (r.nextInt(6) ==
                        0
                    ) {
                        state("blue_ice")
                    } else {
                        state("packed_ice")
                    }
                }
            // parity: pile_pumpkin's real weighted_state_provider: pumpkin weight 19, jack_o_lantern weight 1.
            "minecraft:pile_pumpkin" ->
                placeBlockPile(chunk, random, x, y, z) { r ->
                    if (r.nextInt(20) ==
                        19
                    ) {
                        state("jack_o_lantern")
                    } else {
                        state("pumpkin")
                    }
                }
            "minecraft:acacia" -> TreeFeature.place(chunk, random, x, y, z, TreeConfigs.ACACIA)
            "minecraft:spruce" -> TreeFeature.place(chunk, random, x, y, z, TreeConfigs.SPRUCE)
            "minecraft:pine" -> TreeFeature.place(chunk, random, x, y, z, TreeConfigs.PINE)
            "minecraft:patch_cactus" ->
                placePatch(
                    chunk,
                    random,
                    x,
                    y,
                    z,
                    10,
                ) { c, r, pos -> CactusFeature.place(c, r, pos) }
            "minecraft:patch_taiga_grass" ->
                // parity: taiga_grass's real weighted_state_provider: short_grass weight 1, fern weight 4.
                placePatch(chunk, random, x, y, z, 32) { c, r, pos ->
                    SimpleBlockFeature({ pr, _ ->
                        if (pr.nextInt(5) ==
                            0
                        ) {
                            state("short_grass")
                        } else {
                            state("fern")
                        }
                    }).place(c, r, pos)
                }
            "minecraft:patch_berry_bush" ->
                placePatch(chunk, random, x, y, z, 96, requireGrassBelow = true) { c, r, pos ->
                    SimpleBlockFeature({ _, _ -> state("sweet_berry_bush") }).place(c, r, pos)
                }
            // Pools only reference the features above; an unknown name places nothing.
            else -> Unit
        }
    }

    // parity: BlockPileFeature.place.
    private fun placeBlockPile(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        stateProvider: (WorldgenRandom) -> Int,
    ): Boolean {
        if (y < WorldHeight.MIN_Y + 5) return false
        val xr = 2 + random.nextInt(2)
        val zr = 2 + random.nextInt(2)
        val dirtPathId = state("dirt_path")
        for (bx in x - xr..x + xr) {
            for (by in y..y + 1) {
                for (bz in z - zr..z + zr) {
                    val xd = x - bx
                    val zd = z - bz
                    val near = xd * xd + zd * zd <= random.nextFloat() * 10.0f - random.nextFloat() * 6.0f
                    if (near || random.nextFloat() < 0.031f) {
                        tryPlacePileBlock(chunk, random, bx, by, bz, dirtPathId, stateProvider)
                    }
                }
            }
        }
        return true
    }

    /** // parity: BlockPileFeature.tryPlaceBlock/mayPlaceOn (face-sturdy check approximated, see this file's KDoc). */
    private fun tryPlacePileBlock(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        dirtPathId: Int,
        stateProvider: (WorldgenRandom) -> Int,
    ) {
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
        if (chunk.getBlockStateId(x and 15, y, z and 15) != Blocks.AIR.stateId) return
        if (y - 1 < WorldHeight.MIN_Y) return
        val below = chunk.getBlockStateId(x and 15, y - 1, z and 15)
        val mayPlace = if (below == dirtPathId) random.nextBoolean() else below != Blocks.AIR.stateId
        if (mayPlace) chunk.setBlockStateId(x and 15, y, z and 15, stateProvider(random))
    }

    /**
     * The shared shape of the three patch features: [count] attempts at the piece position with a
     * trapezoid(7, 3) offset each (the real placed features' own modifier list), then [place] runs
     * the configured feature, which does its own target checks.
     */
    private fun placePatch(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        count: Int,
        requireGrassBelow: Boolean = false,
        place: (Chunk, WorldgenRandom, DecorationPos) -> Unit,
    ) {
        val context = PlacementContext(chunk)
        val offset = RandomOffsetPlacement(xzSpread = 7, ySpread = 3)
        var positions: Sequence<DecorationPos> =
            CountPlacement(
                count,
            ).getPositions(context, random, DecorationPos(x, y, z))
        positions = positions.flatMap { offset.getPositions(context, random, it) }
        if (requireGrassBelow) {
            val grassId = state("grass_block")
            positions =
                positions.filter { pos ->
                    pos.y - 1 >= WorldHeight.MIN_Y &&
                        chunk.getBlockStateId(pos.x and 15, pos.y - 1, pos.z and 15) == grassId
                }
        }
        positions.forEach { pos -> place(chunk, random, pos) }
    }

    private fun state(name: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")
}
