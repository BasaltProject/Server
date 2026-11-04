// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
class HungerMobEffect extends MobEffect {
    protected HungerMobEffect(MobEffectCategory category, int color) {
        super((net.minecraft.world.effect.MobEffectCategory) null, (int) 0);
    }
    public boolean applyEffectTick(ServerLevel serverLevel, LivingEntity mob, int amplification) {
        if (mob instanceof net.minecraft.world.entity.player.Player player) {
            player.causeFoodExhaustion(0.005F * (amplification + 1));
        }
        return true;
    }
    public boolean shouldApplyEffectTickThisTick(int tickCount, int amplification) {
        return true;
    }
}
