package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `NetherPortalBlock.randomTick`, zombified piglins spawning in an overworld portal. */
@OptIn(ExperimentalUuidApi::class)
public object NetherPortalGrowth {
    /**
     * parity: `NetherPortalBlock.randomTick`, which is where the zombified piglins in an overworld
     * portal come from. The attribute defaults to off and the overworld's dimension type turns it
     * on, so it follows the dimension here.
     *
     * The piglin it spawns stands still, because nothing has a brain yet.
     */
    public fun netherPortal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        allocateEntityId: () -> Int,
    ): ServerEntity? {
        if (!world.declaration.dimension.spawnsPortalPiglins) return null
        if (!world.gameRules[GameRuleRegistry.SPAWN_MONSTERS]) return null
        val difficulty = difficultyOf(world)
        if (difficulty <= 0) return null
        if (random.nextInt(PORTAL_ODDS) >= difficulty) return null

        var floorY = y
        while (BlockStateDefinitions.blockOf(world.state.getBlockState(x, floorY, z) ?: 0)?.path == "nether_portal") {
            floorY--
        }
        val ground = world.state.getBlockState(x, floorY, z) ?: return null
        if (!BlockShapes.isFaceSturdy(ground, org.basaltmc.server.registry.BlockFace.UP)) return null
        val type = EntityTypes[Identifier.vanilla("zombified_piglin")] ?: return null
        val entity =
            ServerEntity(
                entityId = allocateEntityId(),
                uuid = Uuid.random(),
                type = Identifier.vanilla("zombified_piglin"),
                protocolType = type.protocolId,
                x = x + 0.5,
                y = (floorY + 1).toDouble(),
                z = z + 0.5,
            )
        return world.state.spawnEntity(entity)
    }

    private fun difficultyOf(world: LoadedWorld): Int =
        when (world.settings.difficulty) {
            "peaceful" -> 0
            "easy" -> 1
            "normal" -> 2
            else -> 3
        }

    /** parity: `NetherPortalBlock.randomTick`'s `nextInt(2000) < difficulty`. */
    private const val PORTAL_ODDS = 2000
}
