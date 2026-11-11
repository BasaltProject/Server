// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.util.Mth;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
public class SculkBlock extends DropExperienceBlock implements SculkBehaviour {
    private static int decayPenalty(SculkSpreader spreader, BlockPos pos, BlockPos originPos, int charge) {
        int free = spreader.noGrowthRadius();
        float outer = Mth.square((float) Math.sqrt(pos.distSqr(originPos)) - free);
        float reach = Mth.square(24 - free);
        return Math.max(1, (int) (charge * Math.min(1.0F, outer / reach) * 0.5F));
    }
    private static boolean roomForGrowth(LevelAccessor level, BlockPos pos) {
        BlockState above = level.getBlockState(pos.above());
        boolean open = above.isAir() || above.is(Blocks.WATER) && above.getFluidState().is(Fluids.WATER);
        if (!open) {
            return false;
        }
        int inhibitors = 0;
        for (BlockPos near : BlockPos.betweenClosed(pos.offset(-4, 0, -4), pos.offset(4, 2, 4))) {
            if (level.getBlockState(near).is(net.minecraft.tags.BlockTags.SCULK_GROWTH_INHIBITORS) && ++inhibitors > 2) {
                return false;
            }
        }
        return true;
    }
    private BlockState pickGrowth(LevelAccessor level, BlockPos pos, RandomSource random, boolean worldGen) {
        BlockState picked = random.nextInt(11) == 0
            ? Blocks.SCULK_SHRIEKER.defaultBlockState().setValue(SculkShriekerBlock.CAN_SUMMON, worldGen)
            : Blocks.SCULK_SENSOR.defaultBlockState();
        boolean wet = picked.hasProperty(BlockStateProperties.WATERLOGGED) && !level.getFluidState(pos).isEmpty();
        return wet ? picked.setValue(BlockStateProperties.WATERLOGGED, true) : picked;
    }
    public static final int GROWTH_INHIBITOR_RANGE = 4;
    public SculkBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.util.valueproviders.IntProvider) null, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public int attemptUseCharge(SculkSpreader.ChargeCursor cursor, LevelAccessor level, BlockPos originPos, RandomSource random, SculkSpreader spreader, boolean spreadVein) {
        int charge = cursor.getCharge();
        if (charge == 0 || random.nextInt(spreader.chargeDecayRate()) != 0) {
            return charge;
        }
        BlockPos at = cursor.getPos();
        boolean nearCatalyst = at.closerThan(originPos, spreader.noGrowthRadius());
        if (nearCatalyst || !roomForGrowth(level, at)) {
            if (random.nextInt(spreader.additionalDecayRate()) != 0) {
                return charge;
            }
            return charge - (nearCatalyst ? 1 : decayPenalty(spreader, at, originPos, charge));
        }
        int cost = spreader.growthSpawnCost();
        if (random.nextInt(cost) < charge) {
            BlockPos growthPos = at.above();
            BlockState growth = pickGrowth(level, growthPos, random, spreader.isWorldGeneration());
            level.setBlockAndUpdate(growthPos, growth);
            level.playSound((Entity) null, at, growth.getSoundType().getPlaceSound(), SoundSource.BLOCKS, 1.0F, 1.0F);
        }
        return Math.max(0, charge - cost);
    }
    public boolean canChangeBlockStateOnSpread() {
        return false;
    }
}
