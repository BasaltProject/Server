// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.shapes.VoxelShape;
public interface CommonLevelAccessor extends LevelReader, LevelSimulatedRW, EntityGetter {
    default <T extends BlockEntity> Optional<T> getBlockEntity(BlockPos pos, BlockEntityType<T> type) {
        return LevelReader.super.getBlockEntity(pos, type);
    }
    default List<VoxelShape> getEntityCollisions(Entity source, AABB testArea) {
        if (testArea.getSize() < 1.0E-7) {
            return List.of();
        }
        List<net.minecraft.world.phys.shapes.VoxelShape> shapes = new java.util.ArrayList<>();
        for (Entity other : getEntities(source, testArea.inflate(1.0E-7), e -> true)) {
            if (source != null ? !source.canCollideWith(other) : !other.canBeCollidedWith(null)) {
                continue;
            }
            shapes.add(net.minecraft.world.phys.shapes.VoxelShape.ofBoxes(List.of(other.getBoundingBox())));
        }
        return shapes;
    }
    default boolean isUnobstructed(Entity source, VoxelShape shape) {
        return EntityGetter.super.isUnobstructed(source, shape);
    }
    default BlockPos getHeightmapPos(Heightmap.Types type, BlockPos pos) {
        return LevelReader.super.getHeightmapPos(type, pos);
    }
}
