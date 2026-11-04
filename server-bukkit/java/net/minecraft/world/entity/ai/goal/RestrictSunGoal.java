// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.GoalUtils;

public class RestrictSunGoal extends Goal {
    private final PathfinderMob mob;

    public RestrictSunGoal(PathfinderMob mob) {
        this.mob = mob;
    }

    @Override
    public boolean canUse() {
        return mob.level().isBrightOutside() && mob.getItemBySlot(net.minecraft.world.entity.EquipmentSlot.HEAD).isEmpty()
                && GoalUtils.hasGroundPathNavigation(mob);
    }

    @Override
    public void start() {
        ((net.minecraft.world.entity.ai.navigation.GroundPathNavigation) mob.getNavigation()).setAvoidSun(true);
    }

    @Override
    public void stop() {
        if (GoalUtils.hasGroundPathNavigation(mob)) {
            ((net.minecraft.world.entity.ai.navigation.GroundPathNavigation) mob.getNavigation()).setAvoidSun(false);
        }
    }
}
