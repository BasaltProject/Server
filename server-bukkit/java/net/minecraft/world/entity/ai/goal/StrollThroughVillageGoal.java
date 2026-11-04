// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.phys.Vec3;

public class StrollThroughVillageGoal extends Goal {
    private static final int DISTANCE_THRESHOLD = 10;
    private final PathfinderMob mob;
    private final int interval;
    private BlockPos wantedPos;

    public StrollThroughVillageGoal(PathfinderMob mob, int interval) {
        this.mob = mob;
        this.interval = reducedTickDelay(interval);
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        if (mob.hasControllingPassenger() || mob.level().isBrightOutside() || mob.getRandom().nextInt(interval) != 0) {
            return false;
        }
        ServerLevel level = (ServerLevel) mob.level();
        if (!level.isCloseToVillage(mob.blockPosition(), 6)) {
            return false;
        }
        Vec3 land = LandRandomPos.getPos(mob, 15, 7, pos -> -level.sectionsToVillage(SectionPos.of(pos)));
        wantedPos = land == null ? null : BlockPos.containing(land);
        return wantedPos != null;
    }

    @Override
    public boolean canContinueToUse() {
        return wantedPos != null && !mob.getNavigation().isDone() && wantedPos.equals(mob.getNavigation().getTargetPos());
    }

    @Override
    public void tick() {
        if (wantedPos == null) {
            return;
        }
        PathNavigation navigation = mob.getNavigation();
        if (navigation.isDone() && !wantedPos.closerToCenterThan(mob.position(), DISTANCE_THRESHOLD)) {
            Vec3 goal = Vec3.atBottomCenterOf(wantedPos);
            Vec3 self = mob.position();
            Vec3 shifted = self.subtract(goal).scale(0.4).add(goal);
            Vec3 aim = shifted.subtract(self).normalize().scale(DISTANCE_THRESHOLD).add(self);
            BlockPos target = mob.level().getHeightmapPos(Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, BlockPos.containing(aim));
            if (!navigation.moveTo(target.getX(), target.getY(), target.getZ(), 1.0)) {
                wanderRandomly();
            }
        }
    }

    private void wanderRandomly() {
        RandomSource random = mob.getRandom();
        BlockPos target = mob.level().getHeightmapPos(Heightmap.Types.MOTION_BLOCKING_NO_LEAVES,
                mob.blockPosition().offset(-8 + random.nextInt(16), 0, -8 + random.nextInt(16)));
        mob.getNavigation().moveTo(target.getX(), target.getY(), target.getZ(), 1.0);
    }
}
