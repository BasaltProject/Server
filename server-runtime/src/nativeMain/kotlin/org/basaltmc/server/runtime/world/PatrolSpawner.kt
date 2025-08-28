package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.Gamemode
import kotlin.math.ceil
import kotlin.math.floor

/** Minecraft 26.2's Overworld custom spawner for pillager patrols. */
internal class PatrolSpawner {
    private var nextTick = 0

    fun tick(
        world: LoadedWorld,
        spawnEnemies: Boolean,
        players: List<PlayerSession>,
        random: BasaltRandom,
        spawn: (PatrolSpawnRequest) -> Boolean,
    ) {
        if (world.declaration.dimension != WorldDimension.OVERWORLD || !spawnEnemies) return
        if (!world.gameRules[GameRuleRegistry.SPAWN_PATROLS]) return
        nextTick--
        if (nextTick > 0) return
        nextTick += BASE_INTERVAL_TICKS + random.nextInt(INTERVAL_RANGE_TICKS)
        if (DaylightDetectors.skyDarken(world) >= BRIGHT_OUTSIDE_DARKEN || random.nextInt(ATTEMPT_ODDS) != 0) return
        if (players.isEmpty()) return

        val player = players[random.nextInt(players.size)]
        if (player.gamemode == Gamemode.SPECTATOR) return
        val playerX = floor(player.x).toInt()
        val playerY = floor(player.y).toInt()
        val playerZ = floor(player.z).toInt()
        if (VillageProximity.isCloseToVillage(world, playerX, playerY, playerZ, VILLAGE_SECTION_DISTANCE)) return

        var spawnX = playerX + signedOffset(random)
        var spawnZ = playerZ + signedOffset(random)
        if (
            !hasChunksAt(
                world,
                spawnX - LOADED_RADIUS,
                spawnZ - LOADED_RADIUS,
                spawnX + LOADED_RADIUS,
                spawnZ + LOADED_RADIUS,
            )
        ) {
            return
        }
        val initialBiome = NaturalSpawner.biomeAt(world, spawnX, playerY, spawnZ)
        if (world.clock.totalTicks < PATROLS_ENABLED_AT || initialBiome?.path == MUSHROOM_FIELDS) return

        val effectiveDifficulty = effectiveDifficultyAt(world, spawnX, spawnZ)
        val groupSize = ceil(effectiveDifficulty.toDouble()).toInt() + 1
        repeat(groupSize) { index ->
            val chunk = world.state.chunkIfLoaded(spawnX shr 4, spawnZ shr 4) ?: return@repeat
            val spawnY = chunk.heightAt(spawnX and 15, spawnZ and 15, Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES)
            val accepted = spawnMember(world, spawnX, spawnY, spawnZ, index == 0, random, spawn)
            if (index == 0 && !accepted) return
            spawnX += random.nextInt(MEMBER_SPREAD) - random.nextInt(MEMBER_SPREAD)
            spawnZ += random.nextInt(MEMBER_SPREAD) - random.nextInt(MEMBER_SPREAD)
        }
    }

    private fun spawnMember(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        leader: Boolean,
        random: BasaltRandom,
        spawn: (PatrolSpawnRequest) -> Boolean,
    ): Boolean {
        if (!NaturalSpawner.isValidEmptySpawnBlock(world, PILLAGER, x, y, z)) return false
        if (WorldLight.blockLight(world, x, y, z) > MAX_BLOCK_LIGHT) return false
        if (!NaturalSpawner.canSpawnOnGround(world, x, y, z)) return false
        return spawn(
            PatrolSpawnRequest(
                x.toDouble(),
                y.toDouble(),
                z.toDouble(),
                leader,
                NaturalSpawner.biomeAt(world, x, y, z),
            ),
        )
    }

    private fun effectiveDifficultyAt(
        world: LoadedWorld,
        x: Int,
        z: Int,
    ): Float {
        val inhabitedTime = world.state.chunkIfLoaded(x shr 4, z shr 4)?.inhabitedTime ?: 0L
        return MobLocalDifficulty.effectiveDifficulty(
            world.difficulty,
            world.gameTime,
            inhabitedTime,
            NaturalSpawner.surfaceSlimeChance(world.clock.totalTicks) * 2f,
        )
    }

    private fun signedOffset(random: BasaltRandom): Int {
        val distance = MIN_PLAYER_OFFSET + random.nextInt(PLAYER_OFFSET_RANGE)
        return if (random.nextBoolean()) -distance else distance
    }

    private fun hasChunksAt(
        world: LoadedWorld,
        minX: Int,
        minZ: Int,
        maxX: Int,
        maxZ: Int,
    ): Boolean {
        for (chunkX in (minX shr 4)..(maxX shr 4)) {
            for (chunkZ in (minZ shr 4)..(maxZ shr 4)) {
                if (world.state.chunkIfLoaded(chunkX, chunkZ) == null) return false
            }
        }
        return true
    }

    private companion object {
        val PILLAGER = Identifier.vanilla("pillager")
        const val MUSHROOM_FIELDS = "mushroom_fields"
        const val BASE_INTERVAL_TICKS = 12_000
        const val INTERVAL_RANGE_TICKS = 1_200
        const val ATTEMPT_ODDS = 5
        const val BRIGHT_OUTSIDE_DARKEN = 4
        const val VILLAGE_SECTION_DISTANCE = 2
        const val MIN_PLAYER_OFFSET = 24
        const val PLAYER_OFFSET_RANGE = 24
        const val LOADED_RADIUS = 10
        const val MEMBER_SPREAD = 5
        const val MAX_BLOCK_LIGHT = 8
        const val PATROLS_ENABLED_AT = 120_000L
    }
}

internal data class PatrolSpawnRequest(
    val x: Double,
    val y: Double,
    val z: Double,
    val leader: Boolean,
    val biome: Identifier?,
)
