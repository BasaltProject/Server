package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for seagrass, sea pickles and kelp. */
public object AquaticPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        decorateSpecs(chunk, seed, environment, SPECS, externalBlockState, blockWriteObserver)
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        name: String,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        decorateSpecs(
            chunk,
            seed,
            environment,
            listOf(SPECS.single { it.name == name }),
            externalBlockState,
            blockWriteObserver,
        )
    }

    private fun decorateSpecs(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        specs: List<AquaticSpec>,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val context = PlacementContext(chunk)
        val region = AquaticRegion(chunk, externalBlockState, blockWriteObserver)
        val chunkOrigin = DecorationPos(chunk.chunkX * 16, WorldHeight.MIN_Y, chunk.chunkZ * 16)
        for (spec in specs) {
            random.setFeatureSeed(decorationSeed, spec.index, VEGETAL_DECORATION_STEP)
            if (spec.rarity != null && random.nextFloat() >= 1.0f / spec.rarity) continue
            val outerCount =
                spec.noiseToCountRatio?.let { ratio ->
                    NoiseBasedCountPlacement(ratio, noiseFactor = 80.0)
                        .getPositions(context, random, chunkOrigin)
                        .count()
                } ?: 1
            repeat(outerCount) {
                val x = chunk.chunkX * 16 + random.nextInt(16)
                val z = chunk.chunkZ * 16 + random.nextInt(16)
                val y = context.height(x and 15, z and 15, Chunk.HeightmapType.OCEAN_FLOOR)
                if (environment.biomeAt(x, y, z).substringAfter(':') !in spec.biomes) return@repeat
                val origin = DecorationPos(x, y, z)
                when (spec.kind) {
                    AquaticKind.SEAGRASS ->
                        repeat(spec.bodyCount) {
                            placeSeagrass(region, random, origin, spec.tallProbability)
                        }
                    AquaticKind.SEA_PICKLE -> placeSeaPickles(region, random, origin)
                    AquaticKind.KELP -> placeKelp(region, random, origin)
                }
            }
        }
    }

    private fun placeSeagrass(
        region: AquaticRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
        tallProbability: Float,
    ) {
        val x = origin.x + random.nextInt(8) - random.nextInt(8)
        val z = origin.z + random.nextInt(8) - random.nextInt(8)
        val target = DecorationPos(x, region.oceanFloor(x, z), z)
        if (!isWaterBlock(region.get(target))) return
        val tall = random.nextDouble() < tallProbability.toDouble()
        if (!canSeagrassSurvive(region, target)) return
        if (!tall) {
            region.set(target, Blocks.SEAGRASS.stateId)
            return
        }
        val above = target.copy(y = target.y + 1)
        if (!isWaterBlock(region.get(above))) return
        region.set(target, Blocks.TALL_SEAGRASS.stateId)
        region.set(above, TALL_SEAGRASS_UPPER_STATE_ID)
    }

    private fun placeSeaPickles(
        region: AquaticRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        repeat(20) {
            val x = origin.x + random.nextInt(8) - random.nextInt(8)
            val z = origin.z + random.nextInt(8) - random.nextInt(8)
            val target = DecorationPos(x, region.oceanFloor(x, z), z)
            val stateId = Blocks.SEA_PICKLE.stateId + random.nextInt(4) * 2
            if (isWaterBlock(region.get(target)) && canSeaPickleSurvive(region, target)) {
                region.set(target, stateId)
            }
        }
    }

    private fun placeKelp(
        region: AquaticRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        var position = origin.copy(y = region.oceanFloor(origin.x, origin.z))
        if (!isWaterBlock(region.get(position))) return
        val height = 1 + random.nextInt(10)
        for (level in 0..height) {
            val above = position.copy(y = position.y + 1)
            if (
                isWaterBlock(region.get(position)) &&
                isWaterBlock(region.get(above)) &&
                canKelpSurvive(region, position)
            ) {
                if (level == height) {
                    region.set(position, Blocks.KELP.stateId + 20 + random.nextInt(4))
                } else {
                    region.set(position, Blocks.KELP_PLANT.stateId)
                }
                position = above
                continue
            }
            if (level <= 0) return
            val previous = position.copy(y = position.y - 1)
            val belowPrevious = previous.copy(y = previous.y - 1)
            if (canKelpSurvive(region, previous) && !Blocks.isKelpState(region.get(belowPrevious))) {
                region.set(previous, Blocks.KELP.stateId + 20 + random.nextInt(4))
            }
            return
        }
    }

    private fun canSeagrassSurvive(
        region: AquaticRegion,
        position: DecorationPos,
    ): Boolean {
        if (position.y <= WorldHeight.MIN_Y) return false
        val below = region.get(position.copy(y = position.y - 1))
        return below != MAGMA_BLOCK_STATE_ID && BlockShapes.isFaceSturdy(below, BlockFace.UP)
    }

    private fun canSeaPickleSurvive(
        region: AquaticRegion,
        position: DecorationPos,
    ): Boolean {
        if (position.y <= WorldHeight.MIN_Y) return false
        val below = region.get(position.copy(y = position.y - 1))
        val hasTopCollision = BlockShapes.collisionBoxes(below).any { box -> box[4] == BlockShapes.UNITS }
        return hasTopCollision || BlockShapes.isFaceSturdy(below, BlockFace.UP)
    }

    private fun canKelpSurvive(
        region: AquaticRegion,
        position: DecorationPos,
    ): Boolean {
        if (position.y <= WorldHeight.MIN_Y) return false
        val below = region.get(position.copy(y = position.y - 1))
        if (below == MAGMA_BLOCK_STATE_ID) return false
        return below == Blocks.KELP_PLANT.stateId ||
            Blocks.isKelpState(below) ||
            BlockShapes.isFaceSturdy(below, BlockFace.UP)
    }

    private class AquaticRegion(
        private val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        private val blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        private val externalWrites = HashMap<DecorationPos, Int>()

        fun get(position: DecorationPos): Int =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                else ->
                    externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
                        ?: Blocks.AIR.stateId
            }

        fun oceanFloor(
            x: Int,
            z: Int,
        ): Int {
            if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
                return chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.OCEAN_FLOOR)
            }
            for (y in WorldHeight.MAX_Y downTo WorldHeight.MIN_Y) {
                if (Chunk.HeightmapType.OCEAN_FLOOR.matches(get(DecorationPos(x, y, z)))) return y + 1
            }
            return WorldHeight.MIN_Y
        }

        fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
            if (position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
            } else {
                setExternalDecorationBlockStateId(chunk, externalWrites, externalBlockState, position, stateId)
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }
    }

    private data class AquaticSpec(
        val name: String,
        val index: Int,
        val kind: AquaticKind,
        val biomes: Set<String>,
        val bodyCount: Int = 1,
        val tallProbability: Float = 0.0f,
        val rarity: Int? = null,
        val noiseToCountRatio: Int? = null,
    )

    private enum class AquaticKind { SEAGRASS, SEA_PICKLE, KELP }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun isWaterBlock(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block?.path == "water"

    private val MAGMA_BLOCK_STATE_ID = stateId("magma_block")
    private val TALL_SEAGRASS_UPPER_STATE_ID =
        Blocks.upperDoublePlantStateId(Blocks.TALL_SEAGRASS.stateId)
            ?: error("missing tall seagrass upper half")
    private val SPECS =
        listOf(
            AquaticSpec("seagrass_swamp", 87, AquaticKind.SEAGRASS, setOf("mangrove_swamp", "swamp"), 64, 0.6f),
            AquaticSpec("seagrass_river", 90, AquaticKind.SEAGRASS, setOf("river"), 48, 0.4f),
            AquaticSpec("seagrass_warm", 97, AquaticKind.SEAGRASS, setOf("lukewarm_ocean", "warm_ocean"), 80, 0.3f),
            AquaticSpec("sea_pickle", 98, AquaticKind.SEA_PICKLE, setOf("warm_ocean"), rarity = 16),
            AquaticSpec("seagrass_deep_warm", 99, AquaticKind.SEAGRASS, setOf("deep_lukewarm_ocean"), 80, 0.8f),
            AquaticSpec(
                "kelp_warm",
                100,
                AquaticKind.KELP,
                setOf("deep_lukewarm_ocean", "lukewarm_ocean"),
                noiseToCountRatio = 80,
            ),
            AquaticSpec("seagrass_normal", 101, AquaticKind.SEAGRASS, setOf("ocean"), 48, 0.3f),
            AquaticSpec("seagrass_deep", 102, AquaticKind.SEAGRASS, setOf("deep_ocean"), 48, 0.8f),
            AquaticSpec("seagrass_cold", 103, AquaticKind.SEAGRASS, setOf("cold_ocean"), 32, 0.3f),
            AquaticSpec("seagrass_deep_cold", 104, AquaticKind.SEAGRASS, setOf("deep_cold_ocean"), 40, 0.8f),
            AquaticSpec(
                "kelp_cold",
                105,
                AquaticKind.KELP,
                setOf("cold_ocean", "deep_cold_ocean", "deep_ocean", "ocean"),
                noiseToCountRatio = 120,
            ),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
