// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
class SaturationMobEffect extends InstantaneousMobEffect {
    protected SaturationMobEffect(MobEffectCategory category, int color) {
        super((net.minecraft.world.effect.MobEffectCategory) null, (int) 0);
    }
    public boolean applyEffectTick(ServerLevel level, LivingEntity mob, int amplification) {
        return true;
    }
}
