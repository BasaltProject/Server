// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.piston;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.nbt.NbtOps;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.Mth;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.level.block.state.properties.PistonType;
import org.basaltmc.bukkit.facade.ValueIo;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.shapes.VoxelShape;
public class PistonMovingBlockEntity extends BlockEntity {
    private static final ThreadLocal<Direction> kNoClip = ThreadLocal.withInitial(() -> null);
    public PistonMovingBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }
    private void seed(BlockState moved, Direction facing, boolean extending, boolean source) {
        CompoundTag tag = nbt();
        BlockState.CODEC.encodeStart(NbtOps.INSTANCE, moved).result().ifPresent(encoded -> tag.put("blockState", encoded));
        tag.putByte("facing", (byte) facing.get3DDataValue());
        tag.putFloat("progress", 0.0F);
        tag.putBoolean("extending", extending);
        tag.putBoolean("source", source);
        load(tag);
    }
    private float progress() {
        return nbt().getFloatOr("progress", 0.0F);
    }
    private float extendedProgress(float progress) {
        return isExtending() ? progress - 1.0F : 1.0F - progress;
    }
    public static final double TICK_MOVEMENT = 0.51;
    public PistonMovingBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("piston"), worldPosition, blockState);
    }
    public PistonMovingBlockEntity(BlockPos worldPosition, BlockState blockState, BlockState movedState, Direction direction, boolean extending, boolean isSourcePiston) {
        this(worldPosition, blockState);
        seed(movedState, direction, extending, isSourcePiston);
    }
    public CompoundTag getUpdateTag(HolderLookup.Provider registries) {
        return saveCustomOnly(registries);
    }
    public boolean isExtending() {
        return nbt().getBooleanOr("extending", false);
    }
    public Direction getDirection() {
        return Direction.from3DDataValue(nbt().getByteOr("facing", (byte) 0));
    }
    public boolean isSourcePiston() {
        return nbt().getBooleanOr("source", false);
    }
    public float getProgress(float a) {
        return Mth.lerp(Math.min(a, 1.0F), progress(), progress());
    }
    public float getXOff(float a) {
        return getDirection().getStepX() * extendedProgress(getProgress(a));
    }
    public float getYOff(float a) {
        return getDirection().getStepY() * extendedProgress(getProgress(a));
    }
    public float getZOff(float a) {
        return getDirection().getStepZ() * extendedProgress(getProgress(a));
    }
    public Direction getMovementDirection() {
        return getPushDirection();
    }
    public BlockState getMovedState() {
        net.minecraft.nbt.Tag stored = nbt().get("blockState");
        return stored == null ? Blocks.AIR.defaultBlockState() : BlockState.CODEC.parse(NbtOps.INSTANCE, stored).result().orElse(Blocks.AIR.defaultBlockState());
    }
    public void finalTick() {
        return;
    }
    public void preRemoveSideEffects(BlockPos pos, BlockState state) {
        return;
    }
    public Direction getPushDirection() {
        return isExtending() ? getDirection() : getDirection().getOpposite();
    }
    public static void tick(Level level, BlockPos pos, BlockState state, PistonMovingBlockEntity entity) {
        return;
    }
    protected void loadAdditional(ValueInput input) {
        load(ValueIo.read(input));
    }
    protected void saveAdditional(ValueOutput output) {
        ValueIo.write(output, saveWithoutMetadata((HolderLookup.Provider) null));
    }
    public VoxelShape getCollisionShape(BlockGetter level, BlockPos pos) {
        BlockState moved = getMovedState();
        boolean retracting = !isExtending() && isSourcePiston() && moved.getBlock() instanceof PistonBaseBlock;
        VoxelShape headShape = retracting ? moved.setValue(PistonBaseBlock.EXTENDED, true).getCollisionShape(level, pos) : Shapes.empty();
        float progress = progress();
        if (progress < 1.0F && kNoClip.get() == getMovementDirection()) {
            return headShape;
        }
        BlockState shown = moved;
        if (isSourcePiston()) {
            shown = Blocks.PISTON_HEAD.defaultBlockState().setValue(PistonHeadBlock.FACING, getDirection())
                .setValue(PistonHeadBlock.SHORT, isExtending() != 1.0F - progress < 0.25F);
        }
        float shift = extendedProgress(progress);
        return Shapes.or(headShape, shown.getCollisionShape(level, pos)
            .move(getDirection().getStepX() * shift, getDirection().getStepY() * shift, getDirection().getStepZ() * shift));
    }
    public long getLastTicked() {
        return nbt().getLongOr("last_ticked", 0L);
    }
    public void setLevel(Level level) {
        super.setLevel(level);


    }
}
