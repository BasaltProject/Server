package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/** // parity: the shared jungle `vines` placed and configured feature. */
public object JungleVineDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in listOf(CountPlacement(127), InSquarePlacement, UniformHeightRangePlacement(64, 100))) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { place(chunk, it) }
    }

    private fun place(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean {
        val originState = state(chunk, origin) ?: return false
        if (!Blocks.isAir(originState)) return false
        for (direction in DIRECTIONS) {
            val neighbour = state(chunk, origin.relative(direction)) ?: continue
            if (!Chunk.HeightmapType.OCEAN_FLOOR.matches(neighbour)) continue
            chunk.setBlockStateId(origin.x and 15, origin.y, origin.z and 15, defaultVineStateId - direction.falseBit)
            return true
        }
        return false
    }

    private fun state(
        chunk: Chunk,
        position: DecorationPos,
    ): Int? =
        if (position.y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
        } else {
            null
        }

    private fun DecorationPos.relative(direction: Direction): DecorationPos =
        DecorationPos(x + direction.dx, y + direction.dy, z + direction.dz)

    private enum class Direction(
        val dx: Int,
        val dy: Int,
        val dz: Int,
        val falseBit: Int,
    ) {
        UP(0, 1, 0, 2),
        NORTH(0, 0, -1, 8),
        SOUTH(0, 0, 1, 4),
        WEST(-1, 0, 0, 1),
        EAST(1, 0, 0, 16),
    }

    private val DIRECTIONS = Direction.entries
    private val defaultVineStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("vine"))?.stateId ?: error("unknown block 'vine'")
}
