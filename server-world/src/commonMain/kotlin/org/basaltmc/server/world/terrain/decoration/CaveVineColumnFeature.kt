package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier

/** Exact configured `cave_vine` and shorter `cave_vine_in_moss` block columns. */
internal object CaveVineColumnFeature {
    fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        inMoss: Boolean = false,
    ): Boolean {
        var bodyHeight = if (inMoss) sampleMossHeight(random) else sampleCaveHeight(random)
        var tipHeight = 1
        val totalHeight = bodyHeight + tipHeight

        var validHeight = totalHeight
        var next = origin.copy(y = origin.y - 1)
        for (distance in 0 until totalHeight) {
            if (next.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(blockAt(chunk, next))) {
                validHeight = distance
                break
            }
            next = next.copy(y = next.y - 1)
        }

        // prioritize_tip=true removes body first and retains the final cave-vine tip.
        var toRemove = totalHeight - validHeight
        val bodyRemoved = minOf(bodyHeight, toRemove)
        bodyHeight -= bodyRemoved
        toRemove -= bodyRemoved
        tipHeight -= minOf(tipHeight, toRemove)

        var position = origin
        repeat(bodyHeight) {
            chunk.setDecorationBlockStateId(position.x, position.y, position.z, caveVinePlantState(random))
            position = position.copy(y = position.y - 1)
        }
        repeat(tipHeight) {
            chunk.setDecorationBlockStateId(position.x, position.y, position.z, caveVineTipState(random))
            position = position.copy(y = position.y - 1)
        }
        return bodyHeight + tipHeight > 0
    }

    private fun sampleCaveHeight(random: WorldgenRandom): Int =
        when (random.nextInt(15)) {
            0, 1 -> random.nextInt(20)
            in 2..4 -> random.nextInt(3)
            else -> random.nextInt(7)
        }

    private fun sampleMossHeight(random: WorldgenRandom): Int =
        if (random.nextInt(6) < 5) random.nextInt(4) else 1 + random.nextInt(7)

    private fun caveVinePlantState(random: WorldgenRandom): Int =
        if (random.nextInt(5) < 4) CAVE_VINES_PLANT_NO_BERRIES else CAVE_VINES_PLANT_BERRIES

    private fun caveVineTipState(random: WorldgenRandom): Int {
        val berries = random.nextInt(5) >= 4
        val age = 23 + random.nextInt(3)
        return Blocks.stateIdOf(
            Identifier.vanilla("cave_vines"),
            mapOf("age" to age.toString(), "berries" to berries.toString()),
        ) ?: error("unknown cave_vines state age=$age berries=$berries")
    }

    private fun blockAt(
        chunk: Chunk,
        position: DecorationPos,
    ): Int = chunk.getDecorationBlockStateId(position.x, position.y, position.z)

    private fun state(
        path: String,
        properties: Map<String, String>,
    ): Int = Blocks.stateIdOf(Identifier.vanilla(path), properties) ?: error("unknown block state '$path$properties'")

    private val CAVE_VINES_PLANT_NO_BERRIES = state("cave_vines_plant", mapOf("berries" to "false"))
    private val CAVE_VINES_PLANT_BERRIES = state("cave_vines_plant", mapOf("berries" to "true"))

    internal fun placeForTest(chunk: Chunk, random: WorldgenRandom, origin: DecorationPos, inMoss: Boolean): Boolean =
        place(chunk, random, origin, inMoss)

}
