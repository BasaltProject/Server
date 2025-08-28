package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.light.BlockLight
import org.basaltmc.wire.common.Identifier
import kotlin.math.floor
import kotlin.random.Random

/** Minecraft 26.2's persisted periodic wandering-trader spawner. */
internal class WanderingTraderSpawner(
    initialSpawnDelay: Int = DEFAULT_SPAWN_DELAY,
    initialSpawnChance: Int = MIN_SPAWN_CHANCE,
    private val random: BasaltRandom = LegacyRandom(Random.nextLong()),
) {
    private var tickDelay = DEFAULT_TICK_DELAY
    private var spawnDelay = initialSpawnDelay
    private var spawnChance = initialSpawnChance

    fun tick(
        world: LoadedWorld,
        players: List<PlayerSession>,
        levelRandom: BasaltRandom,
        spawnTrader: (WanderingTraderSpawnRequest) -> Int?,
        spawnLlama: (TraderLlamaSpawnRequest, traderId: Int) -> Unit,
    ) {
        if (world.declaration.dimension != WorldDimension.OVERWORLD) return
        if (!world.gameRules[GameRuleRegistry.SPAWN_WANDERING_TRADERS]) return

        tickDelay--
        if (tickDelay > 0) return
        tickDelay = DEFAULT_TICK_DELAY
        spawnDelay -= DEFAULT_TICK_DELAY
        if (spawnDelay > 0) return

        spawnDelay = DEFAULT_SPAWN_DELAY
        val chanceToSpawn = spawnChance
        spawnChance = (chanceToSpawn + SPAWN_CHANCE_INCREASE).coerceIn(MIN_SPAWN_CHANCE, MAX_SPAWN_CHANCE)
        if (random.nextInt(PERCENT_RANGE) > chanceToSpawn) return
        if (spawn(world, players, levelRandom, spawnTrader, spawnLlama)) spawnChance = MIN_SPAWN_CHANCE
    }

    fun savedState(): WanderingTraderSpawnState = WanderingTraderSpawnState(spawnDelay, spawnChance)

    private fun spawn(
        world: LoadedWorld,
        players: List<PlayerSession>,
        levelRandom: BasaltRandom,
        spawnTrader: (WanderingTraderSpawnRequest) -> Int?,
        spawnLlama: (TraderLlamaSpawnRequest, traderId: Int) -> Unit,
    ): Boolean {
        val alive = players.filter { !it.dead && it.health > 0f }
        if (alive.isEmpty()) return true
        val player = alive[levelRandom.nextInt(alive.size)]
        if (random.nextInt(SPAWN_ONE_IN_X_CHANCE) != 0) return false

        val playerPos = BlockPosition(floor(player.x).toInt(), floor(player.y).toInt(), floor(player.z).toInt())
        val reference = findMeetingBell(world, playerPos, REFERENCE_RADIUS) ?: playerPos
        val traderPos = findSpawnPositionNear(world, reference, REFERENCE_RADIUS) ?: return false
        if (!hasEnoughSpace(world, traderPos)) return false
        if (NaturalSpawner.biomeAt(world, traderPos.x, traderPos.y, traderPos.z)?.path == WITHOUT_TRADER_BIOME) {
            return false
        }

        val traderId =
            spawnTrader(
                WanderingTraderSpawnRequest(
                    x = traderPos.x + 0.5,
                    y = traderPos.y.toDouble(),
                    z = traderPos.z + 0.5,
                    referenceX = reference.x,
                    referenceY = reference.y,
                    referenceZ = reference.z,
                ),
            ) ?: return false
        repeat(LLAMA_COUNT) {
            val llamaPos = findSpawnPositionNear(world, traderPos, LLAMA_RADIUS) ?: return@repeat
            spawnLlama(
                TraderLlamaSpawnRequest(llamaPos.x + 0.5, llamaPos.y.toDouble(), llamaPos.z + 0.5),
                traderId,
            )
        }
        return true
    }

    /** PoiManager.find(MEETING): first in chunk/section traversal, not the closest bell. */
    private fun findMeetingBell(
        world: LoadedWorld,
        center: BlockPosition,
        radius: Int,
    ): BlockPosition? {
        val radiusSquared = radius * radius
        val chunkRadius = radius / 16 + 1
        val centerChunkX = center.x shr 4
        val centerChunkZ = center.z shr 4
        for (chunkZ in centerChunkZ - chunkRadius..centerChunkZ + chunkRadius) {
            for (chunkX in centerChunkX - chunkRadius..centerChunkX + chunkRadius) {
                val chunk = world.state.chunkIfLoaded(chunkX, chunkZ) ?: continue
                val minX = maxOf(center.x - radius, chunkX * 16)
                val maxX = minOf(center.x + radius, chunkX * 16 + 15)
                val minZ = maxOf(center.z - radius, chunkZ * 16)
                val maxZ = minOf(center.z + radius, chunkZ * 16 + 15)
                val minY = maxOf(center.y - radius, chunk.verticalBounds.minY)
                val maxY = minOf(center.y + radius, chunk.verticalBounds.maxY)
                for (y in minY..maxY) {
                    for (z in minZ..maxZ) {
                        for (x in minX..maxX) {
                            val dx = x - center.x
                            val dy = y - center.y
                            val dz = z - center.z
                            if (dx * dx + dy * dy + dz * dz > radiusSquared) continue
                            val state = chunk.getBlockStateId(x and 15, y, z and 15)
                            if (BlockStateDefinitions.blockOf(state)?.path == MEETING_BLOCK) {
                                return BlockPosition(x, y, z)
                            }
                        }
                    }
                }
            }
        }
        return null
    }

    /** Ten signed-square probes through the wandering trader's MOTION_BLOCKING_NO_LEAVES heightmap. */
    private fun findSpawnPositionNear(
        world: LoadedWorld,
        reference: BlockPosition,
        radius: Int,
    ): BlockPosition? {
        repeat(SPAWN_ATTEMPTS) {
            val x = reference.x + random.nextInt(radius * 2) - radius
            val z = reference.z + random.nextInt(radius * 2) - radius
            val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return@repeat
            val y = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES)
            if (isOnGroundSpawnPosition(world, x, y, z)) return BlockPosition(x, y, z)
        }
        return null
    }

    private fun isOnGroundSpawnPosition(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (world.border.distanceOutside(x + 0.5, z + 0.5) > 0.0) return false
        val ground = world.state.getBlockState(x, y - 1, z) ?: return false
        if (!BlockShapes.isFaceSturdy(ground, BlockFace.UP) || BlockLight.emission(ground) >= MAX_GROUND_EMISSION) {
            return false
        }
        return NaturalSpawner.isValidEmptySpawnBlock(world, WANDERING_TRADER, x, y, z) &&
            NaturalSpawner.isValidEmptySpawnBlock(world, WANDERING_TRADER, x, y + 1, z)
    }

    /** The separate inclusive 2x3x2 collision-shape volume checked only for the trader itself. */
    private fun hasEnoughSpace(
        world: LoadedWorld,
        position: BlockPosition,
    ): Boolean {
        for (x in position.x..position.x + 1) {
            for (y in position.y..position.y + 2) {
                for (z in position.z..position.z + 1) {
                    val state = world.state.getBlockState(x, y, z) ?: return false
                    if (!BlockShapes.isEmptyShape(state)) return false
                }
            }
        }
        return true
    }

    private companion object {
        val WANDERING_TRADER: Identifier = Identifier.vanilla("wandering_trader")
        const val MEETING_BLOCK = "bell"
        const val WITHOUT_TRADER_BIOME = "the_void"
        const val DEFAULT_TICK_DELAY = 1_200
        const val DEFAULT_SPAWN_DELAY = 24_000
        const val MIN_SPAWN_CHANCE = 25
        const val MAX_SPAWN_CHANCE = 75
        const val SPAWN_CHANCE_INCREASE = 25
        const val PERCENT_RANGE = 100
        const val SPAWN_ONE_IN_X_CHANCE = 10
        const val SPAWN_ATTEMPTS = 10
        const val REFERENCE_RADIUS = 48
        const val LLAMA_RADIUS = 4
        const val LLAMA_COUNT = 2
        const val MAX_GROUND_EMISSION = 14
    }
}

internal data class WanderingTraderSpawnState(
    val spawnDelay: Int,
    val spawnChance: Int,
)

internal data class WanderingTraderSpawnRequest(
    val x: Double,
    val y: Double,
    val z: Double,
    val referenceX: Int,
    val referenceY: Int,
    val referenceZ: Int,
)

internal data class TraderLlamaSpawnRequest(
    val x: Double,
    val y: Double,
    val z: Double,
)

private data class BlockPosition(
    val x: Int,
    val y: Int,
    val z: Int,
)
