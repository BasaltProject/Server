// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
class RegenerationMobEffect extends MobEffect {
    protected RegenerationMobEffect(MobEffectCategory category, int color) {
        super((net.minecraft.world.effect.MobEffectCategory) null, (int) 0);
    }
    public boolean applyEffectTick(ServerLevel level, LivingEntity mob, int amplification) {
        if (mob.getHealth() < mob.getMaxHealth()) {
            mob.heal(1.0F);
        }
        return true;
    }
    public boolean shouldApplyEffectTickThisTick(int tickCount, int amplification) {
        int interval = 50 >> amplification;
        return interval <= 0 || tickCount % interval == 0;
    }
}
