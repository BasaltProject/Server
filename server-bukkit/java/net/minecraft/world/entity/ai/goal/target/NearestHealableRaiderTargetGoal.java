// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.raid.Raider;

public class NearestHealableRaiderTargetGoal<T extends LivingEntity> extends NearestAttackableTargetGoal<T> {
    private static final int DEFAULT_COOLDOWN = 200;
    private int cooldown;

    public NearestHealableRaiderTargetGoal(Raider raider, Class<T> targetType, boolean mustSee, TargetingConditions.Selector subselector) {
        super(raider, targetType, 500, mustSee, false, subselector);
    }

    public int getCooldown() {
        return cooldown;
    }

    public void decrementCooldown() {
        cooldown--;
    }

    @Override
    public boolean canUse() {
        return cooldown <= 0 && mob.getRandom().nextBoolean() && super.canUse();
    }

    @Override
    public void start() {
        cooldown = reducedTickDelay(DEFAULT_COOLDOWN);
        super.start();
    }
}
