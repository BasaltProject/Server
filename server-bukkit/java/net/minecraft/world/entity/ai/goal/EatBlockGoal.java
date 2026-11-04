// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;

public class EatBlockGoal extends Goal {
    private static final int EAT_ANIMATION_TICKS = 40;
    private final Mob mob;
    private int eatAnimationTick;

    public EatBlockGoal(Mob mob) {
        this.mob = mob;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK, Goal.Flag.JUMP));
    }

    @Override
    public boolean canUse() {
        if (mob.getRandom().nextInt(mob.isBaby() ? 50 : 1000) != 0) {
            return false;
        }
        BlockPos pos = mob.blockPosition();
        return mob.level().getBlockState(pos).is(Blocks.SHORT_GRASS) || mob.level().getBlockState(pos.below()).is(Blocks.GRASS_BLOCK);
    }

    @Override
    public void start() {
        eatAnimationTick = adjustedTickDelay(EAT_ANIMATION_TICKS);
        mob.level().broadcastEntityEvent(mob, (byte) 10);
        mob.getNavigation().stop();
    }

    @Override
    public void stop() {
        eatAnimationTick = 0;
    }

    @Override
    public boolean canContinueToUse() {
        return eatAnimationTick > 0;
    }

    public int getEatAnimationTick() {
        return eatAnimationTick;
    }

    @Override
    public void tick() {
        eatAnimationTick = Math.max(0, eatAnimationTick - 1);
        if (eatAnimationTick != adjustedTickDelay(4)) {
            return;
        }
        BlockPos pos = mob.blockPosition();
        BlockState state = mob.level().getBlockState(pos);
        if (state.is(Blocks.SHORT_GRASS)) {
            mob.level().destroyBlock(pos, false);
            mob.ate();
        } else {
            BlockPos below = pos.below();
            if (mob.level().getBlockState(below).is(Blocks.GRASS_BLOCK)) {
                mob.level().levelEvent(2001, below, net.minecraft.world.level.block.Block.getId(Blocks.GRASS_BLOCK.defaultBlockState()));
                mob.level().setBlock(below, Blocks.DIRT.defaultBlockState(), 2);
                mob.ate();
            }
        }
    }
}
