// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
public class PoisonMobEffect extends MobEffect {
    public static final int DAMAGE_INTERVAL = 25;
    protected PoisonMobEffect(MobEffectCategory category, int color) {
        super((net.minecraft.world.effect.MobEffectCategory) null, (int) 0);
    }
    public boolean applyEffectTick(ServerLevel level, LivingEntity mob, int amplification) {
        if (mob.getHealth() > 1.0F) {
            mob.hurtServer(level, new net.minecraft.world.damagesource.DamageSource((net.minecraft.core.Holder<net.minecraft.world.damagesource.DamageType>) null), 1.0F);
        }
        return true;
    }
    public boolean shouldApplyEffectTickThisTick(int tickCount, int amplification) {
        int interval = 25 >> amplification;
        return interval <= 0 || tickCount % interval == 0;
    }
}
