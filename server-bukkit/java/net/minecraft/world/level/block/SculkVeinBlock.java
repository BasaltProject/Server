// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.util.Mth;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.Fluids;
import java.util.Collection;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class SculkVeinBlock extends MultifaceSpreadeableBlock implements SculkBehaviour {
    private MultifaceSpreader veinSpreader;
    private MultifaceSpreader sameSpaceSpreader;
    private boolean placeSculkFor(SculkSpreader spreader, LevelAccessor level, BlockPos pos, RandomSource random) {
        BlockState vein = level.getBlockState(pos);
        for (Direction support : Direction.allShuffled(random)) {
            BlockPos supportPos = pos.relative(support);
            BlockState supportState = level.getBlockState(supportPos);
            if (!hasFace(vein, support) || !supportState.is(spreader.replaceableBlocks())) {
                continue;
            }
            BlockState sculk = Blocks.SCULK.defaultBlockState();
            level.setBlockAndUpdate(supportPos, sculk);
            Block.pushEntitiesUp(supportState, sculk, level, supportPos);
            level.playSound((Entity) null, supportPos, SoundEvents.SCULK_BLOCK_SPREAD, SoundSource.BLOCKS, 1.0F, 1.0F);
            getSpreader().spreadAll(sculk, level, supportPos, spreader.isWorldGeneration());
            for (Direction around : DIRECTIONS) {
                BlockPos other = supportPos.relative(around);
                BlockState otherState = level.getBlockState(other);
                if (around != support.getOpposite() && otherState.is(this)) {
                    onDischarged(level, otherState, other, random);
                }
            }
            return true;
        }
        return false;
    }
    public SculkVeinBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public MultifaceSpreader getSpreader() {
        if (veinSpreader == null) {
            veinSpreader = new MultifaceSpreader(new SculkVeinSpreaderConfig(MultifaceSpreader.SpreadType.SAME_POSITION, MultifaceSpreader.SpreadType.SAME_PLANE, MultifaceSpreader.SpreadType.WRAP_AROUND));
        }
        return veinSpreader;
    }
    public MultifaceSpreader getSameSpaceSpreader() {
        if (sameSpaceSpreader == null) {
            sameSpaceSpreader = new MultifaceSpreader(new SculkVeinSpreaderConfig(MultifaceSpreader.SpreadType.SAME_POSITION));
        }
        return sameSpaceSpreader;
    }
    public static boolean regrow(LevelAccessor level, BlockPos pos, BlockState existing, Collection<Direction> faces) {
        BlockState grown = Blocks.SCULK_VEIN.defaultBlockState();
        boolean any = false;
        for (Direction face : faces) {
            if (canAttachTo(level, pos, face)) {
                grown = grown.setValue(getFaceProperty(face), true);
                any = true;
            }
        }
        if (!any) {
            return false;
        }
        if (!existing.getFluidState().isEmpty()) {
            grown = grown.setValue(MultifaceBlock.WATERLOGGED, true);
        }
        level.setBlockAndUpdate(pos, grown);
        return true;
    }
    public void onDischarged(LevelAccessor level, BlockState state, BlockPos pos, RandomSource random) {
        if (!state.is(this)) {
            return;
        }
        BlockState left = state;
        for (Direction side : DIRECTIONS) {
            BooleanProperty prop = getFaceProperty(side);
            if (left.getValue(prop) && level.getBlockState(pos.relative(side)).is(Blocks.SCULK)) {
                left = left.setValue(prop, false);
            }
        }
        if (!hasAnyFace(left)) {
            left = (level.getFluidState(pos).isEmpty() ? Blocks.AIR : Blocks.WATER).defaultBlockState();
        }
        level.setBlockAndUpdate(pos, left);
    }
    public int attemptUseCharge(SculkSpreader.ChargeCursor cursor, LevelAccessor level, BlockPos originPos, RandomSource random, SculkSpreader spreader, boolean spreadVeins) {
        if (spreadVeins && placeSculkFor(spreader, level, cursor.getPos(), random)) {
            return cursor.getCharge() - 1;
        }
        return random.nextInt(spreader.chargeDecayRate()) == 0 ? Mth.floor(cursor.getCharge() * 0.5F) : cursor.getCharge();
    }
    public static boolean hasSubstrateAccess(LevelAccessor level, BlockState state, BlockPos pos) {
        if (!state.is(Blocks.SCULK_VEIN)) {
            return false;
        }
        for (Direction side : DIRECTIONS) {
            if (hasFace(state, side) && level.getBlockState(pos.relative(side)).is(net.minecraft.tags.BlockTags.SCULK_REPLACEABLE)) {
                return true;
            }
        }
        return false;
    }
    class SculkVeinSpreaderConfig extends MultifaceSpreader.DefaultSpreaderConfig {
        private final MultifaceSpreader.SpreadType[] types;
        public SculkVeinSpreaderConfig(MultifaceSpreader.SpreadType... spreadTypes) {
            super(SculkVeinBlock.this);
            this.types = spreadTypes;
        }
        public boolean stateCanBeReplaced(BlockGetter level, BlockPos sourcePos, BlockPos placementPos, Direction placementDirection, BlockState existingState) {
            BlockState behind = level.getBlockState(placementPos.relative(placementDirection));
            if (behind.is(Blocks.SCULK) || behind.is(Blocks.SCULK_CATALYST) || behind.is(Blocks.MOVING_PISTON)) {
                return false;
            }
            if (sourcePos.distManhattan(placementPos) == 2) {
                BlockPos back = sourcePos.relative(placementDirection.getOpposite());
                if (level.getBlockState(back).isFaceSturdy(level, back, placementDirection)) {
                    return false;
                }
            }
            FluidState fluid = existingState.getFluidState();
            if (!fluid.isEmpty() && !fluid.is(Fluids.WATER) || existingState.is(net.minecraft.tags.BlockTags.FIRE)) {
                return false;
            }
            return existingState.canBeReplaced() || super.stateCanBeReplaced(level, sourcePos, placementPos, placementDirection, existingState);
        }
        public MultifaceSpreader.SpreadType[] getSpreadTypes() {
            return types;
        }
        public boolean isOtherBlockValidAsSource(BlockState state) {
            return !state.is(Blocks.SCULK_VEIN);
        }
    }
}
