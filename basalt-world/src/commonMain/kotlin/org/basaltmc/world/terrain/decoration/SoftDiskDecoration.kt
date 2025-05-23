package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/** Underwater `disk_sand`, `disk_clay`, `disk_gravel`, and mangrove `disk_grass`. */
public object SoftDiskDecoration {
    /** `addDefaultSoftDisks`: three sand attempts, then one clay and one gravel attempt. */
    public val DEFAULT: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            repeat(3) { placeWaterDisk(chunk, random, SAND) }
            placeWaterDisk(chunk, random, CLAY)
            placeWaterDisk(chunk, random, GRAVEL)
        }

    /** `addSwampClayDisk`: one clay attempt. */
    public val SWAMP: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random -> placeWaterDisk(chunk, random, CLAY) }

    /** `addMangroveSwampDisks`: one grass attempt followed by one clay attempt. */
    public val MANGROVE: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeGrassDisk(chunk, random)
            placeWaterDisk(chunk, random, CLAY)
        }

    private fun placeWaterDisk(
        chunk: Chunk,
        random: WorldgenRandom,
        config: DiskConfig,
    ) {
        val x = chunk.chunkX * 16 + random.nextInt(16)
        val z = chunk.chunkZ * 16 + random.nextInt(16)
        val y = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.OCEAN_FLOOR)
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.hasWaterFluid(get(chunk, x, y, z))) return
        placeDisk(chunk, random, DecorationPos(x, y, z), config)
    }

    private fun placeGrassDisk(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val x = chunk.chunkX * 16 + random.nextInt(16)
        val z = chunk.chunkZ * 16 + random.nextInt(16)
        val y = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.OCEAN_FLOOR) - 1
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || get(chunk, x, y, z) != mudStateId) return
        placeDisk(chunk, random, DecorationPos(x, y, z), GRASS)
    }

    private fun placeDisk(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        config: DiskConfig,
    ) {
        val radius = config.minRadius + random.nextInt(config.maxRadius - config.minRadius + 1)
        val radiusSquared = radius * radius
        for (z in origin.z - radius..origin.z + radius) {
            for (x in origin.x - radius..origin.x + radius) {
                val dx = x - origin.x
                val dz = z - origin.z
                if (dx * dx + dz * dz > radiusSquared) continue
                for (y in origin.y + config.halfHeight downTo origin.y - config.halfHeight) {
                    val current = get(chunk, x, y, z)
                    if (current !in config.targets) continue
                    val replacement = config.state(chunk, x, y, z)
                    set(chunk, x, y, z, replacement)
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
        if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) chunk.getBlockStateId(x and 15, y, z and 15)
        else Blocks.AIR.stateId

    private fun set(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) chunk.setBlockStateId(x and 15, y, z and 15, stateId)
    }

    private data class DiskConfig(
        val minRadius: Int,
        val maxRadius: Int,
        val halfHeight: Int,
        val targets: Set<Int>,
        val state: (Chunk, Int, Int, Int) -> Int,
    )

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val sandStateId = stateId("sand")
    private val sandstoneStateId = stateId("sandstone")
    private val clayStateId = stateId("clay")
    private val gravelStateId = stateId("gravel")
    private val mudStateId = stateId("mud")

    private val SAND =
        DiskConfig(2, 6, 2, setOf(Blocks.DIRT.stateId, Blocks.GRASS_BLOCK.stateId)) { chunk, x, y, z ->
            if (get(chunk, x, y - 1, z) == Blocks.AIR.stateId) sandstoneStateId else sandStateId
        }
    private val CLAY =
        DiskConfig(2, 3, 1, setOf(Blocks.DIRT.stateId, clayStateId)) { _, _, _, _ -> clayStateId }
    private val GRAVEL =
        DiskConfig(2, 5, 2, setOf(Blocks.DIRT.stateId, Blocks.GRASS_BLOCK.stateId)) { _, _, _, _ -> gravelStateId }
    private val GRASS =
        DiskConfig(2, 6, 2, setOf(Blocks.DIRT.stateId, mudStateId)) { chunk, x, y, z ->
            val above = get(chunk, x, y + 1, z)
            if (!Chunk.HeightmapType.OCEAN_FLOOR.matches(above) && !Blocks.hasWaterFluid(above)) {
                Blocks.GRASS_BLOCK.stateId
            } else {
                Blocks.DIRT.stateId
            }
        }

}
