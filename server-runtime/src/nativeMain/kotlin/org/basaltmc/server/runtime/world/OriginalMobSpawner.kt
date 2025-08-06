package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.runtime.registry.VanillaBiomes
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * Passive packs created by the chunk-generation SPAWN status.
 *
 * parity: `NaturalSpawner.spawnMobsForChunkGeneration`: weighted CREATURE packs, the biome's own
 * probability, four position attempts per member and a position clamped inside the generated chunk.
 */
public object OriginalMobSpawner {
    public fun spawnInto(
        chunk: Chunk,
        entries: List<SpawnerEntry>,
        creatureProbability: Float,
        random: BasaltRandom,
        hasCeiling: Boolean = false,
    ): Int {
        if (entries.isEmpty()) return 0
        val minX = chunk.chunkX shl 4
        val minZ = chunk.chunkZ shl 4
        var added = 0
        while (random.nextFloat() < creatureProbability) {
            val entry = NaturalSpawner.pick(entries, random) ?: break
            val count = entry.minCount + random.nextInt(1 + entry.maxCount - entry.minCount)
            var x = minX + random.nextInt(16)
            var z = minZ + random.nextInt(16)
            val startX = x
            val startZ = z
            var groupSize = 0
            val group = MobSpawnGroupData()
            repeat(count) {
                var success = false
                var attempts = 0
                while (!success && attempts < POSITION_ATTEMPTS) {
                    attempts++
                    val y = topPosition(chunk, entry.type.path, x, z, hasCeiling)
                    val width = EntityTypeShapes.widthOf(entry.type)
                    val entityX = x.toDouble().coerceIn(minX + width, minX + 16.0 - width)
                    val entityZ = z.toDouble().coerceIn(minZ + width, minZ + 16.0 - width)
                    val blockX = entityX.toInt()
                    val blockZ = entityZ.toInt()
                    if (canSpawnAt(chunk, entry.type.path, blockX, y, blockZ)) {
                        val yaw = random.nextFloat() * 360f
                        val data = NbtCompound()
                        MobSpawnFinalization.writeGenerated(
                            data,
                            entry.type,
                            groupSize,
                            random,
                            group = group,
                            biome = biomeAt(chunk, blockX, y, blockZ),
                        )
                        chunk.addGeneratedEntity(
                            ChunkGeneratedEntity(
                                type = entry.type,
                                x = entityX,
                                y = y.toDouble(),
                                z = entityZ,
                                yaw = yaw,
                                pitch = 0f,
                                data = data,
                            ),
                        )
                        added++
                        groupSize++
                        success = true
                    }
                    x += random.nextInt(PACK_SPREAD) - random.nextInt(PACK_SPREAD)
                    z += random.nextInt(PACK_SPREAD) - random.nextInt(PACK_SPREAD)
                    while (x !in minX until minX + 16 || z !in minZ until minZ + 16) {
                        x = startX + random.nextInt(PACK_SPREAD) - random.nextInt(PACK_SPREAD)
                        z = startZ + random.nextInt(PACK_SPREAD) - random.nextInt(PACK_SPREAD)
                    }
                }
            }
        }
        return added
    }

    private fun topPosition(
        chunk: Chunk,
        type: String,
        worldX: Int,
        worldZ: Int,
        hasCeiling: Boolean,
    ): Int {
        val heightmap =
            if (type == "ocelot" || type == "parrot") {
                Chunk.HeightmapType.MOTION_BLOCKING
            } else {
                Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES
            }
        var y = chunk.heightAt(worldX and 15, worldZ and 15, heightmap)
        if (!hasCeiling) return y
        val localX = worldX and 15
        val localZ = worldZ and 15
        do {
            y--
        } while (y > chunk.verticalBounds.minY && !isAir(chunk.getBlockStateId(localX, y, localZ)))
        do {
            y--
        } while (y > chunk.verticalBounds.minY && isAir(chunk.getBlockStateId(localX, y, localZ)))
        return y
    }

    private fun canSpawnAt(
        chunk: Chunk,
        type: String,
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Boolean {
        if (y <= chunk.verticalBounds.minY || y >= chunk.verticalBounds.maxY) return false
        if (type == "strider") return canSpawnStrider(chunk, worldX, y, worldZ)
        val ground = chunk.getBlockStateId(worldX and 15, y - 1, worldZ and 15)
        if (!BlockShapes.isFaceSturdy(ground, BlockFace.UP)) return false
        val feet = chunk.getBlockStateId(worldX and 15, y, worldZ and 15)
        val head = chunk.getBlockStateId(worldX and 15, y + 1, worldZ and 15)
        if (!BlockShapes.isEmptyShape(feet) || !BlockShapes.isEmptyShape(head)) return false
        val groundPath = BlockStateDefinitions.blockOf(ground)?.path
        return when (type) {
            in GENERAL_ANIMALS -> groundPath == "grass_block"
            "mooshroom" -> groundPath == "mycelium"
            else -> true
        }
    }

    private fun canSpawnStrider(
        chunk: Chunk,
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Boolean {
        val localX = worldX and 15
        val localZ = worldZ and 15
        if (!isLava(chunk.getBlockStateId(localX, y, localZ))) return false
        var aboveY = y + 1
        while (aboveY <= chunk.verticalBounds.maxY) {
            val above = chunk.getBlockStateId(localX, aboveY, localZ)
            if (!isLava(above)) return isAir(above)
            aboveY++
        }
        return false
    }

    private fun isAir(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "air"

    private fun isLava(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "lava"

    private fun biomeAt(
        chunk: Chunk,
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Identifier? {
        val quartY = ((y - chunk.verticalBounds.minY) shr 2).coerceIn(0, chunk.verticalBounds.height / 4 - 1)
        val biomeId = chunk.getBiomeId((worldX and 15) shr 2, quartY, (worldZ and 15) shr 2)
        return VanillaBiomes.ALL
            .getOrNull(biomeId)
            ?.path
            ?.let(Identifier::vanilla)
    }

    private const val POSITION_ATTEMPTS = 4
    private const val PACK_SPREAD = 5

    private val GENERAL_ANIMALS = setOf("cat", "chicken", "cow", "donkey", "horse", "mule", "pig", "sheep")
}
