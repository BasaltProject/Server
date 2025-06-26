package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.runtime.entity.ServerEntity

/** A world that does nothing back, for the goal tests that only care about where the mob ends up. */
internal object NoMobActions : MobActions {
    override fun swing(mob: ServerEntity) {}

    override fun hurtPlayer(
        mob: ServerEntity,
        playerId: Int,
        damage: Float,
    ) {}

    override fun hurtMob(
        mob: ServerEntity,
        target: ServerEntity,
        damage: Float,
    ) {}

    override fun breed(
        mob: ServerEntity,
        partner: ServerEntity,
    ) {}

    override fun eatGrass(mob: ServerEntity) {}

    override fun primeCreeper(mob: ServerEntity) {}

    override fun explodeCreeper(mob: ServerEntity) {}

    override fun setCharging(
        mob: ServerEntity,
        charging: Boolean,
    ) {}

    override fun shootFireball(
        mob: ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        large: Boolean,
    ) {}

    override fun setBeamTarget(
        mob: ServerEntity,
        targetId: Int?,
    ) {}

    override fun setCasting(
        mob: ServerEntity,
        casting: Boolean,
    ) {}

    override fun throwPotion(
        mob: ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        potion: String,
    ) {}

    override fun castFangs(
        mob: ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
    ) {}

    override fun summonVexes(
        mob: ServerEntity,
        count: Int,
    ) {}

    override fun applyEffect(
        playerId: Int,
        effect: String,
        durationTicks: Int,
        amplifier: Int,
    ) {}

    override fun setSleeping(
        mob: ServerEntity,
        sleeping: Boolean,
    ) {}

    override fun teleported(mob: ServerEntity) {}

    override fun shootArrow(
        mob: ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        power: Float,
    ) {}
}
