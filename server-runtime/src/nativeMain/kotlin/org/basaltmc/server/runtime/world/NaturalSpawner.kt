package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/** A mob the spawner decided to make: what, where, and facing which way. */
public class SpawnRequest(
    public val type: Identifier,
    public val x: Double,
    public val y: Double,
    public val z: Double,
    public val yaw: Float,
)

/**
 * What the world puts in an empty chunk while nobody is looking.
 *
 * parity: `NaturalSpawner`. One position per chunk per tick, a pack of one to four around it of one
 * kind chosen by weight, and caps per category that scale with the chunks being simulated.
 */
public object NaturalSpawner {
    /** parity: `NaturalSpawner`'s own 289, the chunks a cap is measured against. */
    private const val CAP_CHUNKS = 289

    /** parity: the 24 blocks a mob is never spawned within, squared. */
    private const val MIN_PLAYER_DISTANCE_SQUARED = 576.0

    /** parity: `spawnCategoryForPosition`'s three tries and its six blocks of spread. */
    private const val ATTEMPTS = 3
    private const val SPREAD = 6

    /** parity: `Animal.isBrightEnoughToSpawn`, light above eight. */
    private const val ANIMAL_LIGHT = 8

    /**
     * How many of [category] this world may hold, given how many chunks are being simulated.
     *
     * parity: `NaturalSpawner.SpawnState.canSpawnForCategory`, `max * chunks / 289`.
     */
    public fun capFor(
        category: MobCategory,
        simulatedChunks: Int,
    ): Int = category.maxPerChunk * simulatedChunks / CAP_CHUNKS

    /**
     * One chunk's worth of spawning. Answers what to spawn, which the caller puts in the world:
     * this decides, it does not create.
     */
    public fun spawnForChunk(
        world: LoadedWorld,
        chunkX: Int,
        chunkZ: Int,
        category: MobCategory,
        entriesFor: (Identifier) -> List<SpawnerEntry>,
        playerDistanceSquared: (Double, Double, Double) -> Double?,
        random: BasaltRandom,
    ): List<SpawnRequest> {
        val chunk = world.state.chunkIfLoaded(chunkX, chunkZ) ?: return emptyList()
        val localX = random.nextInt(16)
        val localZ = random.nextInt(16)
        val height = chunk.heightAt(localX, localZ, Chunk.HeightmapType.MOTION_BLOCKING)
        if (height <= chunk.verticalBounds.minY) return emptyList()
        val startY = random.nextInt(height + 1)
        val startX = (chunkX shl 4) + localX
        val startZ = (chunkZ shl 4) + localZ

        // parity: a position inside a block that conducts redstone is refused outright.
        val at = world.state.getBlockState(startX, startY, startZ) ?: return emptyList()
        if (BlockShapes.isFullCube(at)) return emptyList()

        val requests = mutableListOf<SpawnRequest>()
        for (attempt in 0 until ATTEMPTS) {
            var x = startX
            var z = startZ
            var chosen: SpawnerEntry? = null
            var pack = 1 + random.nextInt(4)
            var spawned = 0
            for (index in 0 until pack) {
                x += random.nextInt(SPREAD) - random.nextInt(SPREAD)
                z += random.nextInt(SPREAD) - random.nextInt(SPREAD)
                val centreX = x + 0.5
                val centreZ = z + 0.5
                val distance = playerDistanceSquared(centreX, startY.toDouble(), centreZ) ?: continue
                if (distance <= MIN_PLAYER_DISTANCE_SQUARED) continue
                if (chosen == null) {
                    val biome = biomeAt(world, x, startY, z) ?: return requests
                    chosen = pick(entriesFor(biome), random) ?: return requests
                    pack = chosen.minCount + random.nextInt(1 + chosen.maxCount - chosen.minCount)
                }
                if (!canSpawnAt(world, category, x, startY, z, random)) continue
                requests +=
                    SpawnRequest(chosen.type, centreX, startY.toDouble(), centreZ, random.nextFloat() * 360f)
                spawned++
                if (spawned >= pack) break
            }
            if (spawned > 0) break
        }
        return requests
    }

    /**
     * parity: `SpawnPlacementTypes.ON_GROUND` plus each category's own light rule: a monster needs
     * the dark, an animal needs light above eight and grass or sand to stand on.
     */
    public fun canSpawnAt(
        world: LoadedWorld,
        category: MobCategory,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean {
        val ground = world.state.getBlockState(x, y - 1, z) ?: return false
        if (!BlockShapes.isFaceSturdy(ground, org.basaltmc.server.registry.BlockFace.UP)) return false
        val feet = world.state.getBlockState(x, y, z) ?: return false
        val head = world.state.getBlockState(x, y + 1, z) ?: return false
        if (!BlockShapes.isEmptyShape(feet) || !BlockShapes.isEmptyShape(head)) return false

        return when (category) {
            // parity: `Monster.isDarkEnoughToSpawn`: the sky has to be dark where it stands, and the
            // block light under the dimension's own limit, which is zero in the overworld.
            MobCategory.MONSTER ->
                WorldLight.skyLight(world, x, y, z) <= random.nextInt(32) &&
                    WorldLight.blockLight(world, x, y, z) <= 0
            MobCategory.CREATURE ->
                isAnimalGround(ground) &&
                    maxOf(WorldLight.skyLight(world, x, y, z), WorldLight.blockLight(world, x, y, z)) > ANIMAL_LIGHT
            else -> true
        }
    }

    /** parity: `BlockTags.ANIMALS_SPAWNABLE_ON`, which is grass and the few things beside it. */
    private fun isAnimalGround(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path in ANIMAL_GROUND

    /** parity: `WeightedRandomList.getRandom` over the biome's own weights. */
    public fun pick(
        entries: List<SpawnerEntry>,
        random: BasaltRandom,
    ): SpawnerEntry? {
        val total = entries.sumOf { it.weight }
        if (total <= 0) return null
        var roll = random.nextInt(total)
        for (entry in entries) {
            roll -= entry.weight
            if (roll < 0) return entry
        }
        return entries.lastOrNull()
    }

    /** parity: the biome at the position, which is what decides the list to pick from. */
    private fun biomeAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Identifier? = world.biomeSource?.biomeAt(x, y, z)?.id

    private val ANIMAL_GROUND =
        setOf("grass_block", "podzol", "mycelium", "moss_block", "pale_moss_block", "mud", "muddy_mangrove_roots")
}
