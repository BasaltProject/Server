package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/** // parity: warm_ocean_vegetation and the three CoralFeature implementations. */
public object CoralDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> =
            sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        positions =
            positions.flatMap {
                NoiseBasedCountPlacement(noiseToCountRatio = 20, noiseFactor = 400.0)
                    .getPositions(context, random, it)
            }
        positions = positions.flatMap { InSquarePlacement.getPositions(context, random, it) }
        positions =
            positions.flatMap {
                HeightmapPlacement(Chunk.HeightmapType.OCEAN_FLOOR).getPositions(context, random, it)
            }
        positions.forEach { origin ->
            val feature = random.nextInt(3)
            val coralState = CORAL_BLOCKS[random.nextInt(CORAL_BLOCKS.size)]
            when (feature) {
                0 -> placeTree(chunk, random, origin, coralState)
                1 -> placeClaw(chunk, random, origin, coralState)
                else -> placeMushroom(chunk, random, origin, coralState)
            }
        }
    }

    private fun placeTree(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        stateId: Int,
    ) {
        var pos = origin
        val trunkHeight = random.nextInt(3) + 1
        repeat(trunkHeight) {
            if (!placeCoralBlock(chunk, random, pos, stateId)) return
            pos = pos.up()
        }

        val trunkTop = pos
        val branchCount = random.nextInt(3) + 2
        val directions = shuffled(HORIZONTAL_DIRECTIONS.toMutableList(), random)
        for (direction in directions.take(branchCount)) {
            pos = trunkTop.move(direction)
            val branchHeight = random.nextInt(5) + 2
            var segmentLength = 0
            for (index in 0 until branchHeight) {
                if (!placeCoralBlock(chunk, random, pos, stateId)) break
                segmentLength++
                pos = pos.up()
                if (index == 0 || (segmentLength >= 2 && random.nextFloat() < 0.25f)) {
                    pos = pos.move(direction)
                    segmentLength = 0
                }
            }
        }
    }

    private fun placeClaw(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        stateId: Int,
    ) {
        if (!placeCoralBlock(chunk, random, origin, stateId)) return

        val clawDirection = HORIZONTAL_DIRECTIONS[random.nextInt(4)]
        val branchCount = random.nextInt(2) + 2
        val possibleDirections =
            shuffled(
                mutableListOf(clawDirection, clawDirection.clockwise(), clawDirection.counterClockwise()),
                random,
            )
        for (branchDirection in possibleDirections.take(branchCount)) {
            var pos = origin.move(branchDirection)
            val sidewaysLength = random.nextInt(2) + 1
            val inwardLength: Int
            val segmentDirection: Direction
            if (branchDirection == clawDirection) {
                segmentDirection = clawDirection
                inwardLength = random.nextInt(3) + 2
            } else {
                pos = pos.up()
                segmentDirection = if (random.nextInt(2) == 0) branchDirection else Direction.UP
                inwardLength = random.nextInt(3) + 3
            }

            for (index in 0 until sidewaysLength) {
                if (!placeCoralBlock(chunk, random, pos, stateId)) break
                pos = pos.move(segmentDirection)
            }
            pos = pos.move(segmentDirection.opposite()).up()

            for (index in 0 until inwardLength) {
                pos = pos.move(clawDirection)
                if (!placeCoralBlock(chunk, random, pos, stateId)) break
                if (random.nextFloat() < 0.25f) pos = pos.up()
            }
        }
    }

    private fun placeMushroom(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        stateId: Int,
    ) {
        val height = random.nextInt(3) + 3
        val width = random.nextInt(3) + 3
        val length = random.nextInt(3) + 3
        val sink = random.nextInt(3) + 1
        for (x in 0..width) {
            for (y in 0..height) {
                for (z in 0..length) {
                    val xEdge = x == 0 || x == width
                    val yEdge = y == 0 || y == height
                    val zEdge = z == 0 || z == length
                    val shell =
                        (!xEdge || !yEdge) &&
                            (!zEdge || !yEdge) &&
                            (!xEdge || !zEdge) &&
                            (xEdge || yEdge || zEdge)
                    if (shell && random.nextFloat() >= 0.1f) {
                        placeCoralBlock(
                            chunk,
                            random,
                            DecorationPos(origin.x + x, origin.y + y - sink, origin.z + z),
                            stateId,
                        )
                    }
                }
            }
        }
    }

    private fun placeCoralBlock(
        chunk: Chunk,
        random: WorldgenRandom,
        pos: DecorationPos,
        stateId: Int,
    ): Boolean {
        if (!pos.inWorld() || !pos.up().inWorld()) return false
        val target = chunk.blockAt(pos)
        if ((target != Blocks.WATER.stateId && target !in CORALS) || chunk.blockAt(pos.up()) != Blocks.WATER.stateId) {
            return false
        }

        chunk.setBlock(pos, stateId)
        if (random.nextFloat() < 0.25f) {
            chunk.setBlock(pos.up(), CORALS[random.nextInt(CORALS.size)])
        } else if (random.nextFloat() < 0.05f) {
            random.nextInt(4)
            chunk.setBlock(pos.up(), Blocks.SEA_PICKLE.stateId)
        }

        for (direction in HORIZONTAL_DIRECTIONS) {
            if (random.nextFloat() < 0.2f) {
                val side = pos.move(direction)
                if (chunk.blockAt(side) == Blocks.WATER.stateId) {
                    chunk.setBlock(side, WALL_CORALS[random.nextInt(WALL_CORALS.size)])
                }
            }
        }
        return true
    }

    private fun shuffled(
        values: MutableList<Direction>,
        random: WorldgenRandom,
    ): List<Direction> {
        for (size in values.size downTo 2) {
            val swapTo = random.nextInt(size)
            val last = values[size - 1]
            values[size - 1] = values[swapTo]
            values[swapTo] = last
        }
        return values
    }

    private fun DecorationPos.move(direction: Direction): DecorationPos =
        DecorationPos(x + direction.x, y + direction.y, z + direction.z)

    private fun DecorationPos.up(): DecorationPos = DecorationPos(x, y + 1, z)

    private fun DecorationPos.inWorld(): Boolean = y in WorldHeight.MIN_Y..WorldHeight.MAX_Y

    private fun Chunk.blockAt(pos: DecorationPos): Int =
        getBlockStateId(pos.x and 15, pos.y, pos.z and 15)

    private fun Chunk.setBlock(
        pos: DecorationPos,
        stateId: Int,
    ) {
        setBlockStateId(pos.x and 15, pos.y, pos.z and 15, stateId)
    }

    private data class Direction(
        val x: Int,
        val y: Int,
        val z: Int,
    ) {
        fun clockwise(): Direction = Direction(-z, y, x)

        fun counterClockwise(): Direction = Direction(z, y, -x)

        fun opposite(): Direction = Direction(-x, -y, -z)

        companion object {
            val UP = Direction(0, 1, 0)
        }
    }

    private val HORIZONTAL_DIRECTIONS =
        listOf(
            Direction(0, 0, -1),
            Direction(1, 0, 0),
            Direction(0, 0, 1),
            Direction(-1, 0, 0),
        )

    private val CORAL_BLOCKS = blockStates("tube", "brain", "bubble", "fire", "horn", suffix = "_coral_block")
    private val CORALS =
        blockStates("tube", "brain", "bubble", "fire", "horn", suffix = "_coral") +
            blockStates("tube", "brain", "bubble", "fire", "horn", suffix = "_coral_fan")
    private val WALL_CORALS = blockStates("tube", "brain", "bubble", "fire", "horn", suffix = "_coral_wall_fan")

    private fun blockStates(
        vararg names: String,
        suffix: String,
    ): List<Int> =
        names.map { name ->
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name + suffix))?.stateId
                ?: error("unknown block '${name + suffix}'")
        }
}
