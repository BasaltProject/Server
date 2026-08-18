package org.basaltmc.server.runtime.entity

import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.wire.common.Identifier
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * The block of TNT once it is lit: an entity that falls, slides and goes off when its fuse runs out.
 *
 * parity: `PrimedTnt`. It is not the block exploding where it stood: the entity keeps 0.7 of its
 * sideways speed on landing and half its fall, so TNT pushed by a piston or dropped off a ledge goes
 * off somewhere else. That difference is what a cannon is built on.
 */
@OptIn(ExperimentalUuidApi::class)
public object PrimedTnt {
    public val TYPE: Identifier = Identifier.vanilla("tnt")

    /** parity: `PrimedTnt.DEFAULT_FUSE_TIME`, four seconds. */
    public const val DEFAULT_FUSE: Int = 80

    /** parity: `PrimedTnt.DEFAULT_EXPLOSION_POWER`. */
    public const val EXPLOSION_POWER: Float = 4.0f

    /** parity: `PrimedTnt.getDefaultGravity`. */
    private const val GRAVITY = 0.04

    /** parity: `getDeltaMovement().multiply(0.7, -0.5, 0.7)` on landing. */
    private const val GROUND_DRAG = 0.7
    private const val LANDING_BOUNCE = -0.5

    /** parity: the 1/16 of a block above its feet the explosion is centred on (`getY(0.0625)`). */
    public const val EXPLOSION_OFFSET: Double = 0.0625

    /** Lights the TNT that stood at these block coordinates, as `TntBlock.explode` spawns it. */
    public fun prime(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
        entityId: Int,
        fuse: Int = DEFAULT_FUSE,
        protocolType: Int,
    ): ServerEntity {
        val entity =
            ServerEntity(
                entityId = entityId,
                uuid = Uuid.random(),
                type = TYPE,
                protocolType = protocolType,
                // parity: `TntBlock.explode` spawns it at the middle of the block it was.
                x = x + 0.5,
                y = y.toDouble(),
                z = z + 0.5,
            )
        entity.fuse = fuse
        return world.spawnEntity(entity)
    }

    /**
     * One tick: it moves, then the fuse burns down. Answers true when the fuse ran out, which is
     * when the caller explodes it.
     *
     * parity: `PrimedTnt.tick`, in its order: the move happens before the fuse is counted, so TNT
     * lit at the top of a fall goes off where it lands and not where it started.
     */
    public fun tick(
        world: WorldState,
        entity: ServerEntity,
    ): Boolean {
        EntityPhysics.move(
            world,
            entity,
            gravity = GRAVITY,
            horizontalGroundDrag = GROUND_DRAG,
            verticalGroundDrag = LANDING_BOUNCE,
        )
        entity.age++
        entity.fuse--
        return entity.fuse <= 0
    }
}
