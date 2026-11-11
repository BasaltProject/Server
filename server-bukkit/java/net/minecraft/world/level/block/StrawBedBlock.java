// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.Direction;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.state.properties.BedPart;
import java.util.OptionalDouble;
import net.minecraft.core.BlockPos;
import net.minecraft.resources.Identifier;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.attribute.BedRule;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class StrawBedBlock extends AbstractBedBlock {
    private void breakBed(Level level, BlockPos pos) {
        level.playSound((Entity) null, pos, SoundEvents.STRAW_BED_BREAK_LEAVE, SoundSource.BLOCKS, 1.0F, 1.0F);
        level.setBlockAndUpdate(pos, Blocks.AIR.defaultBlockState());
    }
    public StrawBedBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected EnvironmentAttribute<BedRule> getBedEnvironmentAttribute() {
        return net.minecraft.world.attribute.EnvironmentAttributes.STRAW_BED_RULE;
    }
    public void spawnDestroyParticles(Level level, BlockPos pos, BlockState state) {
        super.spawnDestroyParticles(level, pos, state);
    }
    public Identifier getSleptInBedStatType() {
        return net.minecraft.stats.Stats.SLEEP_IN_STRAW_BED;
    }
    public OptionalDouble getSleepHeight(BlockState state, Level level, BlockPos pos) {
        BlockPos restPos = pos;
        BlockState rest = state;
        if (state.getValue(BedBlock.PART) == BedPart.HEAD) {
            restPos = pos.relative(state.getValue(FACING).getOpposite());
            rest = level.getBlockState(restPos);
            if (!rest.is(this) || rest.getValue(BedBlock.PART) != BedPart.FOOT) {
                return OptionalDouble.empty();
            }
        }
        if (!rest.is(this)) {
            return OptionalDouble.empty();
        }
        VoxelShape bedShape = rest.getShape(level, restPos);
        return bedShape.isEmpty() ? OptionalDouble.empty() : OptionalDouble.of(bedShape.max(Direction.Axis.Y));
    }
    protected InteractionResult destroyOnUse(BlockState state, Level level, BlockPos pos, Player player) {
        breakBed(level, pos);
        return InteractionResult.SUCCESS_SERVER;
    }
    protected void destroyOnLeave(Level level, BlockPos pos) {
        breakBed(level, pos);


    }
}
