// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.border.WorldBorder;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.shapes.VoxelShape;

public class PathNavigationRegion implements CollisionGetter {
    protected final Level level;

    public PathNavigationRegion(Level level, BlockPos start, BlockPos end) {
        this.level = level;
    }

    public WorldBorder getWorldBorder() {
        return level.getWorldBorder();
    }

    public BlockGetter getChunkForCollisions(int chunkX, int chunkZ) {
        return level;
    }

    public List<VoxelShape> getEntityCollisions(Entity source, AABB testArea) {
        return List.of();
    }

    public BlockEntity getBlockEntity(BlockPos pos) {
        return level.getBlockEntity(pos);
    }

    public BlockState getBlockState(BlockPos pos) {
        return level.getBlockState(pos);
    }

    public FluidState getFluidState(BlockPos pos) {
        return level.getFluidState(pos);
    }

    public int getMinY() {
        return level.getMinY();
    }

    public int getHeight() {
        return level.getHeight();
    }
}
