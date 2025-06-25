package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.wire.common.Identifier
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.sqrt
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * The things that are thrown or shot: how they fly, and where they land.
 *
 * parity: `Projectile.shoot` for the aim, `ThrowableProjectile.tick` and `AbstractArrow.tick` for
 * the flight. The two differ in what they weigh and what stops them: a snowball is gone the moment
 * it hits anything, an arrow sticks in the block it hit and stays there.
 */
@OptIn(ExperimentalUuidApi::class)
public object Projectiles {
    /** One kind of projectile: what it weighs, what the air does to it, and whether it sticks. */
    public class Kind(
        public val type: Identifier,
        public val gravity: Double,
        public val drag: Double,
        public val sticks: Boolean,
    )

    /** parity: `AbstractArrow.getDefaultGravity` and its 0.99 of air drag. */
    public val ARROW: Kind = Kind(Identifier.vanilla("arrow"), gravity = 0.05, drag = 0.99, sticks = true)
    public val SPECTRAL_ARROW: Kind =
        Kind(Identifier.vanilla("spectral_arrow"), gravity = 0.05, drag = 0.99, sticks = true)

    /** parity: `ThrowableProjectile.getDefaultGravity`, lighter than an arrow and gone on impact. */
    public val SNOWBALL: Kind = Kind(Identifier.vanilla("snowball"), gravity = 0.03, drag = 0.99, sticks = false)
    public val EGG: Kind = Kind(Identifier.vanilla("egg"), gravity = 0.03, drag = 0.99, sticks = false)
    public val ENDER_PEARL: Kind =
        Kind(Identifier.vanilla("ender_pearl"), gravity = 0.03, drag = 0.99, sticks = false)
    public val EXPERIENCE_BOTTLE: Kind =
        Kind(Identifier.vanilla("experience_bottle"), gravity = 0.07, drag = 0.99, sticks = false)
    public val SPLASH_POTION: Kind =
        Kind(Identifier.vanilla("potion"), gravity = 0.05, drag = 0.99, sticks = false)

    /** parity: `ProjectileItem.DispenseConfig`'s own defaults. */
    public const val DISPENSER_POWER: Float = 1.1f
    public const val DISPENSER_UNCERTAINTY: Float = 6.0f

    /** parity: `getMovementToShoot`, the spread one degree of uncertainty is worth. */
    private const val SPREAD_PER_UNCERTAINTY = 0.0172275

    /**
     * parity: the items `DispenseItemBehavior.bootStrap` registers a projectile behaviour for.
     * The ones this build has no entity for (fireworks, wind charges, lingering potions) are not
     * here and fall back to being thrown as an item.
     */
    private val DISPENSED: Map<String, Kind> =
        mapOf(
            "arrow" to ARROW,
            "tipped_arrow" to ARROW,
            "spectral_arrow" to SPECTRAL_ARROW,
            "egg" to EGG,
            "blue_egg" to EGG,
            "brown_egg" to EGG,
            "snowball" to SNOWBALL,
            "experience_bottle" to EXPERIENCE_BOTTLE,
            "splash_potion" to SPLASH_POTION,
        )

    /** What a dispenser shoots for this item, or null when it just throws it. */
    public fun dispensed(item: Identifier): Kind? = DISPENSED[item.path]

    /** The kind a flying entity of this type is, so a tick knows what it weighs. */
    public fun kindOf(type: Identifier): Kind? = ALL.firstOrNull { it.type == type }

    private val ALL: List<Kind> =
        listOf(ARROW, SPECTRAL_ARROW, SNOWBALL, EGG, ENDER_PEARL, EXPERIENCE_BOTTLE, SPLASH_POTION)

    /** Puts a projectile in the world, aimed along [dirX]/[dirY]/[dirZ]. */
    public fun spawn(
        world: WorldState,
        kind: Kind,
        entityId: Int,
        protocolType: Int,
        x: Double,
        y: Double,
        z: Double,
        dirX: Double,
        dirY: Double,
        dirZ: Double,
        power: Float,
        uncertainty: Float,
        random: BasaltRandom,
    ): ServerEntity {
        val entity =
            ServerEntity(
                entityId = entityId,
                uuid = Uuid.random(),
                type = kind.type,
                protocolType = protocolType,
                x = x,
                y = y,
                z = z,
            )
        shoot(entity, dirX, dirY, dirZ, power, uncertainty, random)
        return world.spawnEntity(entity)
    }

