package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Shared 26.2 `glow_lichen` placed feature. */
public object GlowLichenDecoration : BiomeVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        val random =
            WorldgenRandom(
                org.basaltmc.server.random
                    .XoroshiroRandom(seed),
            )
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, GLOW_LICHEN_INDEX, VEGETAL_DECORATION_STEP)
        decoratePlaced(chunk, random, environment, externalBlockState)
    }

    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ): Unit = decoratePlaced(chunk, random, environment = null, externalBlockState = null)

    private fun decoratePlaced(
        chunk: Chunk,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment?,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
    ) {
        val context = PlacementContext(chunk)
        val region = GlowLichenRegion(chunk, externalBlockState)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (
        modifier in
        listOf(
            CountPlacement(IntProvider.Uniform(104, 157)),
            UniformHeightRangePlacement(WorldHeight.MIN_Y, 256),
            InSquarePlacement,
            SurfaceRelativeThresholdFilter(Chunk.HeightmapType.OCEAN_FLOOR, maxInclusive = -13),
        )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { position ->
            val biome = environment?.biomeAt(position.x, position.y, position.z)?.substringAfter(':')
            if (biome == null || biome != "the_void") placeGrowth(region, random, position)
        }
    }

    /** parity: `glow_lichen`, grown at one position rather than scattered through a chunk. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean = placeGrowth(GlowLichenRegion(chunk, null), random, origin)

    private fun placeGrowth(
        region: GlowLichenRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        val oldState = region.get(origin) ?: return false
        if (!isAirWaterOrLichen(oldState)) return false

        val directions = VALID_DIRECTIONS.shuffled(random)
        if (placeOnFirstSupport(region, random, origin, oldState, directions)) return true

        for (searchDirection in directions) {
            val placementDirections = VALID_DIRECTIONS.filter { it != searchDirection.opposite }.shuffled(random)
            val target = origin.relative(searchDirection)
            val targetState = region.get(target) ?: continue
            if (!isAirWaterOrLichen(targetState)) continue
            // parity: 26.2 repeats this same one-block target searchRange times. Failed attempts
            // consume no RNG, so one evaluation is equivalent while preserving the vanilla bug.
            if (placeOnFirstSupport(region, random, target, targetState, placementDirections)) return true
        }
        return false
    }

    private fun placeOnFirstSupport(
        region: GlowLichenRegion,
        random: WorldgenRandom,
        position: DecorationPos,
        oldState: Int,
        directions: List<Direction>,
    ): Boolean {
        for (direction in directions) {
            val neighbour = region.get(position.relative(direction)) ?: continue
            if (neighbour !in canBePlacedOnStateIds) continue
            val newState = withFace(oldState, direction) ?: return false
            region.set(position, newState)
            if (random.nextFloat() < SPREAD_CHANCE) spread(region, random, position, newState, direction)
            return true
        }
        return false
    }

    private fun spread(
        region: GlowLichenRegion,
        random: WorldgenRandom,
        source: DecorationPos,
        sourceState: Int,
        startingFace: Direction,
    ) {
        for (spreadDirection in ALL_DIRECTIONS.shuffled(random)) {
            if (spreadDirection.axis == startingFace.axis || hasFace(sourceState, spreadDirection)) continue
            val candidates =
                listOf(
                    SpreadPos(source, spreadDirection),
                    SpreadPos(source.relative(spreadDirection), startingFace),
                    SpreadPos(source.relative(spreadDirection).relative(startingFace), spreadDirection.opposite),
                )
            for (candidate in candidates) {
                val oldState = region.get(candidate.position) ?: continue
                if (!isAirWaterOrLichen(oldState)) continue
                val neighbourPosition = candidate.position.relative(candidate.face)
                // MultifaceSpreader uses generic sturdy-face attachment after initial placement,
                // not the feature configuration's narrower can_be_placed_on list.
                if (!region.hasSturdyFace(neighbourPosition, candidate.face.opposite)) continue
                val newState = withFace(oldState, candidate.face) ?: continue
                region.set(candidate.position, newState)
                return
            }
        }
    }

    private fun withFace(
        oldState: Int,
        direction: Direction,
    ): Int? {
        var bits =
            when {
                Blocks.isGlowLichenState(oldState) -> oldState - firstGlowLichenStateId
                oldState == Blocks.WATER.stateId -> ALL_FACE_FALSE_BITS
                Blocks.isAir(oldState) -> ALL_FACE_FALSE_BITS or WATERLOGGED_FALSE_BIT
                else -> return null
            }
        if (bits and direction.falseBit == 0) return null
        bits = bits and direction.falseBit.inv()
        return firstGlowLichenStateId + bits
    }

    private fun hasFace(
        stateId: Int,
        direction: Direction,
    ): Boolean = Blocks.isGlowLichenState(stateId) && (stateId - firstGlowLichenStateId) and direction.falseBit == 0

    private fun isAirWaterOrLichen(stateId: Int): Boolean =
        Blocks.isAir(stateId) || stateId == Blocks.WATER.stateId || Blocks.isGlowLichenState(stateId)

    private class GlowLichenRegion(
        private val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
    ) {
        private val externalWrites = HashMap<DecorationPos, Int>()

        fun get(position: DecorationPos): Int? =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                else -> externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
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
        }

        fun hasSturdyFace(
            position: DecorationPos,
            face: Direction,
        ): Boolean =
            when (get(position)?.let(Blocks::byStateId)?.block?.path) {
                "sculk_sensor", "sculk_shrieker" -> face == Direction.DOWN
                in NON_STURDY_BLOCKS -> false
                else -> get(position)?.let(Chunk.HeightmapType.OCEAN_FLOOR::matches) == true
            }
    }

    private fun <T> List<T>.shuffled(random: WorldgenRandom): List<T> {
        val copy = toMutableList()
        for (size in copy.size downTo 2) {
            val swapTo = random.nextInt(size)
            val last = copy[size - 1]
            copy[size - 1] = copy[swapTo]
            copy[swapTo] = last
        }
        return copy
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private data class SpreadPos(
        val position: DecorationPos,
        val face: Direction,
    )

    private enum class Axis { X, Y, Z }

    private enum class Direction(
        val dx: Int,
        val dy: Int,
        val dz: Int,
        val axis: Axis,
        val falseBit: Int,
    ) {
        DOWN(0, -1, 0, Axis.Y, 64),
        UP(0, 1, 0, Axis.Y, 4),
        NORTH(0, 0, -1, Axis.Z, 16),
        SOUTH(0, 0, 1, Axis.Z, 8),
        WEST(-1, 0, 0, Axis.X, 1),
        EAST(1, 0, 0, Axis.X, 32),
        ;

        val opposite: Direction
            get() =
                when (this) {
                    DOWN -> UP
                    UP -> DOWN
                    NORTH -> SOUTH
                    SOUTH -> NORTH
                    WEST -> EAST
                    EAST -> WEST
                }
    }

    private fun DecorationPos.relative(direction: Direction): DecorationPos =
        DecorationPos(x + direction.dx, y + direction.dy, z + direction.dz)

    private const val WATERLOGGED_FALSE_BIT = 2
    private const val ALL_FACE_FALSE_BITS = 64 + 32 + 16 + 8 + 4 + 1
    private const val SPREAD_CHANCE = 0.5f
    private const val GLOW_LICHEN_INDEX = 0
    private const val VEGETAL_DECORATION_STEP = 9

    private val defaultGlowLichenStateId = stateId("glow_lichen")
    private val firstGlowLichenStateId = defaultGlowLichenStateId - 127
    private val canBePlacedOnStateIds =
        setOf(
            "stone",
            "andesite",
            "diorite",
            "granite",
            "dripstone_block",
            "calcite",
            "tuff",
            "deepslate",
            "sulfur",
            "cinnabar",
        ).map(::stateId)
            .toSet()
    private val VALID_DIRECTIONS =
        listOf(Direction.UP, Direction.NORTH, Direction.EAST, Direction.SOUTH, Direction.WEST)
    private val ALL_DIRECTIONS = Direction.entries
    private val NON_STURDY_BLOCKS =
        setOf("glow_lichen", "sculk_vein", "pointed_dripstone", "sulfur_spike", "snow")
}
