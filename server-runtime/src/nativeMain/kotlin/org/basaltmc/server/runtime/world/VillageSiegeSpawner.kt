package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.light.BlockLight
import kotlin.math.PI
import kotlin.math.floor
import kotlin.math.sin

/** Minecraft 26.2's midnight village zombie siege. */
internal class VillageSiegeSpawner {
    private var hasSetupSiege = false
    private var siegeTonight = false
    private var zombiesToSpawn = 0
    private var nextSpawnTime = 0
    private var spawnX = 0
    private var spawnY = 0
    private var spawnZ = 0

    fun tick(
        world: LoadedWorld,
        spawnEnemies: Boolean,
        players: List<PlayerSession>,
        random: BasaltRandom,
        spawn: (VillageSiegeSpawnRequest) -> Unit,
    ) {
        if (world.declaration.dimension != WorldDimension.OVERWORLD) return
        if (DaylightDetectors.skyDarken(world) < BRIGHT_OUTSIDE_DARKEN || !spawnEnemies) {
            siegeTonight = false
            hasSetupSiege = false
            return
        }

        if (world.clock.totalTicks.mod(WorldClock.DAY_LENGTH.toLong()) == ROLL_TIME) {
            siegeTonight = random.nextInt(SIEGE_CHANCE) == 0
        }
        if (!siegeTonight) return

        if (!hasSetupSiege) {
            if (!tryToSetupSiege(world, players, random)) return
            hasSetupSiege = true
        }

        if (nextSpawnTime > 0) {
            nextSpawnTime--
            return
        }
        nextSpawnTime = SPAWN_DELAY
        if (zombiesToSpawn > 0) {
            findRandomSpawnPos(world, spawnX, spawnZ, random)?.let(spawn)
            zombiesToSpawn--
        } else {
            siegeTonight = false
        }
    }

    /** Returns true once a qualifying player is found, even if all ten center probes fail. */
    private fun tryToSetupSiege(
        world: LoadedWorld,
        players: List<PlayerSession>,
        random: BasaltRandom,
    ): Boolean {
        for (player in players) {
            if (player.gamemode == org.basaltmc.wire.protocol.play.Gamemode.SPECTATOR) continue
            val centerX = floor(player.x).toInt()
            val centerY = floor(player.y).toInt()
            val centerZ = floor(player.z).toInt()
            if (!VillageProximity.isCloseToVillage(world, centerX, centerY, centerZ, VILLAGE_SECTION_DISTANCE)) continue
            if (NaturalSpawner.biomeAt(world, centerX, centerY, centerZ)?.path == MUSHROOM_FIELDS) continue

            repeat(CENTER_ATTEMPTS) {
                val angle = random.nextFloat() * TWO_PI
                spawnX = centerX + floor(minecraftCos(angle) * CENTER_DISTANCE).toInt()
                spawnY = centerY
                spawnZ = centerZ + floor(minecraftSin(angle) * CENTER_DISTANCE).toInt()
                if (findRandomSpawnPos(world, spawnX, spawnZ, random) != null) {
                    nextSpawnTime = 0
                    zombiesToSpawn = ZOMBIE_COUNT
                    return true
                }
            }
            return true
        }
        return false
    }

    private fun findRandomSpawnPos(
        world: LoadedWorld,
        centerX: Int,
        centerZ: Int,
        random: BasaltRandom,
    ): VillageSiegeSpawnRequest? {
        repeat(POSITION_ATTEMPTS) {
            val x = centerX + random.nextInt(POSITION_DIAMETER) - POSITION_RADIUS
            val z = centerZ + random.nextInt(POSITION_DIAMETER) - POSITION_RADIUS
            val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return@repeat
            val y = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE)
            if (!VillageProximity.isCloseToVillage(world, x, y, z, VILLAGE_SECTION_DISTANCE)) return@repeat
            if (!checkZombieSpawnRules(world, x, y, z, random)) return@repeat
            return VillageSiegeSpawnRequest(x + 0.5, y.toDouble(), z + 0.5)
        }
        return null
    }

    /** `Monster.checkMonsterSpawnRules(EVENT)`, including its RNG order. */
    private fun checkZombieSpawnRules(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean {
        if (WorldLight.skyLight(world, x, y, z) > random.nextInt(MAX_RANDOM_SKY_LIGHT)) return false
        if (WorldLight.blockLight(world, x, y, z) > MAX_MONSTER_BLOCK_LIGHT) return false
        val skyDampen = if (world.weather.thundering) THUNDER_SKY_DAMPEN else DaylightDetectors.skyDarken(world)
        if (WorldLight.rawBrightness(world, x, y, z, skyDampen) > random.nextInt(MONSTER_LIGHT_RANGE)) return false
        val ground = world.state.getBlockState(x, y - 1, z) ?: return false
        return BlockShapes.isFaceSturdy(ground, BlockFace.UP) && BlockLight.emission(ground) < MAX_GROUND_EMISSION
    }

    private fun minecraftSin(angle: Float): Float {
        val index = (angle * SIN_INDEX_SCALE).toLong().toInt() and SIN_INDEX_MASK
        return sin(index / SIN_INDEX_SCALE).toFloat()
    }

    private fun minecraftCos(angle: Float): Float {
        val index = ((angle * SIN_INDEX_SCALE).toLong().toInt() + COS_INDEX_OFFSET) and SIN_INDEX_MASK
        return sin(index / SIN_INDEX_SCALE).toFloat()
    }

    private companion object {
        const val MUSHROOM_FIELDS = "mushroom_fields"
        const val BRIGHT_OUTSIDE_DARKEN = 4
        const val ROLL_TIME = 18_000L
        const val SIEGE_CHANCE = 10
        const val CENTER_ATTEMPTS = 10
        const val CENTER_DISTANCE = 32f
        const val POSITION_ATTEMPTS = 10
        const val POSITION_RADIUS = 8
        const val POSITION_DIAMETER = 16
        const val VILLAGE_SECTION_DISTANCE = 1
        const val ZOMBIE_COUNT = 20
        const val SPAWN_DELAY = 2
        const val MAX_RANDOM_SKY_LIGHT = 32
        const val MAX_MONSTER_BLOCK_LIGHT = 0
        const val MONSTER_LIGHT_RANGE = 8
        const val THUNDER_SKY_DAMPEN = 10
        const val MAX_GROUND_EMISSION = 14
        const val TWO_PI = (PI * 2.0).toFloat()
        const val SIN_INDEX_SCALE = 10430.378350470453
        const val SIN_INDEX_MASK = 65_535
        const val COS_INDEX_OFFSET = 16_384
    }
}

internal data class VillageSiegeSpawnRequest(
    val x: Double,
    val y: Double,
    val z: Double,
)
