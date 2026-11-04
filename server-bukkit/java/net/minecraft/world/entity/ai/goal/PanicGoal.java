// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.tags.TagKey;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.phys.Vec3;

public class PanicGoal extends Goal {
    public static final int WATER_CHECK_DISTANCE_VERTICAL = 1;
    protected final PathfinderMob mob;
    protected final double speedModifier;
    protected double posX;
    protected double posY;
    protected double posZ;
    protected boolean isRunning;

    public PanicGoal(PathfinderMob mob, double speedModifier) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    public PanicGoal(PathfinderMob mob, double speedModifier, TagKey<DamageType> panicCausingDamageTypes) {
        this(mob, speedModifier);
    }

    public PanicGoal(PathfinderMob mob, double speedModifier, Function<PathfinderMob, TagKey<DamageType>> panicCausingDamageTypes) {
        this(mob, speedModifier);
    }

    @Override
    public boolean canUse() {
        return shouldPanic() && findRandomPosition();
    }

    protected boolean shouldPanic() {
        return mob.getLastHurtByMob() != null || mob.isOnFire();
    }

    protected boolean findRandomPosition() {
        Vec3 target = LandRandomPos.getPos(mob, 5, 4);
        if (target == null) {
            return false;
        }
        posX = target.x;
        posY = target.y;
        posZ = target.z;
        return true;
    }

    public boolean isRunning() {
        return isRunning;
    }

    @Override
    public void start() {
        mob.getNavigation().moveTo(posX, posY, posZ, speedModifier);
        isRunning = true;
    }

    @Override
    public void stop() {
        isRunning = false;
    }

    @Override
    public boolean canContinueToUse() {
        return !mob.getNavigation().isDone();
    }

    protected BlockPos lookForWater(LevelReader level, Entity mob, int xzDist) {
        return null;
    }
}
