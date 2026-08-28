package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.ironBarsStateId
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.terrain.DensityChunkSampler
import org.basaltmc.server.world.terrain.EndNoiseRouter
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin

/** // parity: the End's SURFACE_STRUCTURES placed features. */
public object EndSurfaceStructuresDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        router: EndNoiseRouter,
        biomeSource: TheEndBiomeSource,
    ) {
        val sourceChunks = mutableMapOf<Pair<Int, Int>, Chunk>()
        for (sourceChunkX in chunk.chunkX - 1..chunk.chunkX + 1) {
            for (sourceChunkZ in chunk.chunkZ - 1..chunk.chunkZ + 1) {
                placeReturnGateway(chunk, sourceChunkX, sourceChunkZ, seed, router, biomeSource, sourceChunks)
            }
        }
        for (spike in spikesForSeed(seed)) {
            if (spike.intersects(chunk)) placeSpike(chunk, spike, seed)
        }
    }

    private fun placeReturnGateway(
        target: Chunk,
        sourceChunkX: Int,
        sourceChunkZ: Int,
        seed: Long,
        router: EndNoiseRouter,
        biomeSource: TheEndBiomeSource,
        sourceChunks: MutableMap<Pair<Int, Int>, Chunk>,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, sourceChunkX * 16, sourceChunkZ * 16)
        random.setFeatureSeed(decorationSeed, RETURN_GATEWAY_INDEX, SURFACE_STRUCTURES_STEP)
        if (random.nextFloat() >= 1.0f / RETURN_GATEWAY_RARITY) return

        val x = sourceChunkX * 16 + random.nextInt(16)
        val z = sourceChunkZ * 16 + random.nextInt(16)
        if (!intersectsTarget(target, x, z)) return
        val source =
            if (sourceChunkX == target.chunkX && sourceChunkZ == target.chunkZ) {
                target
            } else {
                sourceChunks.getOrPut(sourceChunkX to sourceChunkZ) {
                    generateRawSourceChunk(sourceChunkX, sourceChunkZ, seed, router, biomeSource)
                }
            }
        val surfaceY = source.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
        if (surfaceY <= source.verticalBounds.minY) return
        val y = surfaceY + 3 + random.nextInt(7)
        if (biomeSource.blockBiomeAt(x, y, z) != Biomes.END_HIGHLANDS) return
        placeGateway(target, x, y, z)
    }

    private fun generateRawSourceChunk(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
        router: EndNoiseRouter,
        biomeSource: TheEndBiomeSource,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ, ChunkHeight.END)
        val sampler = DensityChunkSampler(router.finalDensity, chunkX, chunkZ, 0, END_GENERATION_DEPTH, 8, 4)
        for (y in 0 until END_GENERATION_DEPTH) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    if (sampler.sample(x, y, z) > 0.0) chunk.setBlockStateId(x, y, z, Blocks.END_STONE.stateId)
                }
            }
        }
        EndRawDecoration.decorate(chunk, seed, biomeSource)
        return chunk
    }

    private fun placeGateway(
        chunk: Chunk,
        originX: Int,
        originY: Int,
        originZ: Int,
    ) {
        for (z in originZ - 1..originZ + 1) {
            for (y in originY - 2..originY + 2) {
                for (x in originX - 1..originX + 1) {
                    val sameX = x == originX
                    val sameY = y == originY
                    val sameZ = z == originZ
                    val end = abs(y - originY) == 2
                    val stateId =
                        when {
                            sameX && sameY && sameZ -> Blocks.END_GATEWAY.stateId
                            sameY -> Blocks.AIR.stateId
                            end && sameX && sameZ -> Blocks.BEDROCK.stateId
                            (sameX || sameZ) && !end -> Blocks.BEDROCK.stateId
                            else -> Blocks.AIR.stateId
                        }
                    setBlock(chunk, x, y, z, stateId)
                }
            }
        }
        if (originX.floorDiv(16) == chunk.chunkX && originZ.floorDiv(16) == chunk.chunkZ) {
            val data =
                NbtCompound().apply {
                    putString("id", "minecraft:end_gateway")
                    putLong("Age", 0L)
                    putIntArray("exit_portal", intArrayOf(100, 50, 0))
                    putBoolean("ExactTeleport", true)
                    putCompound("components", NbtCompound())
                }
            chunk.setBlockEntity(
                ChunkBlockEntity(originX and 15, originY, originZ and 15, Identifier.vanilla("end_gateway"), data),
            )
        }
    }

    private fun intersectsTarget(
        chunk: Chunk,
        x: Int,
        z: Int,
    ): Boolean {
        val minX = chunk.chunkX * 16
        val minZ = chunk.chunkZ * 16
        return x + 1 >= minX && x - 1 <= minX + 15 && z + 1 >= minZ && z - 1 <= minZ + 15
    }

    private fun setBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (x.floorDiv(16) != chunk.chunkX || z.floorDiv(16) != chunk.chunkZ) return
        if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
        chunk.setBlockStateId(x and 15, y, z and 15, stateId)
    }

    /** parity: `end_spike`, which places every spike of this seed that reaches into the chunk. */
    public fun placeSpikesIn(
        chunk: Chunk,
        seed: Long,
    ): Boolean {
        var placed = false
        for (spike in spikesForSeed(seed)) {
            if (!spike.intersects(chunk)) continue
            placeSpike(chunk, spike, seed)
            placed = true
        }
        return placed
    }

    private fun placeSpike(
        chunk: Chunk,
        spike: EndSpike,
        seed: Long,
    ) {
        val minX = maxOf(chunk.chunkX * 16, spike.centerX - spike.radius)
        val maxX = minOf(chunk.chunkX * 16 + 15, spike.centerX + spike.radius)
        val minZ = maxOf(chunk.chunkZ * 16, spike.centerZ - spike.radius)
        val maxZ = minOf(chunk.chunkZ * 16 + 15, spike.centerZ + spike.radius)
        for (z in minZ..maxZ) {
            for (x in minX..maxX) {
                val dx = x - spike.centerX
                val dz = z - spike.centerZ
                val insidePillar = dx * dx + dz * dz <= spike.radius * spike.radius + 1
                for (y in chunk.verticalBounds.minY..spike.height + 10) {
                    when {
                        insidePillar && y < spike.height -> setBlock(chunk, x, y, z, Blocks.OBSIDIAN.stateId)
                        y > 65 -> setBlock(chunk, x, y, z, Blocks.AIR.stateId)
                    }
                }
            }
        }
        if (spike.guarded) placeCage(chunk, spike)
        placeCrystal(chunk, spike, seed)
    }

    private fun placeCage(
        chunk: Chunk,
        spike: EndSpike,
    ) {
        for (dx in -2..2) {
            for (dz in -2..2) {
                for (dy in 0..3) {
                    val isXSide = abs(dx) == 2
                    val isZSide = abs(dz) == 2
                    val top = dy == 3
                    if (!isXSide && !isZSide && !top) continue
                    val xEdge = dx == -2 || dx == 2 || top
                    val zEdge = dz == -2 || dz == 2 || top
                    val stateId =
                        Blocks.ironBarsStateId(
                            north = xEdge && dz != -2,
                            east = zEdge && dx != 2,
                            south = xEdge && dz != 2,
                            west = zEdge && dx != -2,
                        )
                    setBlock(chunk, spike.centerX + dx, spike.height + dy, spike.centerZ + dz, stateId)
                }
            }
        }
    }

    private fun placeCrystal(
        chunk: Chunk,
        spike: EndSpike,
        seed: Long,
    ) {
        if (spike.centerX.floorDiv(16) != chunk.chunkX || spike.centerZ.floorDiv(16) != chunk.chunkZ) return
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed =
            random.setDecorationSeed(
                seed,
                spike.centerX.floorDiv(16) * 16,
                spike.centerZ.floorDiv(16) * 16,
            )
        random.setFeatureSeed(decorationSeed, END_SPIKE_INDEX, SURFACE_STRUCTURES_STEP)
        chunk.addGeneratedEntity(
            ChunkGeneratedEntity(
                type = Identifier.vanilla("end_crystal"),
                x = spike.centerX + 0.5,
                y = spike.height + 1.0,
                z = spike.centerZ + 0.5,
                yaw = random.nextFloat() * 360.0f,
                pitch = 0.0f,
            ),
        )
        setBlock(chunk, spike.centerX, spike.height, spike.centerZ, Blocks.BEDROCK.stateId)
        setBlock(chunk, spike.centerX, spike.height + 1, spike.centerZ, Blocks.FIRE.stateId)
    }

    internal fun spikesForSeed(seed: Long): List<EndSpike> {
        val key = LegacyRandom(seed).nextLong() and 65535L
        val sizes = (0 until SPIKE_COUNT).toMutableList()
        val random = LegacyRandom(key)
        for (size in SPIKE_COUNT downTo 2) {
            val swapTo = random.nextInt(size)
            val last = sizes[size - 1]
            sizes[size - 1] = sizes[swapTo]
            sizes[swapTo] = last
        }
        return List(SPIKE_COUNT) { index ->
            val angle = 2.0 * (-kotlin.math.PI + kotlin.math.PI / SPIKE_COUNT * index)
            val size = sizes[index]
            EndSpike(
                centerX = floor(SPIKE_DISTANCE * cos(angle)).toInt(),
                centerZ = floor(SPIKE_DISTANCE * sin(angle)).toInt(),
                radius = 2 + size / 3,
                height = 76 + size * 3,
                guarded = size == 1 || size == 2,
            )
        }
    }

    internal data class EndSpike(
        val centerX: Int,
        val centerZ: Int,
        val radius: Int,
        val height: Int,
        val guarded: Boolean,
    ) {
        fun intersects(chunk: Chunk): Boolean {
            val minX = chunk.chunkX * 16
            val minZ = chunk.chunkZ * 16
            return centerX + radius >= minX &&
                centerX - radius <= minX + 15 &&
                centerZ + radius >= minZ &&
                centerZ - radius <= minZ + 15
        }
    }

    private const val END_GENERATION_DEPTH = 128
    private const val SPIKE_COUNT = 10
    private const val SPIKE_DISTANCE = 42.0
    private const val RETURN_GATEWAY_RARITY = 700
    private const val SURFACE_STRUCTURES_STEP = 4
    private const val RETURN_GATEWAY_INDEX = 0
    private const val END_SPIKE_INDEX = 1
}
