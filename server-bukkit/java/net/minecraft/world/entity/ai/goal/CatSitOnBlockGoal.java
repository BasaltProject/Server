// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.core.BlockPos;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.entity.animal.feline.Cat;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.BedBlock;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.FurnaceBlock;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BedPart;

public class CatSitOnBlockGoal extends MoveToBlockGoal {
    private final Cat cat;

    public CatSitOnBlockGoal(Cat cat, double speedModifier) {
        super(cat, speedModifier, 8);
        this.cat = cat;
    }

    @Override
    public boolean canUse() {
        return cat.isTame() && !cat.isOrderedToSit() && super.canUse();
    }

    @Override
    public void start() {
        super.start();
        cat.setInSittingPose(false);
    }

    @Override
    public void stop() {
        super.stop();
        cat.setInSittingPose(false);
    }

    @Override
    public void tick() {
        super.tick();
        cat.setInSittingPose(isReachedTarget());
    }

    @Override
    protected boolean isValidTarget(LevelReader level, BlockPos pos) {
        if (!level.isEmptyBlock(pos.above())) {
            return false;
        }
        BlockState state = level.getBlockState(pos);
        if (!state.is(BlockTags.CATS_CAN_SIT_ON)) {
            return false;
        }
        if (state.is(Blocks.CHEST)) {
            return ChestBlockEntity.getOpenCount(level, pos) < 1;
        }
        if (state.is(Blocks.FURNACE)) {
            return state.getValue(FurnaceBlock.LIT);
        }
        if (state.is(BlockTags.BEDS)) {
            return state.getOptionalValue(BedBlock.PART).map(part -> part != BedPart.HEAD).orElse(true);
        }
        return true;
    }
}
