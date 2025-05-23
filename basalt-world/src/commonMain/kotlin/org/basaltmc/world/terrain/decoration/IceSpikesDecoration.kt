package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import kotlin.math.abs
import kotlin.math.ceil

/** Ice-spikes biome `SURFACE_STRUCTURES`: three spikes followed by two packed-ice disks. */
public object IceSpikesDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        repeat(3) {
            val x = random.nextInt(16)
            val z = random.nextInt(16)
            val y = chunk.heightAt(x, z, Chunk.HeightmapType.WORLD_SURFACE)
            placeSpike(chunk, random, DecorationPos(x, y, z))
        }
        repeat(2) {
            val x = random.nextInt(16)
            val z = random.nextInt(16)
            val y = chunk.heightAt(x, z, Chunk.HeightmapType.WORLD_SURFACE) - 1
            if (get(chunk, x, y, z) == Blocks.SNOW_BLOCK.stateId) placeIcePatch(chunk, random, DecorationPos(x, y, z))
        }
    }

    private fun placeSpike(
        chunk: Chunk,
        random: WorldgenRandom,
        suppliedOrigin: DecorationPos,
    ) {
        var origin = suppliedOrigin
        while (get(chunk, origin.x, origin.y, origin.z) == Blocks.AIR.stateId && origin.y > WorldHeight.MIN_Y + 2) {
            origin = origin.copy(y = origin.y - 1)
        }
        if (get(chunk, origin.x, origin.y, origin.z) != Blocks.SNOW_BLOCK.stateId) return

        origin = origin.copy(y = origin.y + random.nextInt(4))
        val height = random.nextInt(4) + 7
        val width = height / 4 + random.nextInt(2)
        if (width > 1 && random.nextInt(60) == 0) origin = origin.copy(y = origin.y + 10 + random.nextInt(30))

        for (yOffset in 0 until height) {
            val scale = (1.0f - yOffset.toFloat() / height) * width
            val newWidth = ceil(scale).toInt()
            for (xOffset in -newWidth..newWidth) {
                val dx = abs(xOffset) - 0.25f
                for (zOffset in -newWidth..newWidth) {
                    val dz = abs(zOffset) - 0.25f
                    val inside = (xOffset == 0 && zOffset == 0) || dx * dx + dz * dz <= scale * scale
                    val edge =
                        xOffset == -newWidth ||
                            xOffset == newWidth ||
                            zOffset == -newWidth ||
                            zOffset == newWidth
                    if (inside && (!edge || random.nextFloat() <= 0.75f)) {
                        replaceSpikeBlock(chunk, origin.x + xOffset, origin.y + yOffset, origin.z + zOffset)
                        if (yOffset != 0 && newWidth > 1) {
                            replaceSpikeBlock(chunk, origin.x + xOffset, origin.y - yOffset, origin.z + zOffset)
                        }
                    }
                }
            }
        }

        val pillarWidth = (width - 1).coerceIn(0, 1)
        for (xOffset in -pillarWidth..pillarWidth) {
            for (zOffset in -pillarWidth..pillarWidth) {
                var cursorY = origin.y - 1
                var runLength = if (abs(xOffset) == 1 && abs(zOffset) == 1) random.nextInt(5) else 50
                while (cursorY > 50) {
                    val stateId = get(chunk, origin.x + xOffset, cursorY, origin.z + zOffset)
                    if (stateId != Blocks.AIR.stateId &&
                        stateId !in replaceableStateIds &&
                        stateId != Blocks.PACKED_ICE.stateId
                    ) {
                        break
                    }
                    set(chunk, origin.x + xOffset, cursorY, origin.z + zOffset, Blocks.PACKED_ICE.stateId)
                    cursorY--
                    runLength--
                    if (runLength <= 0) {
                        cursorY -= random.nextInt(5) + 1
                        runLength = random.nextInt(5)
                    }
                }
            }
        }
    }

    private fun replaceSpikeBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val stateId = get(chunk, x, y, z)
        if (stateId == Blocks.AIR.stateId || stateId in replaceableStateIds) {
            set(chunk, x, y, z, Blocks.PACKED_ICE.stateId)
        }
    }

    private fun placeIcePatch(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val radius = 2 + random.nextInt(2)
        val top = origin.y + 1
        val bottom = origin.y - 2
        for (x in origin.x - radius..origin.x + radius) {
            for (z in origin.z - radius..origin.z + radius) {
                val dx = x - origin.x
                val dz = z - origin.z
                if (dx * dx + dz * dz <= radius * radius) {
                    for (y in top downTo bottom + 1) {
                        if (get(chunk, x, y, z) in icePatchTargetStateIds) {
                            set(chunk, x, y, z, Blocks.PACKED_ICE.stateId)
                        }
                    }
                }
            }
        }
    }

    private fun get(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int =
        if (x in 0 until 16 && z in 0 until 16 && y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            chunk.getBlockStateId(x, y, z)
        } else {
            Blocks.AIR.stateId
        }

    private fun set(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (x in 0 until 16 && z in 0 until 16 && y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            chunk.setBlockStateId(x, y, z, stateId)
        }
    }

    private val replaceableStateIds: Set<Int> =
        stateIds(
            "dirt",
            "coarse_dirt",
            "podzol",
            "rooted_dirt",
            "grass_block",
            "mycelium",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "snow_block",
            "ice",
        )

    private val icePatchTargetStateIds: Set<Int> =
        stateIds("dirt", "grass_block", "podzol", "coarse_dirt", "mycelium", "snow_block", "ice")

    private fun stateIds(vararg names: String): Set<Int> =
        names.mapNotNull { Blocks.REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }.toSet()
}
