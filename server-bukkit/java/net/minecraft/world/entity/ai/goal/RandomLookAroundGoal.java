// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.Mob;

public class RandomLookAroundGoal extends Goal {
    private final Mob mob;
    private double relX;
    private double relZ;
    private int lookTime;

    public RandomLookAroundGoal(Mob mob) {
        this.mob = mob;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        return mob.getRandom().nextFloat() < 0.02F;
    }

    @Override
    public boolean canContinueToUse() {
        return lookTime >= 0;
    }

    @Override
    public void start() {
        double angle = (Math.PI * 2) * mob.getRandom().nextDouble();
        relX = Math.cos(angle);
        relZ = Math.sin(angle);
        lookTime = 20 + mob.getRandom().nextInt(20);
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        lookTime--;
        mob.getLookControl().setLookAt(mob.getX() + relX, mob.getEyeY(), mob.getZ() + relZ);
    }
}
