// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;
import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.monster.Creeper;
public class SwellGoal extends Goal {
    private final Creeper creeper;
    private LivingEntity target;
    public SwellGoal(Creeper creeper) {
        this.creeper = creeper;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }
    public boolean canUse() {
        LivingEntity candidate = creeper.getTarget();
        return creeper.getSwellDir() > 0 || candidate != null && creeper.distanceToSqr(candidate) < 9.0;
    }
    public void start() {
        creeper.getNavigation().stop();
        target = creeper.getTarget();
    }
    public void stop() {
        target = null;
    }
    public boolean requiresUpdateEveryTick() {
        return true;
    }
    public void tick() {
        if (target == null || creeper.distanceToSqr(target) > 49.0 || !creeper.getSensing().hasLineOfSight(target)) {
            creeper.setSwellDir(-1);
        } else {
            creeper.setSwellDir(1);
        }
    }
}
