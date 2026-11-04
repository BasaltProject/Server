// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.animal.feline.Cat;
import net.minecraft.world.level.LevelReader;

public class CatLieOnBlockGoal extends MoveToBlockGoal {
    private final Cat cat;

    public CatLieOnBlockGoal(Cat cat, double speedModifier, int searchRange) {
        super(cat, speedModifier, searchRange, 6);
        this.cat = cat;
        this.verticalSearchStart = -2;
        setFlags(EnumSet.of(Goal.Flag.JUMP, Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        return cat.isTame() && !cat.isOrderedToSit() && !cat.isLying() && super.canUse();
    }

    @Override
    public void start() {
        super.start();
        cat.setInSittingPose(false);
    }

    @Override
    protected int nextStartTick(PathfinderMob mob) {
        return 40;
    }

    @Override
    public void stop() {
        super.stop();
        cat.setLying(false);
    }

    @Override
    public void tick() {
        super.tick();
        cat.setInSittingPose(false);
        if (!isReachedTarget()) {
            cat.setLying(false);
        } else if (!cat.isLying()) {
            cat.setLying(true);
        }
    }

    @Override
    protected boolean isValidTarget(LevelReader level, BlockPos pos) {
        return level.isEmptyBlock(pos.above()) && level.getBlockState(pos).is(BlockTags.CATS_CAN_LIE_ON);
    }
}
