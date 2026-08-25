package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects

/**
 * Status effects: their per-tick countdown, and what a tick of each one does.
 *
 * [tickOmens] is [RaidSystem]'s: a bad omen ticking down over a village is how a raid starts,
 * so that one case is handed off rather than reimplemented here.
 *
 * parity: `LivingEntity.tickEffects` plus each effect's own `applyEffectTick`.
 */
public class StatusEffectSystem(
    private val playerRegistry: PlayerRegistry,
    private val tickOmens: (LoadedWorld, PlayerSession, ActiveEffect) -> Unit,
) {
    /**
     * Counts every status effect down and applies the ones that act on a tick.
     *
     * parity: `LivingEntity.tickEffects` plus each effect's own `applyEffectTick`, whose intervals
     * are the halvings by amplifier that make a stronger regeneration heal faster.
     */
    public fun tickEffects(world: LoadedWorld) {
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name || player.dead) continue
            if (player.effects.isEmpty()) continue
            for (active in player.effects.values.toList()) {
                applyEffectTick(world, player, active)
                if (active.infinite) continue
                active.remainingTicks--
                if (active.remainingTicks <= 0) {
                    player.effects.remove(active.effect)
                    player.connection.sendEffectRemoved(active.effect)
                }
            }
        }
    }

    private fun applyEffectTick(
        world: LoadedWorld,
        player: PlayerSession,
        active: ActiveEffect,
    ) {
        val ticks = active.remainingTicks
        when (active.effect) {
            MobEffects.REGENERATION ->
                if (onInterval(ticks, REGENERATION_INTERVAL, active.amplifier) &&
                    player.health < player.maxHealth
                ) {
                    player.connection.heal(1f)
                }
            MobEffects.POISON ->
                if (onInterval(ticks, POISON_INTERVAL, active.amplifier) && player.health > 1f) {
                    player.connection.hurt(1f, "magic", DamageTypes.MAGIC)
                }
            MobEffects.WITHER ->
                if (onInterval(ticks, WITHER_INTERVAL, active.amplifier)) {
                    player.connection.hurt(1f, "wither", DamageTypes.WITHER)
                }
            MobEffects.HUNGER -> player.food.addExhaustion(HUNGER_EXHAUSTION * (active.amplifier + 1))
            MobEffects.BAD_OMEN, MobEffects.RAID_OMEN -> tickOmens(world, player, active)
            else -> Unit
        }
    }

    /** parity: `shouldApplyEffectTickThisTick`, whose interval halves with every level. */
    private fun onInterval(
        ticks: Int,
        base: Int,
        amplifier: Int,
    ): Boolean {
        val interval = base shr amplifier
        return interval <= 0 || ticks % interval == 0
    }

    private companion object {
        // parity: each effect's own `shouldApplyEffectTickThisTick`, halved per level.
        const val REGENERATION_INTERVAL = 50
        const val POISON_INTERVAL = 25
        const val WITHER_INTERVAL = 40
        const val HUNGER_EXHAUSTION = 0.005f
    }
}
