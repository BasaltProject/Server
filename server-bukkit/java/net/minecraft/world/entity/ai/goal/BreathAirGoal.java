// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.MoverType;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.pathfinder.PathComputationType;
import net.minecraft.world.phys.Vec3;

public class BreathAirGoal extends Goal {
    private final PathfinderMob mob;

    public BreathAirGoal(PathfinderMob mob) {
        this.mob = mob;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        return mob.getAirSupply() < 140;
    }

    @Override
    public boolean canContinueToUse() {
        return canUse();
    }

    @Override
    public boolean isInterruptable() {
        return false;
    }

    @Override
    public void start() {
        mob.getNavigation().stop();
        findAirPosition();
    }

    private void findAirPosition() {
        BlockPos destination = null;
        for (BlockPos pos : BlockPos.neighborColumn(mob.getBlockX(), mob.getBlockY(), mob.getBlockZ(), mob.getBlockY() + 8)) {
            if (givesAir(mob.level(), pos)) {
                destination = pos;
                break;
            }
        }
        if (destination == null) {
            destination = BlockPos.containing(mob.getX(), mob.getY() + 8.0, mob.getZ());
        }
        mob.getNavigation().moveTo(destination.getX(), destination.getY() + 1, destination.getZ(), 1.0);
    }

    @Override
    public void tick() {
        findAirPosition();
        mob.moveRelative(0.02F, new Vec3(mob.xxa, mob.yya, mob.zza));
        mob.move(MoverType.SELF, mob.getDeltaMovement());
    }

    private boolean givesAir(LevelReader level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        return (level.getFluidState(pos).isEmpty() || state.is(Blocks.BUBBLE_COLUMN)) && state.isPathfindable(PathComputationType.LAND);
    }
}
