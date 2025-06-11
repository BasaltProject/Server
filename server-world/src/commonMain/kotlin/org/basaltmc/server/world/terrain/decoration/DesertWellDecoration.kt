package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/** // parity: `desert_well`, global index 2 of Overworld `SURFACE_STRUCTURES`. */
public object DesertWellDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, GLOBAL_INDEX, SURFACE_STRUCTURES_STEP)
        if (random.nextFloat() >= 1.0f / 1000.0f) return
        val localX = random.nextInt(16)
        val localZ = random.nextInt(16)
        val suppliedY = chunk.heightAt(localX, localZ, Chunk.HeightmapType.WORLD_SURFACE)
        val worldX = chunk.chunkX * 16 + localX
        val worldZ = chunk.chunkZ * 16 + localZ
        if (environment.biomeAt(worldX, suppliedY, worldZ).substringAfter(':') != "desert") return
        placeAt(chunk, random, localX, suppliedY, localZ)
    }

    /**
     * The feature itself, at one position: what the decoration pass calls per origin and what
     * `/place feature` calls once. Chunk-local x and z, as everything in this package uses.
     */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        originX: Int,
        suppliedY: Int,
        originZ: Int,
    ) {
        var originY = suppliedY + 1
        while (isAir(chunk, originX, originY, originZ) && originY > WorldHeight.MIN_Y + 2) originY--
        if (get(chunk, originX, originY, originZ) != sandStateId) return
        for (offsetX in -2..2) {
            for (offsetZ in -2..2) {
                if (isAir(chunk, originX + offsetX, originY - 1, originZ + offsetZ) &&
                    isAir(chunk, originX + offsetX, originY - 2, originZ + offsetZ)
                ) {
                    return
                }
            }
        }

        for (offsetY in -2..0) {
            for (offsetX in -2..2) {
                for (offsetZ in -2..2) {
                    set(
                        chunk,
                        originX + offsetX,
                        originY + offsetY,
                        originZ + offsetZ,
                        sandstoneStateId,
                    )
                }
            }
        }
        set(chunk, originX, originY, originZ, Blocks.WATER.stateId)
        for ((offsetX, offsetZ) in HORIZONTAL_DIRECTIONS) {
            set(chunk, originX + offsetX, originY, originZ + offsetZ, Blocks.WATER.stateId)
        }
        set(chunk, originX, originY - 1, originZ, sandStateId)
        for ((offsetX, offsetZ) in HORIZONTAL_DIRECTIONS) {
            set(chunk, originX + offsetX, originY - 1, originZ + offsetZ, sandStateId)
        }

        for (offsetX in -2..2) {
            for (offsetZ in -2..2) {
                if (offsetX == -2 || offsetX == 2 || offsetZ == -2 || offsetZ == 2) {
                    set(chunk, originX + offsetX, originY + 1, originZ + offsetZ, sandstoneStateId)
                }
            }
        }
        set(chunk, originX + 2, originY + 1, originZ, sandstoneSlabStateId)
        set(chunk, originX - 2, originY + 1, originZ, sandstoneSlabStateId)
        set(chunk, originX, originY + 1, originZ + 2, sandstoneSlabStateId)
        set(chunk, originX, originY + 1, originZ - 2, sandstoneSlabStateId)

        for (offsetX in -1..1) {
            for (offsetZ in -1..1) {
                set(
                    chunk,
                    originX + offsetX,
                    originY + 4,
                    originZ + offsetZ,
                    if (offsetX == 0 && offsetZ == 0) sandstoneStateId else sandstoneSlabStateId,
                )
            }
        }
        for (offsetY in 1..3) {
            set(chunk, originX - 1, originY + offsetY, originZ - 1, sandstoneStateId)
            set(chunk, originX - 1, originY + offsetY, originZ + 1, sandstoneStateId)
            set(chunk, originX + 1, originY + offsetY, originZ - 1, sandstoneStateId)
            set(chunk, originX + 1, originY + offsetY, originZ + 1, sandstoneStateId)
        }

        val waterOffsets = listOf(0 to 0, 1 to 0, 0 to 1, -1 to 0, 0 to -1)
        val shallow = waterOffsets[random.nextInt(waterOffsets.size)]
        placeSuspiciousSand(chunk, originX + shallow.first, originY - 1, originZ + shallow.second)
        val deep = waterOffsets[random.nextInt(waterOffsets.size)]
        placeSuspiciousSand(chunk, originX + deep.first, originY - 2, originZ + deep.second)
    }

    private fun placeSuspiciousSand(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (x !in 0 until 16 || z !in 0 until 16 || y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
        chunk.setBlockStateId(x, y, z, suspiciousSandStateId)
        val data = NbtCompound()
        data.putString("LootTable", "minecraft:archaeology/desert_well")
        val worldX = chunk.chunkX * 16 + x
        val worldZ = chunk.chunkZ * 16 + z
        data.putLong("LootTableSeed", packedBlockPosition(worldX, y, worldZ))
        chunk.setBlockEntity(ChunkBlockEntity(x, y, z, Identifier.vanilla("brushable_block"), data))
    }

    private fun packedBlockPosition(
        x: Int,
        y: Int,
        z: Int,
    ): Long =
        ((x.toLong() and 0x3ffffffL) shl 38) or
            ((z.toLong() and 0x3ffffffL) shl 12) or
            (y.toLong() and 0xfffL)

    private fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = Blocks.isAir(get(chunk, x, y, z))

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

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private const val SURFACE_STRUCTURES_STEP = 4
    private const val GLOBAL_INDEX = 2
    private val HORIZONTAL_DIRECTIONS = listOf(0 to -1, 1 to 0, 0 to 1, -1 to 0)
    private val sandStateId = stateId("sand")
    private val sandstoneStateId = stateId("sandstone")
    private val sandstoneSlabStateId = stateId("sandstone_slab")
    private val suspiciousSandStateId = stateId("suspicious_sand")
}
