package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.registry.isLeavesState
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.math.ceil

/** A mob the spawner decided to make: what, where, and facing which way. */
public class SpawnRequest(
    public val type: Identifier,
    public val x: Double,
    public val y: Double,
    public val z: Double,
    public val yaw: Float,
    /** Accepted members already finalized in this one pack; zero means the pack's first mob. */
    public val groupIndex: Int = 0,
    /** Concrete `SpawnGroupData`, shared only by accepted members of this outer pack attempt. */
    public val group: MobSpawnGroupData = MobSpawnGroupData(),
    /** Biome at this member's actual block position, used by variant spawn rules. */
    public val biome: Identifier? = null,
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

    /** parity: ServerChunkCache's interval for persistent CREATURE spawning. */
    private const val PERSISTENT_SPAWN_INTERVAL = 400L

    /**
     * How many of [category] this world may hold, given how many chunks are being simulated.
     *
     * parity: `NaturalSpawner.SpawnState.canSpawnForCategory`, `max * chunks / 289`.
     */
    public fun capFor(
        category: MobCategory,
        simulatedChunks: Int,
    ): Int = category.maxPerChunk * simulatedChunks / CAP_CHUNKS

    /** parity: `getFilteredSpawningCategories`, including all four aquatic categories. */
    public fun categoriesFor(
        difficulty: String,
        gameTime: Long,
    ): List<MobCategory> =
        MobCategory.entries.filter {
            it != MobCategory.MISC &&
                (difficulty != "peaceful" || it.friendly) &&
                (!it.persistent || gameTime % PERSISTENT_SPAWN_INTERVAL == 0L)
        }

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
        /** Runs at the exact attempt, so rejected plugin/cost spawns do not consume the pack. */
        requestAccepted: ((SpawnRequest) -> Boolean)? = null,
    ): List<SpawnRequest> {
        val chunk = world.state.chunkIfLoaded(chunkX, chunkZ) ?: return emptyList()
        val localX = random.nextInt(16)
        val localZ = random.nextInt(16)
        val topEmptyY = spawnTopY(chunk, localX, localZ)
        if (topEmptyY <= chunk.verticalBounds.minY) return emptyList()
        // parity: `getRandomPosWithin` picks between the world's floor and the surface, and the floor
        // is below zero: taking the height as a bound crashed on every world whose surface is
        // negative, which is every superflat one.
        val minY = chunk.verticalBounds.minY
        val startY = minY + random.nextInt(topEmptyY - minY + 1)
        val startX = (chunkX shl 4) + localX
        val startZ = (chunkZ shl 4) + localZ

        // parity: a position inside a block that conducts redstone is refused outright.
        val at = world.state.getBlockState(startX, startY, startZ) ?: return emptyList()
        if (BlockShapes.isFullCube(at)) return emptyList()

        val requests = mutableListOf<SpawnRequest>()
        var clusterSize = 0
        for (attempt in 0 until ATTEMPTS) {
            val group = MobSpawnGroupData()
            var x = startX
            var z = startZ
            var chosen: SpawnerEntry? = null
            var pack = ceil(random.nextFloat() * 4f).toInt()
            var spawned = 0
            var index = 0
            while (index < pack) {
                index++
                x += random.nextInt(SPREAD) - random.nextInt(SPREAD)
                z += random.nextInt(SPREAD) - random.nextInt(SPREAD)
                val centreX = x + 0.5
                val centreZ = z + 0.5
                val distance = playerDistanceSquared(centreX, startY.toDouble(), centreZ) ?: continue
                if (distance <= MIN_PLAYER_DISTANCE_SQUARED) continue
                val spawn = world.spawn
                val spawnDx = spawn.first - centreX
                val spawnDy = spawn.second - startY
                val spawnDz = spawn.third - centreZ
                if (spawnDx * spawnDx + spawnDy * spawnDy + spawnDz * spawnDz <= MIN_PLAYER_DISTANCE_SQUARED) continue
                if (chosen == null) {
                    val biome = biomeAt(world, x, startY, z) ?: return requests
                    chosen = pick(entriesFor(biome), random) ?: return requests
                    pack = chosen.minCount + random.nextInt((1 + chosen.maxCount - chosen.minCount).coerceAtLeast(1))
                }
                if (!canSpawnAt(world, chosen.type, category, x, startY, z, random)) continue
                val request =
                    SpawnRequest(
                        chosen.type,
                        centreX,
                        startY.toDouble(),
                        centreZ,
                        random.nextFloat() * 360f,
                        groupIndex = spawned,
                        group = group,
                        biome = biomeAt(world, x, startY, z),
                    )
                if (requestAccepted != null && !requestAccepted(request)) continue
                requests += request
                spawned++
                clusterSize++
                if (clusterSize >= maxClusterSize(chosen.type)) return requests
                if (spawned >= pack) break
            }
        }
        return requests
    }

    /** parity: `getRandomPosWithin` samples through WORLD_SURFACE plus one extra block. */
    internal fun spawnTopY(
        chunk: Chunk,
        localX: Int,
        localZ: Int,
    ): Int = chunk.heightAt(localX, localZ, Chunk.HeightmapType.WORLD_SURFACE) + 1

    /** parity: mob overrides of `getMaxSpawnClusterSize`, default four. */
    internal fun maxClusterSize(type: Identifier): Int =
        when (type.path) {
            "ghast", "happy_ghast", "pillager" -> 1
            "horse", "donkey", "mule", "skeleton_horse", "zombie_horse" -> 6
            "cod", "pufferfish", "tropical_fish", "wolf" -> 8
            "salmon" -> 5
            else -> 4
        }

    /**
     * parity: `SpawnPlacementTypes.ON_GROUND` plus each category's own light rule: a monster needs
     * the dark, while the selected type decides ground, water or lava placement.
     */
    public fun canSpawnAt(
        world: LoadedWorld,
        category: MobCategory,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean = canSpawnAt(world, type = null, category, x, y, z, random)

    /**
     * The entity type matters independently of its cap category: drowned are monsters that spawn
     * in water, and striders are creatures that spawn in lava.
     */
    public fun canSpawnAt(
        world: LoadedWorld,
        type: Identifier?,
        category: MobCategory,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean {
        val typePath = type?.path
        if (typePath == "strider") return canSpawnInLava(world, x, y, z)
        if (typePath in WATER_TYPES) return canSpawnInWater(world, typePath, x, y, z, random)

        val ground = world.state.getBlockState(x, y - 1, z) ?: return false
        if (!BlockShapes.isFaceSturdy(ground, org.basaltmc.server.registry.BlockFace.UP)) return false
        val feet = world.state.getBlockState(x, y, z) ?: return false
        val head = world.state.getBlockState(x, y + 1, z) ?: return false
        if (!canStandIn(feet, typePath) || !canStandIn(head, typePath)) return false
        if (typePath == "ocelot") {
            return y >= world.declaration.dimension.seaLevel &&
                (BlockStateDefinitions.blockOf(ground)?.path == "grass_block" || Blocks.isLeavesState(ground)) &&
                random.nextInt(3) != 0
        }
        if (typePath == "bat") return canSpawnBat(world, ground, x, y, z, random)
        if (typePath == "slime") return canSpawnSlime(world, x, y, z, random)

        return when (category) {
            // parity: `Monster.isDarkEnoughToSpawn`: the sky has to be dark where it stands, and the
            // block light under the dimension's own limit, which is zero in the overworld.
            MobCategory.MONSTER -> {
                val dark =
                    typePath in MONSTERS_IGNORING_LIGHT ||
                        (
                            WorldLight.skyLight(world, x, y, z) <= random.nextInt(32) &&
                                WorldLight.blockLight(world, x, y, z) <= 0
                        )
                dark && monsterTypeRule(typePath, ground, world, x, y, z, random)
            }
            MobCategory.CREATURE ->
                animalGroundAllows(typePath, ground) &&
                    WorldLight.rawBrightness(world, x, y, z) > ANIMAL_LIGHT &&
                    (typePath != "turtle" || y < world.declaration.dimension.seaLevel + 4)
            else -> true
        }
    }

    // parity: NaturalSpawner.isValidEmptySpawnBlock, all four of its answers.
    private fun canStandIn(
        stateId: Int,
        typePath: String?,
    ): Boolean {
        if (BlockShapes.isFullCube(stateId)) return false
        if (Redstone.isSignalSource(stateId)) return false
        if (Blocks.hasFluid(stateId)) return false
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        // parity: `BlockTags.PREVENT_MOB_SPAWNING_INSIDE`, which is the rails and nothing else.
        if (path in RAILS) return false
        return !isDangerous(stateId, path, typePath)
    }

    /** parity: `EntityType.isBlockDangerous`, whose burning half a fire-immune mob walks through. */
    private fun isDangerous(
        stateId: Int,
        path: String,
        typePath: String?,
    ): Boolean {
        val burning =
            path in BURNING_BLOCKS ||
                (path == "campfire" && BlockStateDefinitions.propertiesOf(stateId)["lit"] == "true")
        val immune = typePath != null && EntityTypeShapes.isFireImmune(Identifier.vanilla(typePath))
        if (burning && !immune) return true
        return path in HURTS_ON_CONTACT
    }

    private fun monsterTypeRule(
        type: String?,
        ground: Int,
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean {
        val groundPath = BlockStateDefinitions.blockOf(ground)?.path
        return when (type) {
            "ghast" -> random.nextInt(20) == 0
            "hoglin", "piglin", "zombified_piglin" -> groundPath != "nether_wart_block"
            "husk", "parched", "stray" -> canSeeSky(world, x, y, z)
            else -> true
        }
    }

    private fun canSeeSky(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return false
        return y >= chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE)
    }

    private fun canSpawnBat(
        world: LoadedWorld,
        ground: Int,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean {
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return false
        if (y >= chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE)) return false
        if (random.nextBoolean()) return false
        if (WorldLight.rawBrightness(world, x, y, z) > random.nextInt(4)) return false
        return BlockStateDefinitions.blockOf(ground)?.path in BAT_GROUND
    }

    /** parity: 26.2 `Slime.checkSlimeSpawnRules`, including its moon timeline and slime chunks. */
    private fun canSpawnSlime(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean {
        if (world.difficulty == "peaceful") return false
        val surfaceBiome = biomeAt(world, x, y, z)?.path in SURFACE_SLIME_BIOMES
        if (surfaceBiome && y > 50 && y < 70) {
            if (
                random.nextFloat() < surfaceSlimeChance(world.clock.totalTicks) &&
                WorldLight.rawBrightness(world, x, y, z, DaylightDetectors.skyDarken(world)) <= random.nextInt(8)
            ) {
                return true
            }
        }
        val slimeChunk = isSlimeChunk(world.metadata.seed, x shr 4, z shr 4)
        return random.nextInt(10) == 0 && slimeChunk && y < 40
    }

    /** parity: the constant eight-keyframe `timeline/moon` gameplay track. */
    internal fun surfaceSlimeChance(clockTicks: Long): Float {
        val wrapped = ((clockTicks % MOON_PERIOD_TICKS) + MOON_PERIOD_TICKS) % MOON_PERIOD_TICKS
        val phase = (wrapped / DAY_TICKS).toInt()
        return kotlin.math.abs(4 - phase) * 0.125f
    }

    /** parity: `WorldgenRandom.seedSlimeChunk`, including Java's intentional Int overflow. */
    internal fun isSlimeChunk(
        worldSeed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Boolean {
        val xSquare = chunkX * chunkX * 4_987_142
        val xLinear = chunkX * 5_947_611
        val zSquare = (chunkZ * chunkZ).toLong() * 4_392_871L
        val zLinear = chunkZ * 389_711
        val seed = (worldSeed + xSquare + xLinear + zSquare + zLinear) xor SLIME_CHUNK_SALT
        return LegacyRandom(seed).nextInt(10) == 0
    }

    /** parity: `SpawnPlacementTypes.IN_WATER` plus the common water-animal predicates. */
    private fun canSpawnInWater(
        world: LoadedWorld,
        type: String?,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean {
        val at = world.state.getBlockState(x, y, z) ?: return false
        val above = world.state.getBlockState(x, y + 1, z) ?: return false
        if (!isWaterFluid(at) || BlockShapes.isFullCube(above)) return false
        val below = world.state.getBlockState(x, y - 1, z) ?: return false
        val seaLevel = world.declaration.dimension.seaLevel
        return when (type) {
            "axolotl" -> BlockStateDefinitions.blockOf(below)?.path == "clay"
            "glow_squid" ->
                y <= seaLevel - 33 &&
                    maxOf(WorldLight.skyLight(world, x, y, z), WorldLight.blockLight(world, x, y, z)) == 0
            "nautilus" -> y in seaLevel - 25..seaLevel - 5 && isWaterFluid(below) && isWaterBlock(above)
            "cod", "dolphin", "pufferfish", "salmon", "squid" ->
                y in seaLevel - 13..seaLevel && isWaterFluid(below) && isWaterBlock(above)
            "tropical_fish" -> {
                val anyHeight = biomeAt(world, x, y, z)?.path == "lush_caves"
                isWaterFluid(below) && isWaterBlock(above) && (anyHeight || y in seaLevel - 13..seaLevel)
            }
            "drowned" -> {
                val dark =
                    WorldLight.skyLight(world, x, y, z) <= random.nextInt(32) &&
                        WorldLight.blockLight(world, x, y, z) <= 0
                val river = biomeAt(world, x, y, z)?.path?.contains("river") == true
                val frequencyAllows =
                    if (river) {
                        random.nextInt(15) == 0
                    } else {
                        y < seaLevel - 5 && random.nextInt(40) == 0
                    }
                dark && isWaterFluid(below) && frequencyAllows
            }
            else -> true
        }
    }

    /** parity: `SpawnPlacementTypes.IN_LAVA` and Strider's scan to the lava surface. */
    private fun canSpawnInLava(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val at = world.state.getBlockState(x, y, z) ?: return false
        if (!isLavaFluid(at)) return false
        var aboveY = y + 1
        while (aboveY <= world.declaration.dimension.chunkHeight.maxY) {
            val above = world.state.getBlockState(x, aboveY, z) ?: return false
            if (!isLavaFluid(above)) return BlockStateDefinitions.blockOf(above)?.path == "air"
            aboveY++
        }
        return false
    }

    /** parity: 26.2 `BlockTags.ANIMALS_SPAWNABLE_ON`. */
    private fun animalGroundAllows(
        type: String?,
        stateId: Int,
    ): Boolean {
        val ground = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        return when (type) {
            "armadillo" ->
                ground in ARMADILLO_GROUND ||
                    ("glazed" !in ground && (ground == "terracotta" || ground.endsWith("_terracotta")))
            "camel" -> ground in SAND_GROUND
            "fox" -> ground in FOX_WOLF_GROUND
            "frog" -> ground in FROG_GROUND
            "goat" -> ground in GOAT_GROUND
            "mooshroom" -> ground == "mycelium"
            "parrot" ->
                ground in PARROT_GROUND ||
                    ground.endsWith("_log") ||
                    ground.endsWith("_wood") ||
                    Blocks.isLeavesState(stateId)
            "polar_bear" -> ground == "grass_block" || ground == "ice"
            "rabbit" -> ground in RABBIT_GROUND
            "turtle" -> ground in SAND_GROUND
            "wolf" -> ground in FOX_WOLF_GROUND
            else -> ground == "grass_block"
        }
    }

    private fun isWaterBlock(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "water"

    private fun isWaterFluid(stateId: Int): Boolean =
        isWaterBlock(stateId) || BlockStateDefinitions.propertiesOf(stateId)["waterlogged"] == "true"

    private fun isLavaFluid(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "lava"

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
    internal fun biomeAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Identifier? {
        world.biomeZoomer
            .biomeAt(x, y, z) { quartX, quartY, quartZ ->
                noiseBiomeAt(world, quartX, quartY, quartZ)
            }?.let { return it }

        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4)
        if (chunk != null) {
            val clampedY = y.coerceIn(chunk.verticalBounds.minY, chunk.verticalBounds.maxY)
            val quartY = (clampedY - chunk.verticalBounds.minY) shr 2
            val biomeId = chunk.getBiomeId((x and 15) shr 2, quartY, (z and 15) shr 2)
            Biomes.REGISTRY.byId(biomeId)?.let { return it.id }
        }
        return world.biomeSource?.blockBiomeAt(x, y, z)?.id
    }

    private fun noiseBiomeAt(
        world: LoadedWorld,
        quartX: Int,
        quartY: Int,
        quartZ: Int,
    ): Identifier? {
        val chunk = world.state.chunkIfLoaded(quartX shr 2, quartZ shr 2)
        if (chunk != null) {
            val localQuartY =
                (quartY - (chunk.verticalBounds.minY shr 2))
                    .coerceIn(0, chunk.verticalBounds.height / 4 - 1)
            val biomeId = chunk.getBiomeId(quartX and 3, localQuartY, quartZ and 3)
            Biomes.REGISTRY.byId(biomeId)?.let { return it.id }
        }
        return world.biomeSource?.biomeAtQuart(quartX, quartY, quartZ)?.id
    }

    private val ARMADILLO_GROUND = setOf("grass_block", "red_sand", "coarse_dirt")
    private val SAND_GROUND = setOf("sand", "red_sand", "suspicious_sand")
    private val FOX_WOLF_GROUND = setOf("grass_block", "snow", "snow_block", "podzol", "coarse_dirt")
    private val FROG_GROUND = setOf("grass_block", "mud", "mangrove_roots", "muddy_mangrove_roots")
    private val GOAT_GROUND = setOf("grass_block", "stone", "snow", "snow_block", "packed_ice", "gravel")
    private val PARROT_GROUND = setOf("grass_block", "air")
    private val RABBIT_GROUND = setOf("grass_block", "snow", "snow_block", "sand")
    private val BAT_GROUND = setOf("stone", "granite", "diorite", "andesite", "tuff", "deepslate")

    /** parity: `BlockTags.RAILS`, the whole of `PREVENT_MOB_SPAWNING_INSIDE`. */
    private val RAILS = setOf("rail", "powered_rail", "detector_rail", "activator_rail")

    /** parity: `NodeEvaluator.isBurningBlock`, minus the lit campfire, which needs its state. */
    private val BURNING_BLOCKS = setOf("fire", "soul_fire", "lava", "magma_block", "lava_cauldron")

    /** parity: the rest of `EntityType.isBlockDangerous`, which no mob is immune to. */
    private val HURTS_ON_CONTACT = setOf("wither_rose", "sweet_berry_bush", "cactus", "powder_snow")

    private val WATER_TYPES =
        setOf(
            "axolotl",
            "cod",
            "dolphin",
            "drowned",
            "elder_guardian",
            "glow_squid",
            "guardian",
            "nautilus",
            "pufferfish",
            "salmon",
            "squid",
            "tropical_fish",
        )

    private val MONSTERS_IGNORING_LIGHT =
        setOf("blaze", "breeze", "ghast", "hoglin", "magma_cube", "piglin", "sulfur_cube", "zoglin", "zombified_piglin")

    private val SURFACE_SLIME_BIOMES = setOf("swamp", "mangrove_swamp")

    private const val DAY_TICKS = 24_000L
    private const val MOON_PERIOD_TICKS = DAY_TICKS * 8L
    private const val SLIME_CHUNK_SALT = 987_234_911L
}
