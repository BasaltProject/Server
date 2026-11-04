// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.GoalUtils;
import net.minecraft.world.phys.Vec3;

public class FleeSunGoal extends Goal {
    protected final PathfinderMob mob;
    private double wantedX;
    private double wantedY;
    private double wantedZ;
    private final double speedModifier;

    public FleeSunGoal(PathfinderMob mob, double speedModifier) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        if (mob.getTarget() != null || !mob.level().isBrightOutside() || !mob.isOnFire()
                || !mob.level().canSeeSky(mob.blockPosition())
                || !mob.getItemBySlot(net.minecraft.world.entity.EquipmentSlot.HEAD).isEmpty()) {
            return false;
        }
        return setWantedPos();
    }

    protected boolean setWantedPos() {
        Vec3 pos = getHidePos();
        if (pos == null) {
            return false;
        }
        wantedX = pos.x;
        wantedY = pos.y;
        wantedZ = pos.z;
        return true;
    }

    @Override
    public boolean canContinueToUse() {
        return !mob.getNavigation().isDone();
    }

    @Override
    public void start() {
        mob.getNavigation().moveTo(wantedX, wantedY, wantedZ, speedModifier);
    }

    protected Vec3 getHidePos() {
        for (int i = 0; i < 10; i++) {
            BlockPos candidate = mob.blockPosition().offset(mob.getRandom().nextInt(20) - 10,
                    mob.getRandom().nextInt(6) - 3, mob.getRandom().nextInt(20) - 10);
            if (!mob.level().canSeeSky(candidate) && mob.getWalkTargetValue(candidate) < 0.0F) {
                return Vec3.atBottomCenterOf(candidate);
            }
        }
        return null;
    }
}
