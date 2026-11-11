// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;

import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;

class HealOrHarmMobEffect extends InstantaneousMobEffect {
    private final boolean harm;

    public HealOrHarmMobEffect(MobEffectCategory category, int color, boolean isHarm) {
        super(category, color);
        this.harm = isHarm;
    }

    private boolean heals(LivingEntity mob) {
        return harm == mob.isInvertedHealAndHarm();
    }

    @Override
    public boolean applyEffectTick(ServerLevel level, LivingEntity mob, int amplification) {
        if (heals(mob)) {
            mob.heal(Math.max(4 << amplification, 0));
        } else {
            mob.hurtServer(level, mob.damageSources().magic(), 6 << amplification);
        }
        return true;
    }

    @Override
    public void applyInstantaneousEffect(ServerLevel serverLevel, Entity source, Entity owner, LivingEntity mob, int amplification, double scale) {
        if (heals(mob)) {
            mob.heal((int) (scale * (4 << amplification) + 0.5));
            return;
        }
        float amount = (int) (scale * (6 << amplification) + 0.5);
        var damage = source == null ? mob.damageSources().magic() : mob.damageSources().indirectMagic(source, owner);
        mob.hurtServer(serverLevel, damage, amount);
    }
}
