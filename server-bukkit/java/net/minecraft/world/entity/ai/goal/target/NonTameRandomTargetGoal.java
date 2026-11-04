// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;

public class NonTameRandomTargetGoal<T extends LivingEntity> extends NearestAttackableTargetGoal<T> {
    private final TamableAnimal tamableMob;

    public NonTameRandomTargetGoal(TamableAnimal mob, Class<T> targetType, boolean mustSee, TargetingConditions.Selector selector) {
        super(mob, targetType, 10, mustSee, false, selector);
        this.tamableMob = mob;
    }

    @Override
    public boolean canUse() {
        return !tamableMob.isTame() && super.canUse();
    }

    @Override
    public boolean canContinueToUse() {
        return targetConditions != null ? targetConditions.test(getServerLevel(mob), mob, target) : super.canContinueToUse();
    }
}