    /**
     * parity: `Projectile.shoot`: the direction is normalised, each axis gets its own triangular
     * spread, and the whole thing is scaled by the power. The rotation follows the movement, which
     * is what points an arrow the way it is going.
     */
    public fun shoot(
        entity: ServerEntity,
        dirX: Double,
        dirY: Double,
        dirZ: Double,
        power: Float,
        uncertainty: Float,
        random: BasaltRandom,
    ) {
        val length = sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ)
        if (length == 0.0) return
        val spread = SPREAD_PER_UNCERTAINTY * uncertainty
        val vx = (dirX / length + random.triangle(0.0, spread)) * power
        val vy = (dirY / length + random.triangle(0.0, spread)) * power
        val vz = (dirZ / length + random.triangle(0.0, spread)) * power
        entity.velocityX = vx
        entity.velocityY = vy
        entity.velocityZ = vz
        aimAlongMovement(entity)
    }

    /**
     * One tick of flight. Answers where it hit, or null if it is still going.
     *
     * parity: the order both projectile classes share: drag, then the line from where it is to
     * where it would be, then gravity. An arrow that hit stops dead and stays; anything else is the
     * caller's to remove.
     */
    public fun tick(
        world: WorldState,
        entity: ServerEntity,
        kind: Kind,
    ): BlockHit? {
        entity.age++
        if (entity.onGround) return null

        entity.velocityX *= kind.drag
        entity.velocityY *= kind.drag
        entity.velocityZ *= kind.drag

        val hit =
            BlockRay.cast(
                world,
                entity.x,
                entity.y,
                entity.z,
                entity.x + entity.velocityX,
                entity.y + entity.velocityY,
                entity.z + entity.velocityZ,
            )
        if (hit != null) {
            entity.x = hit.x
            entity.y = hit.y
            entity.z = hit.z
            if (kind.sticks) {
                // parity: `AbstractArrow.setInGround`, which zeroes the movement and leaves it stuck.
                entity.velocityX = 0.0
                entity.velocityY = 0.0
                entity.velocityZ = 0.0
                entity.onGround = true
            }
            return hit
        }

        entity.x += entity.velocityX
        entity.y += entity.velocityY
        entity.z += entity.velocityZ
        entity.velocityY -= kind.gravity
        aimAlongMovement(entity)
        return null
    }

    /**
     * How far along the line from/to a box is first touched, from zero to one, or null when the
     * line misses it.
     *
     * parity: `AABB.clip` over `ProjectileUtil.getEntityHitResult`, which is what makes an arrow
     * hit whoever it passes through rather than only whoever it ends the tick inside.
     */
    public fun clipBox(
        fromX: Double,
        fromY: Double,
        fromZ: Double,
        toX: Double,
        toY: Double,
        toZ: Double,
        minX: Double,
        minY: Double,
        minZ: Double,
        maxX: Double,
        maxY: Double,
        maxZ: Double,
    ): Double? {
        var enter = 0.0
        var exit = 1.0
        val axes =
            arrayOf(
                doubleArrayOf(fromX, toX - fromX, minX, maxX),
                doubleArrayOf(fromY, toY - fromY, minY, maxY),
                doubleArrayOf(fromZ, toZ - fromZ, minZ, maxZ),
            )
        for (axis in axes) {
            val (start, delta, low, high) = axis
            if (delta == 0.0) {
                if (start < low || start > high) return null
                continue
            }
            val first = (low - start) / delta
            val second = (high - start) / delta
            enter = maxOf(enter, minOf(first, second))
            exit = minOf(exit, maxOf(first, second))
            if (enter > exit) return null
        }
        return enter
    }

    /** parity: the two `atan2` calls that turn the movement into a yaw and a pitch. */
    private fun aimAlongMovement(entity: ServerEntity) {
        val horizontal = sqrt(entity.velocityX * entity.velocityX + entity.velocityZ * entity.velocityZ)
        entity.yaw = (atan2(entity.velocityX, entity.velocityZ) * DEGREES).toFloat()
        entity.pitch = (atan2(entity.velocityY, horizontal) * DEGREES).toFloat()
    }

    private const val DEGREES = 180.0 / PI
}
