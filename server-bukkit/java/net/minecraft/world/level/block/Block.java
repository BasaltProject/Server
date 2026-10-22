package net.minecraft.world.level.block;

import net.minecraft.core.BlockPos;
import net.minecraft.core.IdMapper;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.material.MapColor;
import net.minecraft.world.phys.shapes.VoxelShape;
import org.basaltmc.bukkit.Unsupported;
public class Block extends BlockBehaviour implements ItemLike {
    protected Block() {
    }
    public static IdMapper BLOCK_STATE_REGISTRY;
    public BlockState defaultBlockState() {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.defaultBlockState");
    }
    public MapColor defaultMapColor() {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.defaultMapColor");
    }
    public String getDescriptionId() {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.getDescriptionId");
    }
    public float getExplosionResistance() {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.getExplosionResistance");
    }
    public float getFriction() {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.getFriction");
    }
    public static int getId(BlockState blockState) {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.getId");
    }
    public StateDefinition getStateDefinition() {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.getStateDefinition");
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.getStateForPlacement");
    }
    public static boolean isShapeFullBlock(VoxelShape shape) {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.isShapeFullBlock");
    }
    public static BlockState stateById(int idWithData) {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.stateById");
    }
    public static BlockState updateFromNeighbourShapes(BlockState state, LevelAccessor level, BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.block.Block.updateFromNeighbourShapes");
    }
}
