package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/** Shared 26.2 `glow_lichen` placed feature. */
public object GlowLichenDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val context = PlacementContext(chunk)
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
        positions.forEach { placeGrowth(chunk, random, it) }
    }

    private fun placeGrowth(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        val oldState = state(chunk, origin) ?: return false
        if (!isAirWaterOrLichen(oldState)) return false

        val directions = VALID_DIRECTIONS.shuffled(random)
        if (placeOnFirstSupport(chunk, random, origin, oldState, directions)) return true

        for (searchDirection in directions) {
            val placementDirections = VALID_DIRECTIONS.filter { it != searchDirection.opposite }.shuffled(random)
            val target = origin.relative(searchDirection)
            val targetState = state(chunk, target) ?: continue
            if (!isAirWaterOrLichen(targetState)) continue
            // parity: 26.2 repeats this same one-block target searchRange times. Failed attempts
            // consume no RNG, so one evaluation is equivalent while preserving the vanilla bug.
            if (placeOnFirstSupport(chunk, random, target, targetState, placementDirections)) return true
        }
        return false
    }

    private fun placeOnFirstSupport(
        chunk: Chunk,
        random: WorldgenRandom,
        position: DecorationPos,
        oldState: Int,
        directions: List<Direction>,
    ): Boolean {
        for (direction in directions) {
            val neighbour = state(chunk, position.relative(direction)) ?: continue
            if (neighbour !in canBePlacedOnStateIds) continue
            val newState = withFace(oldState, direction) ?: return false
            setState(chunk, position, newState)
            if (random.nextFloat() < SPREAD_CHANCE) spread(chunk, random, position, newState, direction)
            return true
        }
        return false
    }

    private fun spread(
        chunk: Chunk,
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
                val oldState = state(chunk, candidate.position) ?: continue
                if (!isAirWaterOrLichen(oldState)) continue
                val neighbour = state(chunk, candidate.position.relative(candidate.face)) ?: continue
                // MultifaceSpreader uses generic sturdy-face attachment after initial placement,
                // not the feature configuration's narrower can_be_placed_on list.
                if (!Chunk.HeightmapType.OCEAN_FLOOR.matches(neighbour)) continue
                val newState = withFace(oldState, candidate.face) ?: continue
                setState(chunk, candidate.position, newState)
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

    private fun state(
        chunk: Chunk,
        position: DecorationPos,
    ): Int? =
        if (position.y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
        } else {
            null
        }

    private fun setState(
        chunk: Chunk,
        position: DecorationPos,
        stateId: Int,
    ) {
        chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
    }

    private fun isAirWaterOrLichen(stateId: Int): Boolean =
        Blocks.isAir(stateId) || stateId == Blocks.WATER.stateId || Blocks.isGlowLichenState(stateId)

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
}
