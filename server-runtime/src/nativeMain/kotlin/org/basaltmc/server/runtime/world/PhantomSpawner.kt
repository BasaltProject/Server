package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.storage.StoredStats
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.Gamemode
import kotlin.math.floor

/** Minecraft 26.2's Overworld custom spawner for insomnia phantoms. */
internal class PhantomSpawner {
    private var nextTick = 0

    fun tick(
        world: LoadedWorld,
        spawnEnemies: Boolean,
        players: List<PlayerSession>,
        random: BasaltRandom,
        spawn: (SpawnRequest) -> Unit,
    ) {
        if (world.declaration.dimension != WorldDimension.OVERWORLD || !spawnEnemies) return
        if (!world.gameRules[GameRuleRegistry.SPAWN_PHANTOMS]) return
        nextTick--
        if (nextTick > 0) return
        nextTick += (MIN_INTERVAL_SECONDS + random.nextInt(INTERVAL_SECONDS_RANGE)) * TICKS_PER_SECOND
        if (world.declaration.dimension.hasSkylight && DaylightDetectors.skyDarken(world) < MIN_SKY_DARKEN) return

        for (player in players) {
            if (player.gamemode == Gamemode.SPECTATOR) continue
            val playerX = floor(player.x).toInt()
            val playerY = floor(player.y).toInt()
            val playerZ = floor(player.z).toInt()
            if (
                world.declaration.dimension.hasSkylight &&
                (
                    playerY < world.declaration.dimension.seaLevel ||
                        !NaturalSpawner.canSeeSky(world, playerX, playerY, playerZ)
                )
            ) {
                continue
            }

            val inhabitedTime =
                world.state
                    .chunkIfLoaded(playerX shr 4, playerZ shr 4)
                    ?.inhabitedTime ?: 0L
            val moonBrightness = NaturalSpawner.surfaceSlimeChance(world.clock.totalTicks) * 2f
            val effectiveDifficulty =
                MobLocalDifficulty.effectiveDifficulty(
                    world.difficulty,
                    world.gameTime,
                    inhabitedTime,
                    moonBrightness,
                )
            if (effectiveDifficulty <= random.nextFloat() * MAX_DIFFICULTY_ROLL) continue

            val timeSinceRest =
                player.stats[StoredStats.TIME_SINCE_REST]
                    .coerceIn(1L, Int.MAX_VALUE.toLong())
                    .toInt()
            if (random.nextInt(timeSinceRest) < MIN_TIME_SINCE_REST) continue

            val spawnX = playerX - HORIZONTAL_RADIUS + random.nextInt(HORIZONTAL_DIAMETER)
            val spawnY = playerY + MIN_HEIGHT + random.nextInt(HEIGHT_RANGE)
            val spawnZ = playerZ - HORIZONTAL_RADIUS + random.nextInt(HORIZONTAL_DIAMETER)
            if (!NaturalSpawner.isValidEmptySpawnBlock(world, PHANTOM, spawnX, spawnY, spawnZ)) continue

            val group = MobSpawnGroupData()
            val groupSize = 1 + random.nextInt(difficultyId(world.difficulty) + 1)
            repeat(groupSize) { groupIndex ->
                spawn(
                    SpawnRequest(
                        PHANTOM,
                        spawnX + 0.5,
                        spawnY.toDouble(),
                        spawnZ + 0.5,
                        yaw = 0f,
                        groupIndex = groupIndex,
                        group = group,
                        biome = NaturalSpawner.biomeAt(world, spawnX, spawnY, spawnZ),
                    ),
                )
            }
        }
    }

    private fun difficultyId(difficulty: String): Int =
        when (difficulty) {
            "peaceful" -> 0
            "easy" -> 1
            "normal" -> 2
            "hard" -> 3
            else -> 1
        }

    private companion object {
        val PHANTOM = Identifier.vanilla("phantom")
        const val TICKS_PER_SECOND = 20
        const val MIN_INTERVAL_SECONDS = 60
        const val INTERVAL_SECONDS_RANGE = 60
        const val MIN_SKY_DARKEN = 5
        const val MAX_DIFFICULTY_ROLL = 3f
        const val MIN_TIME_SINCE_REST = 72_000
        const val MIN_HEIGHT = 20
        const val HEIGHT_RANGE = 15
        const val HORIZONTAL_RADIUS = 10
        const val HORIZONTAL_DIAMETER = 21
    }
}
