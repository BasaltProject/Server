package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature
import org.basaltmc.wire.common.Identifier

/**
 * The placed features the desert, savanna, snowy and taiga village pools reference through their
 * `feature_pool_element` entries. Each mirrors its `placed_feature/<name>.json` modifier list:
 * count plus trapezoid offset, no in-square spread, since a village element scatters around its
 * own piece rather than the chunk corner.
 *
 * // parity: BlockPileFeature for the five piles, including hay's `rotated_block_provider`.
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
            "minecraft:pile_hay" -> placeBlockPile(chunk, random, x, y, z, ::rotatedHayState)
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
            "minecraft:acacia" ->
                if (supportsVegetation(chunk, x, y - 1, z)) {
                    TreeFeature.place(chunk, random, x, y, z, TreeConfigs.ACACIA)
                }
            "minecraft:oak" ->
                if (supportsVegetation(chunk, x, y - 1, z)) {
                    TreeFeature.place(chunk, random, x, y, z, TreeConfigs.OAK)
                }
            "minecraft:spruce" ->
                if (supportsVegetation(chunk, x, y - 1, z)) {
                    TreeFeature.place(chunk, random, x, y, z, TreeConfigs.SPRUCE)
                }
            "minecraft:pine" ->
                if (supportsVegetation(chunk, x, y - 1, z)) {
                    TreeFeature.place(chunk, random, x, y, z, TreeConfigs.PINE)
                }
            "minecraft:flower_plain" -> PlainsDecoration.placeFlowerPlainAt(chunk, random, x, y, z)
            "minecraft:patch_cactus" ->
                placePatch(
                    chunk,
                    random,
                    x,
                    y,
                    z,
                    10,
                    predicate = CactusFeature::canPlace,
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
                placePatch(
                    chunk,
                    random,
                    x,
                    y,
                    z,
                    96,
                    predicate = { c, pos ->
                        Blocks.isAir(c.getDecorationBlockStateId(pos.x, pos.y, pos.z)) &&
                            Blocks.byStateId(c.getDecorationBlockStateId(pos.x, pos.y - 1, pos.z))?.block?.path ==
                            "grass_block"
                    },
                ) { c, r, pos ->
                    SimpleBlockFeature({ _, _ -> SWEET_BERRY_AGE_3_STATE_ID }).place(c, r, pos)
                }
            // Village pools only reference the features above; an unknown name places nothing.
            else -> Unit
        }
    }

    /** // parity: oak's placed-feature would_survive oak_sapling predicate. */
    private fun supportsVegetation(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        val path = Blocks.byStateId(chunk.getDecorationBlockStateId(x, y, z))?.block?.path
        return path in SUPPORTS_VEGETATION_PATHS
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
        for (bz in z - zr..z + zr) {
            for (by in y..y + 1) {
                for (bx in x - xr..x + xr) {
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

    /** // parity: BlockPileFeature.tryPlaceBlock/mayPlaceOn. */
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
        if (!Blocks.isAir(chunk.getDecorationBlockStateId(x, y, z))) return
        if (y - 1 < WorldHeight.MIN_Y) return
        val below = chunk.getDecorationBlockStateId(x, y - 1, z)
        val mayPlace = mayPlacePileOn(below, dirtPathId, random)
        if (mayPlace) chunk.setDecorationBlockStateId(x, y, z, stateProvider(random))
    }

    internal fun mayPlacePileOn(
        belowStateId: Int,
        dirtPathStateId: Int,
        random: WorldgenRandom,
    ): Boolean =
        if (belowStateId == dirtPathStateId) {
            random.nextBoolean()
        } else {
            BlockShapes.isFaceSturdy(belowStateId, BlockFace.UP)
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
        predicate: (Chunk, DecorationPos) -> Boolean = { c, pos ->
            Blocks.isAir(c.getDecorationBlockStateId(pos.x, pos.y, pos.z))
        },
        place: (Chunk, WorldgenRandom, DecorationPos) -> Unit,
    ) {
        val context = PlacementContext(chunk)
        val offset = RandomOffsetPlacement(xzSpread = 7, ySpread = 3)
        var positions: Sequence<DecorationPos> =
            CountPlacement(
                count,
            ).getPositions(context, random, DecorationPos(x, y, z))
        positions = positions.flatMap { offset.getPositions(context, random, it) }
        positions = positions.filter { pos -> predicate(chunk, pos) }
        positions.forEach { pos -> place(chunk, random, pos) }
    }

    private fun state(name: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

    /** // parity: RotatedBlockProvider.getState/Direction.Axis.getRandom, whose enum order is X, Y, Z. */
    private fun rotatedHayState(random: WorldgenRandom): Int = HAY_AXIS_STATES[random.nextInt(HAY_AXIS_STATES.size)]

    private val HAY_AXIS_STATES =
        listOf("x", "y", "z").map { axis ->
            Blocks.stateIdOf(Identifier.vanilla("hay_block"), mapOf("axis" to axis))
                ?: error("unknown hay_block axis '$axis'")
        }
    private val SWEET_BERRY_AGE_3_STATE_ID =
        Blocks.stateIdOf(Identifier.vanilla("sweet_berry_bush"), mapOf("age" to "3"))
            ?: error("unknown mature sweet berry bush")

    private val SUPPORTS_VEGETATION_PATHS =
        setOf(
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "grass_block",
            "podzol",
            "mycelium",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "farmland",
        )
}
