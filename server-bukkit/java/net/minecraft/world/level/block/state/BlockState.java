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
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.shapes.VoxelShape;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.craftbukkit.block.data.CraftBlockData;
public class BlockState extends BlockBehaviour.BlockStateBase {
    protected BlockState() {
    }
    public CraftBlockData asBlockData() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.asBlockData");
    }
    public boolean canBeReplaced() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.canBeReplaced");
    }
    public boolean canBeReplaced(BlockPlaceContext blockPlaceContext) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.canBeReplaced");
    }
    public boolean canOcclude() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.canOcclude");
    }
    public boolean canSurvive(LevelReader levelReader, BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.canSurvive");
    }
    public Block getBlock() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.getBlock");
    }
    public float getDestroySpeed(BlockGetter blockGetter, BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.getDestroySpeed");
    }
    public FluidState getFluidState() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.getFluidState");
    }
    public int getLightDampening() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.getLightDampening");
    }
    public int getLightEmission() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.getLightEmission");
    }
    public PushReaction getPistonPushReaction() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.getPistonPushReaction");
    }
    public BlockPos getPostProcessPos(BlockGetter blockGetter, BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.getPostProcessPos");
    }
    public VoxelShape getShape(BlockGetter blockGetter, BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.getShape");
    }
    public void handleNeighborChanged(Level level, BlockPos blockPos, Block block, Orientation orientation, boolean flag) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.handleNeighborChanged");
    }
    public boolean hasAnalogOutputSignal() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.hasAnalogOutputSignal");
    }
    public boolean hasBlockEntity() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.hasBlockEntity");
    }
    public boolean ignitedByLava() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.ignitedByLava");
    }
    public boolean isAir() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.isAir");
    }
    public boolean isRandomlyTicking() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.isRandomlyTicking");
    }
    public boolean isSignalSource() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.isSignalSource");
    }
    public boolean isSolid() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.isSolid");
    }
    public boolean liquid() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.liquid");
    }
    public void onPlace(Level level, BlockPos blockPos, BlockState blockState, boolean flag) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.onPlace");
    }
    public boolean requiresCorrectToolForDrops() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.requiresCorrectToolForDrops");
    }
    public Object setValue(Property property, Comparable comparable) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.setValue");
    }
    public boolean triggerEvent(Level level, BlockPos blockPos, int value, int value2) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.triggerEvent");
    }
    public void updateIndirectNeighbourShapes(LevelAccessor levelAccessor, BlockPos blockPos, int value, int value2) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.updateIndirectNeighbourShapes");
    }
    public void updateNeighbourShapes(LevelAccessor levelAccessor, BlockPos blockPos, int value, int value2) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.updateNeighbourShapes");
    }
    public InteractionResult useItemOn(ItemStack itemStack, Level level, Player player, InteractionHand interactionHand, BlockHitResult blockHitResult) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.BlockState.useItemOn");
    }
}
