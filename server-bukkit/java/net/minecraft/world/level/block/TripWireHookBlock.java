// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.Optional;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.entity.Entity;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.level.redstone.ExperimentalRedstoneUtils;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class TripWireHookBlock extends Block {
    private static void announce(Level lv, BlockPos at, boolean att, boolean pow, boolean wasAtt, boolean wasPow) {
        SoundEvent snd = null;
        net.minecraft.core.Holder<GameEvent> evt = null;
        float volume = 0.4F;
        float pitch = 1.0F;
        if (pow != wasPow) {
            snd = pow ? SoundEvents.TRIPWIRE_CLICK_ON : SoundEvents.TRIPWIRE_CLICK_OFF;
            evt = pow ? GameEvent.BLOCK_ACTIVATE : GameEvent.BLOCK_DEACTIVATE;
            pitch = pow ? 0.6F : 0.5F;
        } else if (att != wasAtt) {
            snd = att ? SoundEvents.TRIPWIRE_ATTACH : SoundEvents.TRIPWIRE_DETACH;
            evt = att ? GameEvent.BLOCK_ATTACH : GameEvent.BLOCK_DETACH;
            pitch = att ? 0.7F : 1.2F / (lv.getRandom().nextFloat() * 0.2F + 0.9F);
        }
        if (snd != null) {
            lv.playSound((Entity) null, at, snd, SoundSource.BLOCKS, volume, pitch);
            lv.gameEvent((Entity) null, evt, at);
        }
    }
    private static void poke(Block self, Level lv, BlockPos at, Direction facing) {
        Direction back = facing.getOpposite();
        Orientation o = ExperimentalRedstoneUtils.initialOrientation(lv, back, Direction.UP);
        lv.updateNeighborsAt(at, self, o);
        lv.updateNeighborsAt(at.relative(back), self, o);
    }
    private static void afterRemoval(BlockState st, Level lv, BlockPos at) {
        boolean att = st.getValue(ATTACHED);
        boolean pow = st.getValue(POWERED);
        if (att || pow) {
            calculateState(lv, at, st, true, false, -1, null);
        }
        if (pow) {
            poke(st.getBlock(), lv, at, st.getValue(FACING));
        }
    }
    public static final EnumProperty<Direction> FACING = HorizontalDirectionalBlock.FACING;
    public static final BooleanProperty POWERED = net.minecraft.world.level.block.state.properties.BlockStateProperties.POWERED;
    public static final BooleanProperty ATTACHED = net.minecraft.world.level.block.state.properties.BlockStateProperties.ATTACHED;
    protected static final int WIRE_DIST_MIN = 1;
    protected static final int WIRE_DIST_MAX = 42;
    public TripWireHookBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    public void setPlacedBy(Level level, BlockPos pos, BlockState state, LivingEntity by, ItemStack itemStack) {
        super.setPlacedBy(level, pos, state, by, itemStack);
    }
    public static void calculateState(Level level, BlockPos pos, BlockState state, boolean isBeingDestroyed, boolean canUpdate, int wireSource, BlockState wireSourceState) {
        if (state.getOptionalValue(FACING).isEmpty()) {
            return;
        }
        Direction dir = state.getValue(FACING);
        boolean oldAtt = state.getOptionalValue(ATTACHED).orElse(false);
        boolean oldPow = state.getOptionalValue(POWERED).orElse(false);
        Block self = state.getBlock();
        boolean att = !isBeingDestroyed;
        boolean pow = false;
        int partner = 0;
        BlockState[] line = new BlockState[WIRE_DIST_MAX];
        for (int step = 1; step < WIRE_DIST_MAX; step++) {
            BlockState seen = level.getBlockState(pos.relative(dir, step));
            if (seen.is(Blocks.TRIPWIRE_HOOK)) {
                partner = seen.getValue(FACING) == dir.getOpposite() ? step : partner;
                break;
            }
            boolean source = step == wireSource;
            if (!source && !seen.is(Blocks.TRIPWIRE)) {
                att = false;
                continue;
            }
            seen = source && wireSourceState != null ? wireSourceState : seen;
            boolean armed = !seen.getValue(TripWireBlock.DISARMED);
            pow |= armed && seen.getValue(TripWireBlock.POWERED);
            line[step] = seen;
            if (source) {
                level.scheduleTick(pos, self, 10);
                att &= armed;
            }
        }
        att &= partner > 1;
        pow &= att;
        BlockState next = self.defaultBlockState().trySetValue(ATTACHED, att).trySetValue(POWERED, pow);
        if (partner > 0) {
            BlockPos far = pos.relative(dir, partner);
            level.setBlockAndUpdate(far, next.setValue(FACING, dir.getOpposite()));
            poke(self, level, far, dir.getOpposite());
            if (!level.getBlockState(pos).is(Blocks.TRIPWIRE_HOOK)) {
                afterRemoval(next, level, pos);
                return;
            }
            announce(level, far, att, pow, oldAtt, oldPow);
        }
        announce(level, pos, att, pow, oldAtt, oldPow);
        if (!isBeingDestroyed) {
            level.setBlockAndUpdate(pos, next.setValue(FACING, dir));
            if (canUpdate) {
                poke(self, level, pos, dir);
            }
        }
        if (oldAtt == att) {
            return;
        }
        for (int step = 1; step < partner; step++) {
            BlockPos at = pos.relative(dir, step);
            BlockState cur = level.getBlockState(at);
            if (line[step] != null && (cur.is(Blocks.TRIPWIRE) || cur.is(Blocks.TRIPWIRE_HOOK))) {
                level.setBlockAndUpdate(at, line[step].trySetValue(ATTACHED, att));
            }
        }


    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected void affectNeighborsAfterRemoval(BlockState state, ServerLevel level, BlockPos pos, boolean movedByPiston) {
        super.affectNeighborsAfterRemoval(state, level, pos, movedByPiston);
    }
    protected int ownSignal(BlockState state, BlockGetter level, BlockPos pos) {
        return super.ownSignal(state, level, pos);
    }
    protected int getDirectSignal(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        return super.getDirectSignal(state, level, pos, direction);
    }
    protected boolean isSignalSource(BlockState state) {
        return super.isSignalSource(state);
    }
    protected BlockState rotate(BlockState state, Rotation rotation) {
        return super.rotate(state, rotation);
    }
    protected BlockState mirror(BlockState state, Mirror mirror) {
        return super.mirror(state, mirror);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
}
