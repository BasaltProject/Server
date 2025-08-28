package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.wire.common.Identifier

/** A player, as the goals see one: where they are, and what they are holding. */
public class PlayerView(
    public val entityId: Int,
    /** Their uuid, which is how a pet knows its own owner. */
    public val uuid: String,
    public val x: Double,
    public val y: Double,
    public val z: Double,
    public val width: Double,
    public val height: Double,
    /** parity: `TargetingConditions`, which skips a creative or spectating player outright. */
    public val targetable: Boolean,
    public val heldItem: Identifier?,
    public val sneaking: Boolean,
    /** Where they are looking, which is what an enderman reads. */
    public val yaw: Float = 0f,
    public val pitch: Float = 0f,
)

/** What a goal can make the world do beyond moving its own mob. */
public interface MobActions {
    /** parity: `Mob.swing`, the arm the client draws before the hit lands. */
    public fun swing(mob: ServerEntity)

    public fun hurtPlayer(
        mob: ServerEntity,
        playerId: Int,
        damage: Float,
    )

    public fun hurtMob(
        mob: ServerEntity,
        target: ServerEntity,
        damage: Float,
    )

    /** parity: `Animal.spawnChildFromBreeding`, which is the world's to do and not the goal's. */
    public fun breed(
        mob: ServerEntity,
        partner: ServerEntity,
    )

    /** parity: `EatBlockGoal`, which eats the grass under or in front of the mob. */
    public fun eatGrass(mob: ServerEntity)

    /** parity: `SoundEvents.CREEPER_PRIMED`, the hiss when the fuse starts. */
    public fun primeCreeper(mob: ServerEntity)

    /** parity: `Creeper.explodeCreeper`, three blocks of blast where it stood. */
    public fun explodeCreeper(mob: ServerEntity)

    /** parity: `Ghast.setCharging` and `Blaze.setCharged`, the tell before a shot. */
    public fun setCharging(
        mob: ServerEntity,
        charging: Boolean,
    )

    /** parity: `LargeFireball` and `SmallFireball`, thrown from the mob at what it is aiming at. */
    public fun shootFireball(
        mob: ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        large: Boolean,
    )

    /** parity: `Guardian`'s beam, which the client draws from the mob to whatever it names. */
    public fun setBeamTarget(
        mob: ServerEntity,
        targetId: Int?,
    )

    /** parity: `SpellcasterIllager.setIsCastingSpell`, the pose and the particles while it casts. */
    public fun setCasting(
        mob: ServerEntity,
        casting: Boolean,
    )

    /** parity: `Witch.performRangedAttack`, a splash potion thrown at somebody. */
    public fun throwPotion(
        mob: ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        potion: String,
    )

    /** parity: `Evoker.EvokerAttackSpellGoal`, the two lines of fangs it opens under you. */
    public fun castFangs(
        mob: ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
    )

    /** parity: `Evoker.EvokerSummonSpellGoal`, which is where vexes come from. */
    public fun summonVexes(
        mob: ServerEntity,
        count: Int,
    )

    /** parity: an effect put on a player by something that is not a potion they drank. */
    public fun applyEffect(
        playerId: Int,
        effect: String,
        durationTicks: Int,
        amplifier: Int,
    )

    /** parity: `Villager` sleeping, which is a pose the client draws lying in the bed. */
    public fun setSleeping(
        mob: ServerEntity,
        sleeping: Boolean,
    )

    /** parity: `EnderMan.teleport`, which is a move plus the sound and particles at both ends. */
    public fun teleported(mob: ServerEntity)

    /** parity: `AbstractSkeleton.performRangedAttack`, an arrow at 1.6 with the difficulty's spread. */
    public fun shootArrow(
        mob: ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        power: Float,
    )
}

/**
 * Everything one mob's goals are allowed to see this tick.
 *
 * The mob and the world are the two halves vanilla's goals hold as fields; the rest is what this
 * build has to hand them explicitly, since a goal here cannot reach into the server.
 */
public class MobContext(
    public val world: WorldState,
    public val mob: ServerEntity,
    public val random: BasaltRandom,
    public val entities: List<ServerEntity>,
    public val players: List<PlayerView>,
    public val actions: MobActions,
    /** parity: `Level.getMaxLocalRawBrightness`, which is what the light-shy goals ask. */
    public val light: Int,
    public val night: Boolean,
    /** parity: the villager schedule reads the day's own time rather than the total. */
    public val dayTime: Long = 0L,
    /** parity: `Level.getGameTime`, used by absolute AI cooldown deadlines. */
    public val gameTime: Long = 0L,
    /** parity: `Raid`, which is what puts a village in its hide activity. */
    public val raidNearby: Boolean = false,
    /** parity: `Attributes.MOVEMENT_SPEED` for this mob, with its modifiers. */
    public val speed: Double,
) {
    public fun nearestPlayer(range: Double): PlayerView? {
        var best: PlayerView? = null
        var bestDistance = range * range
        for (player in players) {
            if (!player.targetable) continue
            val distance = distanceSquaredTo(player.x, player.y, player.z)
            if (distance > bestDistance) continue
            bestDistance = distance
            best = player
        }
        return best
    }

    public fun distanceSquaredTo(
        x: Double,
        y: Double,
        z: Double,
    ): Double {
        val dx = mob.x - x
        val dy = mob.y - y
        val dz = mob.z - z
        return dx * dx + dy * dy + dz * dz
    }

    public fun playerById(entityId: Int): PlayerView? = players.firstOrNull { it.entityId == entityId }

    public fun entityById(entityId: Int): ServerEntity? = entities.firstOrNull { it.entityId == entityId }
}
