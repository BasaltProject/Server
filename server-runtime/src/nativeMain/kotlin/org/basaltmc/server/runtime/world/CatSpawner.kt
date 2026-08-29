package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.world.light.BlockLight
import org.basaltmc.wire.common.Identifier
import kotlin.math.floor

/** Minecraft 26.2's Overworld custom spawner for village and swamp-hut cats. */
internal class CatSpawner(
    /** Structure-piece membership; false until the Overworld generator supplies swamp huts. */
    private val catsSpawnInStructureAt: (LoadedWorld, Int, Int, Int) -> Boolean = { _, _, _, _ -> false },
) {
    private var nextTick = 0

    fun tick(
        world: LoadedWorld,
        players: List<PlayerSession>,
        random: BasaltRandom,
        spawn: (CatSpawnRequest) -> Unit,
    ) {
        if (world.declaration.dimension != WorldDimension.OVERWORLD) return
        nextTick--
        if (nextTick > 0) return
        nextTick = TICK_DELAY

        val alivePlayers = players.filter { !it.dead && it.health > 0f }
        if (alivePlayers.isEmpty()) return
        val player = alivePlayers[random.nextInt(alivePlayers.size)]
        val x = floor(player.x).toInt() + signedOffset(random)
        val y = floor(player.y).toInt()
        val z = floor(player.z).toInt() + signedOffset(random)
        if (!hasChunksAt(world, x - LOADED_RADIUS, z - LOADED_RADIUS, x + LOADED_RADIUS, z + LOADED_RADIUS)) return
        if (!isSpawnPositionOk(world, x, y, z)) return

        if (VillageProximity.isCloseToVillage(world, x, y, z, VILLAGE_SECTION_DISTANCE)) {
            if (VillageProximity.occupiedHomesNear(world, x, y, z, VILLAGE_RADIUS) <= MIN_OCCUPIED_HOMES) return
            if (catsIn(world, x, y, z, VILLAGE_RADIUS) >= MAX_VILLAGE_CATS) return
            spawn(request(world, x, y, z, persistent = false, inHut = false))
        } else if (catsSpawnInStructureAt(world, x, y, z)) {
            if (catsIn(world, x, y, z, HUT_RADIUS) != 0) return
            spawn(request(world, x, y, z, persistent = true, inHut = true))
        }
    }

    private fun request(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        persistent: Boolean,
        inHut: Boolean,
    ): CatSpawnRequest =
        CatSpawnRequest(
            x = x + 0.5,
            y = y.toDouble(),
            z = z + 0.5,
            persistent = persistent,
            context =
                CatSpawnContext(
                    moonBrightness = NaturalSpawner.surfaceSlimeChance(world.clock.totalTicks) * 2f,
                    inBlackCatStructure = inHut,
                ),
            biome = NaturalSpawner.biomeAt(world, x, y, z),
        )

    /** `SpawnPlacementTypes.ON_GROUND`; CatSpawner intentionally does not run the animal predicate. */
    private fun isSpawnPositionOk(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (world.border.distanceOutside(x + 0.5, z + 0.5) > 0.0) return false
        val ground = world.state.getBlockState(x, y - 1, z) ?: return false
        if (!BlockShapes.isFaceSturdy(ground, org.basaltmc.server.registry.BlockFace.UP)) return false
        if (BlockLight.emission(ground) >= MAX_GROUND_LIGHT_EMISSION) return false
        return NaturalSpawner.isValidEmptySpawnBlock(world, CAT, x, y, z) &&
            NaturalSpawner.isValidEmptySpawnBlock(world, CAT, x, y + 1, z)
    }

    private fun catsIn(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        horizontalRadius: Int,
    ): Int {
        val search =
            Aabb(
                x - horizontalRadius.toDouble(),
                y - VERTICAL_RADIUS.toDouble(),
                z - horizontalRadius.toDouble(),
                x + 1.0 + horizontalRadius,
                y + 1.0 + VERTICAL_RADIUS,
                z + 1.0 + horizontalRadius,
            )
        return world.state.entities().count { entity ->
            !entity.removed &&
                entity.type == CAT &&
                Aabb
                    .of(entity.x, entity.y, entity.z, EntityTypeShapes.widthOf(CAT), EntityTypeShapes.heightOf(CAT))
                    .intersects(search)
        }
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
        val CAT = Identifier.vanilla("cat")
        const val TICK_DELAY = 1_200
        const val MIN_PLAYER_OFFSET = 8
        const val PLAYER_OFFSET_RANGE = 24
        const val LOADED_RADIUS = 10
        const val VILLAGE_SECTION_DISTANCE = 2
        const val VILLAGE_RADIUS = 48
        const val HUT_RADIUS = 16
        const val VERTICAL_RADIUS = 8
        const val MIN_OCCUPIED_HOMES = 4
        const val MAX_VILLAGE_CATS = 5
        const val MAX_GROUND_LIGHT_EMISSION = 14
    }
}

internal data class CatSpawnRequest(
    val x: Double,
    val y: Double,
    val z: Double,
    val persistent: Boolean,
    val context: CatSpawnContext,
    val biome: Identifier?,
)
