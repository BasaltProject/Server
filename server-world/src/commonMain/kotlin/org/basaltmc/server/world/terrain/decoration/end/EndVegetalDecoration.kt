package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.DensityChunkSampler
import org.basaltmc.server.world.terrain.EndNoiseRouter
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import kotlin.math.abs

/** // parity: `chorus_plant`, the End highlands VEGETAL_DECORATION feature. */
public object EndVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        router: EndNoiseRouter,
        biomeSource: TheEndBiomeSource,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, CHORUS_PLANT_INDEX, VEGETAL_DECORATION_STEP)
        val world = ChorusWorld(chunk, router)
        repeat(random.nextInt(5)) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
            if (biomeSource.biomeAtQuart(x shr 2, y shr 2, z shr 2) != Biomes.END_HIGHLANDS) return@repeat
            val origin = EndPos(x, y, z)
            if (world.isEmpty(origin) && world.pathAt(origin.below()) == "end_stone") {
                generatePlant(world, origin, random)
            }
        }
    }

    /** parity: `chorus_plant`, grown at one position rather than scattered over a chunk. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        router: EndNoiseRouter,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val world = ChorusWorld(chunk, router)
        val origin = EndPos(x, y, z)
        if (!world.isEmpty(origin) || world.pathAt(origin.below()) != "end_stone") return false
        generatePlant(world, origin, random)
        return true
    }

    private fun generatePlant(
        world: ChorusWorld,
        origin: EndPos,
        random: BasaltRandom,
    ) {
        world.setPlant(origin)
        growTreeRecursive(world, origin, random, origin, 0)
    }

    private fun growTreeRecursive(
        world: ChorusWorld,
        current: EndPos,
        random: BasaltRandom,
        start: EndPos,
        depth: Int,
    ) {
        var height = random.nextInt(4) + 1
        if (depth == 0) height++
        for (offsetY in 1..height) {
            val target = current.above(offsetY)
            if (!world.allHorizontalNeighborsEmpty(target, null)) return
            world.setPlant(target)
            world.setPlant(target.below())
        }

        var placedStem = false
        if (depth < MAX_BRANCH_DEPTH) {
            var stems = random.nextInt(4)
            if (depth == 0) stems++
            repeat(stems) {
                val direction = HORIZONTAL_DIRECTIONS[random.nextInt(HORIZONTAL_DIRECTIONS.size)]
                val target = current.above(height).relative(direction)
                if (
                    abs(target.x - start.x) < MAX_HORIZONTAL_SPREAD &&
                    abs(target.z - start.z) < MAX_HORIZONTAL_SPREAD &&
                    world.isEmpty(target) &&
                    world.isEmpty(target.below()) &&
                    world.allHorizontalNeighborsEmpty(target, direction.opposite)
                ) {
                    placedStem = true
                    world.setPlant(target)
                    world.setPlant(target.relative(direction.opposite))
                    growTreeRecursive(world, target, random, start, depth + 1)
                }
            }
        }
        if (!placedStem) world.setBlock(current.above(height), Blocks.chorusFlowerStateId(5))
    }

    private class ChorusWorld(
        private val chunk: Chunk,
        private val router: EndNoiseRouter,
    ) {
        private val outsideWrites = mutableMapOf<EndPos, Int>()
        private val samplers = mutableMapOf<Pair<Int, Int>, DensityChunkSampler>()

        fun isEmpty(position: EndPos): Boolean = Blocks.isAir(stateAt(position))

        fun pathAt(position: EndPos): String? = Blocks.byStateId(stateAt(position))?.block?.path

        fun allHorizontalNeighborsEmpty(
            position: EndPos,
            ignored: HorizontalDirection?,
        ): Boolean =
            HORIZONTAL_DIRECTIONS.all { direction ->
                direction == ignored || isEmpty(position.relative(direction))
            }

        fun setPlant(position: EndPos) {
            val stateId =
                Blocks.chorusPlantStateId(
                    down = pathAt(position.below()) in CHORUS_DOWN_CONNECTIONS,
                    east = pathAt(position.relative(HorizontalDirection.EAST)) in CHORUS_CONNECTIONS,
                    north = pathAt(position.relative(HorizontalDirection.NORTH)) in CHORUS_CONNECTIONS,
                    south = pathAt(position.relative(HorizontalDirection.SOUTH)) in CHORUS_CONNECTIONS,
                    up = pathAt(position.above()) in CHORUS_CONNECTIONS,
                    west = pathAt(position.relative(HorizontalDirection.WEST)) in CHORUS_CONNECTIONS,
                )
            setBlock(position, stateId)
        }

        fun setBlock(
            position: EndPos,
            stateId: Int,
        ) {
            if (position.y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
            val expectedStateId = stateAt(position)
            outsideWrites[position] = stateId
            chunk.setDecorationBlockStateId(
                position.x,
                position.y,
                position.z,
                stateId,
                expectedExternalStateId = expectedStateId,
            )
        }

        private fun stateAt(position: EndPos): Int {
            outsideWrites[position]?.let { return it }
            if (position.y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return Blocks.AIR.stateId
            if (position.x.floorDiv(16) == chunk.chunkX && position.z.floorDiv(16) == chunk.chunkZ) {
                return chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
            }
            if (position.y !in 0 until END_GENERATION_DEPTH) return Blocks.AIR.stateId
            val sampleChunkX = position.x.floorDiv(16)
            val sampleChunkZ = position.z.floorDiv(16)
            val sampler =
                samplers.getOrPut(sampleChunkX to sampleChunkZ) {
                    DensityChunkSampler(router.finalDensity, sampleChunkX, sampleChunkZ, 0, END_GENERATION_DEPTH, 8, 4)
                }
            return if (sampler.sample(position.x and 15, position.y, position.z and 15) > 0.0) {
                Blocks.END_STONE.stateId
            } else {
                Blocks.AIR.stateId
            }
        }
    }

    private data class EndPos(
        val x: Int,
        val y: Int,
        val z: Int,
    ) {
        fun above(distance: Int = 1): EndPos = copy(y = y + distance)

        fun below(): EndPos = copy(y = y - 1)

        fun relative(direction: HorizontalDirection): EndPos = copy(x = x + direction.dx, z = z + direction.dz)
    }

    private enum class HorizontalDirection(
        val dx: Int,
        val dz: Int,
    ) {
        NORTH(0, -1),
        EAST(1, 0),
        SOUTH(0, 1),
        WEST(-1, 0),
        ;

        val opposite: HorizontalDirection
            get() =
                when (this) {
                    NORTH -> SOUTH
                    EAST -> WEST
                    SOUTH -> NORTH
                    WEST -> EAST
                }
    }

    private val HORIZONTAL_DIRECTIONS = HorizontalDirection.entries
    private val CHORUS_CONNECTIONS = setOf("chorus_plant", "chorus_flower")
    private val CHORUS_DOWN_CONNECTIONS = CHORUS_CONNECTIONS + "end_stone"
    private const val END_GENERATION_DEPTH = 128
    private const val MAX_BRANCH_DEPTH = 4
    private const val MAX_HORIZONTAL_SPREAD = 8
    private const val VEGETAL_DECORATION_STEP = 9
    private const val CHORUS_PLANT_INDEX = 0
}
