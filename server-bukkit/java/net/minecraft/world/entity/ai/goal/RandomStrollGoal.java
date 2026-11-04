// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.world.phys.Vec3;

public class RandomStrollGoal extends Goal {
    public static final int DEFAULT_INTERVAL = 120;
    protected final PathfinderMob mob;
    protected double wantedX;
    protected double wantedY;
    protected double wantedZ;
    protected final double speedModifier;
    protected int interval;
    protected boolean forceTrigger;
    private final boolean checkNoActionTime;

    public RandomStrollGoal(PathfinderMob mob, double speedModifier) {
        this(mob, speedModifier, DEFAULT_INTERVAL);
    }

    public RandomStrollGoal(PathfinderMob mob, double speedModifier, int interval) {
        this(mob, speedModifier, interval, true);
    }

    public RandomStrollGoal(PathfinderMob mob, double speedModifier, int interval, boolean checkNoActionTime) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        this.interval = interval;
        this.checkNoActionTime = checkNoActionTime;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        if (mob.isVehicle()) {
            return false;
        }
        if (!forceTrigger) {
            if (mob.getRandom().nextInt(adjustedTickDelay(interval)) != 0) {
                return false;
            }
        }
        Vec3 position = getPosition();
        if (position == null) {
            return false;
        }
        wantedX = position.x;
        wantedY = position.y;
        wantedZ = position.z;
        forceTrigger = false;
        return true;
    }

    protected Vec3 getPosition() {
        return DefaultRandomPos.getPos(mob, 10, 7);
    }

    @Override
    public boolean canContinueToUse() {
        return !mob.getNavigation().isDone() && !mob.isVehicle();
    }

    @Override
    public void start() {
        mob.getNavigation().moveTo(wantedX, wantedY, wantedZ, speedModifier);
    }

    @Override
    public void stop() {
        mob.getNavigation().stop();
    }

    public void trigger() {
        forceTrigger = true;
    }

    public void setInterval(int interval) {
        this.interval = interval;
    }
}
