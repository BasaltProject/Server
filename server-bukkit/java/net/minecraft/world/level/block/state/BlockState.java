package net.minecraft.world.level.block.state;

import net.minecraft.core.BlockPos;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.shapes.VoxelShape;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.Effects;
import org.bukkit.craftbukkit.block.data.CraftBlockData;

public class BlockState extends BlockBehaviour.BlockStateBase {
    private static final int kBlocksMotion = 1;
    private static final int kOccludes = 2;
    private static final int kFullCube = 4;
    private static final int kReplaceable = 8;
    private static final int kHasCollision = 32;
    private static final int kTicksRandomly = 128;
    private static final int kAir = 256;
    private static final int kHasBlockEntity = 1 << 18;
    private static final PushReaction[] kReactions = {PushReaction.PUSH_PULL, PushReaction.POPPED,
        PushReaction.IMMOVEABLE, PushReaction.IGNORE_ENTITY, PushReaction.PUSH};
    private int flags = -1;

    public BlockState(Block block, int id) {
        super(block, id);
    }

    private int flags() {
        int found = flags;
        if (found < 0) {
            found = WorldNatives.stateFlags(stateId());
            flags = found;
        }
        return found;
    }

    public CraftBlockData asBlockData() {
        return CraftBlockData.createData(this);
    }

    public boolean isAir() {
        return (flags() & kAir) != 0;
    }

    public boolean canOcclude() {
        return (flags() & kOccludes) != 0;
    }

    public boolean isSolid() {
        return (flags() & kBlocksMotion) != 0;
    }

    public boolean canBeReplaced() {
        return (flags() & kReplaceable) != 0;
    }

    public boolean canBeReplaced(BlockPlaceContext context) {
        return canBeReplaced();
    }

    public boolean isRandomlyTicking() {
        return (flags() & kTicksRandomly) != 0;
    }

    public boolean hasBlockEntity() {
        return (flags() & kHasBlockEntity) != 0;
    }

    public int getLightEmission() {
        return (flags() >> 9) & 15;
    }

    public int getLightDampening() {
        return (flags() >> 13) & 31;
    }

    public boolean liquid() {
        String path = getBlock().blockKey().getPath();
        return path.equals("water") || path.equals("lava");
    }

    public FluidState getFluidState() {
        if ((flags() & 16) == 0) {
            return Fluids.EMPTY.defaultFluidState();
        }
        String path = getBlock().blockKey().getPath();
        return path.equals("lava") ? Fluids.LAVA.defaultFluidState() : Fluids.WATER.defaultFluidState();
    }

    public float getDestroySpeed(BlockGetter level, BlockPos pos) {
        return (float) getBlock().facts()[0];
    }

    public boolean requiresCorrectToolForDrops() {
        return getBlock().facts()[2] != 0.0;
    }

    public PushReaction getPistonPushReaction() {
        double[] found = WorldNatives.blockInfo(stateId());
        if (found == null || found.length < 4 || found[3] < 0 || found[3] >= kReactions.length) {
            return PushReaction.PUSH_PULL;
        }
        return kReactions[(int) found[3]];
    }

    public boolean isSignalSource() {
        return false;
    }

    public boolean ignitedByLava() {
        return false;
    }

    public boolean hasAnalogOutputSignal() {
        return false;
    }

    public VoxelShape getShape(BlockGetter level, BlockPos pos) {
        int bits = flags();
        return VoxelShape.of((bits & kFullCube) != 0, (bits & kHasCollision) == 0);
    }

    public BlockPos getPostProcessPos(BlockGetter level, BlockPos pos) {
        return pos;
    }

    public boolean canSurvive(LevelReader level, BlockPos pos) {
        return true;
    }

    public void onPlace(Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        Effects.placed(level, pos, oldState);
    }

    public void handleNeighborChanged(Level level, BlockPos pos, Block neighbor, Orientation orientation, boolean movedByPiston) {
        Effects.neighborChanged(level, pos, neighbor == null ? null : neighbor.defaultBlockState());
    }

    public boolean triggerEvent(Level level, BlockPos pos, int id, int param) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.triggerEvent");
    }

    public void updateNeighbourShapes(LevelAccessor level, BlockPos pos, int flags, int recursionLeft) {
        Effects.shapes(level, pos);
    }

    public void updateIndirectNeighbourShapes(LevelAccessor level, BlockPos pos, int flags, int recursionLeft) {
        Effects.indirectShapes(level, pos, this);
    }

    public InteractionResult useItemOn(ItemStack stack, Level level, Player player, InteractionHand hand,
            BlockHitResult hit) {
        return InteractionResult.PASS;
    }

    @Override
    public String toString() {
        return "Block{" + getBlock().blockKey() + "}" + WorldNatives.stateText(stateId()).replaceFirst("^[^\\[]*", "");
    }
}
