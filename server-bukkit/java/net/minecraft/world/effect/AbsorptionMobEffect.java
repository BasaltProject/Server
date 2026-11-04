// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
class AbsorptionMobEffect extends MobEffect {
    protected AbsorptionMobEffect(MobEffectCategory category, int color) {
        super((net.minecraft.world.effect.MobEffectCategory) null, (int) 0);
    }
    public boolean applyEffectTick(ServerLevel level, LivingEntity mob, int amplification) {
        return true;
    }
    public boolean shouldApplyEffectTickThisTick(int tickCount, int amplification) {
        return false;
    }
    public void onEffectStarted(LivingEntity mob, int amplifier) {
        mob.setAbsorptionAmount(Math.max(mob.getAbsorptionAmount(), 4 * (amplifier + 1)));
    }
}
