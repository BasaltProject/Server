// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.tags.BlockTags;
import net.minecraft.tags.FluidTags;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.animal.dolphin.Dolphin;
import net.minecraft.world.phys.Vec3;

public class DolphinJumpGoal extends JumpGoal {
    private static final int[] kStepsToCheck = {0, 1, 4, 5, 6, 7};
    private final Dolphin dolphin;
    private final int interval;
    private boolean breached;

    public DolphinJumpGoal(Dolphin dolphin, int interval) {
        this.dolphin = dolphin;
        this.interval = reducedTickDelay(interval);
    }

    @Override
    public boolean canUse() {
        if (dolphin.getRandom().nextInt(interval) != 0) {
            return false;
        }
        Direction motion = dolphin.getMotionDirection();
        BlockPos origin = dolphin.blockPosition();
        for (int step : kStepsToCheck) {
            if (!waterIsClear(origin, motion.getStepX(), motion.getStepZ(), step)
                    || !surfaceIsClear(origin, motion.getStepX(), motion.getStepZ(), step)) {
                return false;
            }
        }
        return true;
    }

    private boolean waterIsClear(BlockPos origin, int stepX, int stepZ, int step) {
        BlockPos next = origin.offset(stepX * step, 0, stepZ * step);
        return dolphin.level().getFluidState(next).is(FluidTags.WATER) && !dolphin.level().getBlockState(next).is(BlockTags.BLOCKS_DOLPHIN_JUMP);
    }

    private boolean surfaceIsClear(BlockPos origin, int stepX, int stepZ, int step) {
        return dolphin.level().getBlockState(origin.offset(stepX * step, 1, stepZ * step)).isAir()
                && dolphin.level().getBlockState(origin.offset(stepX * step, 2, stepZ * step)).isAir();
    }

    @Override
    public boolean canContinueToUse() {
        double vy = dolphin.getDeltaMovement().y;
        boolean settled = vy * vy < 0.03F && dolphin.getXRot() != 0.0F && Math.abs(dolphin.getXRot()) < 10.0F && dolphin.isInWater();
        return !settled && !dolphin.onGround();
    }

    @Override
    public boolean isInterruptable() {
        return false;
    }

    @Override
    public void start() {
        Direction direction = dolphin.getMotionDirection();
        dolphin.setDeltaMovement(dolphin.getDeltaMovement().add(direction.getStepX() * 0.6, 0.7, direction.getStepZ() * 0.6));
        dolphin.getNavigation().stop();
    }

    @Override
    public void stop() {
        dolphin.setXRot(0.0F);
    }

    @Override
    public void tick() {
        boolean wasBreached = breached;
        if (!wasBreached) {
            breached = dolphin.level().getFluidState(dolphin.blockPosition()).is(FluidTags.WATER);
        }
        if (breached && !wasBreached) {
            dolphin.playSound(SoundEvents.DOLPHIN_JUMP, 1.0F, 1.0F);
        }
        Vec3 movement = dolphin.getDeltaMovement();
        if (movement.y * movement.y < 0.03F && dolphin.getXRot() != 0.0F) {
            dolphin.setXRot(Mth.rotLerp(0.2F, dolphin.getXRot(), 0.0F));
        } else if (movement.length() > 1.0E-5F) {
            dolphin.setXRot((float) (Math.atan2(-movement.y, movement.horizontalDistance()) * (180.0F / (float) Math.PI)));
        }
    }
}
