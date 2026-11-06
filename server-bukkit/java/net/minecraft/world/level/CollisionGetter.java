// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.border.WorldBorder;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public interface CollisionGetter extends BlockGetter {
    WorldBorder getWorldBorder();
    BlockGetter getChunkForCollisions(int chunkX, int chunkZ);
    default boolean isUnobstructed(Entity source, VoxelShape shape) {
        return true;
    }
    default boolean isUnobstructed(BlockState state, BlockPos pos, CollisionContext context) {
        VoxelShape shape = state.getCollisionShape(this, pos, context);
        return shape.isEmpty() || isUnobstructed(null, shape.move(pos));
    }
    default boolean isUnobstructed(Entity ignore) {
        return true;
    }
    default boolean noCollision(AABB aabb) {
        return noBlockCollision(null, aabb);
    }
    default boolean noCollision(Entity source) {
        return noCollision(source, source.getBoundingBox());
    }
    default boolean noCollision(Entity entity, AABB aabb) {
        return noBlockCollision(entity, aabb);
    }
    default boolean noCollision(Entity entity, AABB aabb, boolean alwaysCollideWithFluids) {
        return noBlockCollision(entity, aabb, alwaysCollideWithFluids) && noEntityCollision(entity, aabb)
            && noBorderCollision(entity, aabb);
    }
    default boolean noBlockCollision(Entity entity, AABB aabb) {
        return !getBlockCollisions(entity, aabb).iterator().hasNext();
    }
    default boolean noBlockCollision(Entity entity, AABB aabb, boolean alwaysCollideWithFluids) {

        Iterable<VoxelShape> hits = alwaysCollideWithFluids ? getBlockAndLiquidCollisions(entity, aabb) : getBlockCollisions(entity, aabb);
        return !hits.iterator().hasNext();
    }
    default boolean noEntityCollision(Entity entity, AABB aabb) {
        return true;
    }
    default boolean noBorderCollision(Entity entity, AABB aabb) {
        return true;
    }
    default List<VoxelShape> getEntityCollisions(Entity source, AABB testArea) {
        return java.util.List.of();
    }
    default Iterable<VoxelShape> getCollisions(Entity source, AABB box) {
        return getBlockCollisions(source, box);
    }
    default Iterable<VoxelShape> getPreMoveCollisions(Entity source, AABB box, Vec3 oldPos) {
        return getBlockCollisions(source, box);
    }
    default Iterable<VoxelShape> getBlockCollisions(Entity source, AABB box) {
        java.util.List<VoxelShape> out = new java.util.ArrayList<>();
        for (int x = (int) Math.floor(box.minX); x <= (int) Math.floor(box.maxX); x++) {
            for (int y = (int) Math.floor(box.minY); y <= (int) Math.floor(box.maxY); y++) {
                for (int z = (int) Math.floor(box.minZ); z <= (int) Math.floor(box.maxZ); z++) {
                    BlockPos pos = new BlockPos(x, y, z);
                    VoxelShape shape = getBlockState(pos).getCollisionShape((BlockGetter) this, pos);
                    if (shape.isEmpty()) {
                        continue;
                    }
                    VoxelShape moved = shape.move(x, y, z);
                    for (AABB part : moved.toAabbs()) {
                        if (part.intersects(box)) {
                            out.add(moved);
                            break;
                        }
                    }
                }
            }
        }
        return out;
    }
    default Iterable<VoxelShape> getBlockAndLiquidCollisions(Entity source, AABB box) {
        return getBlockCollisions(source, box);
    }
    default Iterable<VoxelShape> getBlockCollisionsFromContext(CollisionContext source, AABB box) {
        return getBlockCollisions(null, box);
    }
    default BlockHitResult clipIncludingBorder(ClipContext c) {
        return clip(c);
    }
    default boolean collidesWithSuffocatingBlock(Entity source, AABB box) {
        for (VoxelShape shape : getBlockCollisions(source, box)) {
            if (!shape.isEmpty()) {
                return true;
            }
        }
        return false;
    }
    default Optional<BlockPos> findSupportingBlock(Entity source, AABB box) {
        BlockPos best = null;
        double bestDistance = Double.MAX_VALUE;
        for (BlockPos pos : BlockPos.betweenClosed(box)) {
            VoxelShape shape = getBlockState(pos).getCollisionShape(this, pos);
            boolean touching = false;
            for (AABB part : shape.move(pos).toAabbs()) {
                touching |= part.intersects(box);
            }
            if (!touching) {
                continue;
            }
            double distance = pos.distToCenterSqr(source.position());
            if (distance < bestDistance || (distance == bestDistance && (best == null || best.compareTo(pos) < 0))) {
                best = pos.immutable();
                bestDistance = distance;
            }
        }
        return Optional.ofNullable(best);
    }
    default Optional<Vec3> findFreePosition(Entity source, VoxelShape allowedCenters, Vec3 preferredCenter, double sizeX, double sizeY, double sizeZ) {
        if (allowedCenters.isEmpty()) {
            return Optional.empty();
        }
        AABB searchArea = allowedCenters.bounds().inflate(sizeX, sizeY, sizeZ);
        VoxelShape blocked = net.minecraft.world.phys.shapes.Shapes.empty();
        for (VoxelShape shape : getBlockCollisions(source, searchArea)) {
            if (getWorldBorder() != null && !getWorldBorder().isWithinBounds(shape.bounds())) {
                continue;
            }
            for (AABB part : shape.toAabbs()) {
                blocked = net.minecraft.world.phys.shapes.Shapes.or(blocked, net.minecraft.world.phys.shapes.Shapes.create(part.inflate(sizeX / 2.0, sizeY / 2.0, sizeZ / 2.0)));
            }
        }
        return net.minecraft.world.phys.shapes.Shapes.join(allowedCenters, blocked, net.minecraft.world.phys.shapes.BooleanOp.ONLY_FIRST).closestPointTo(preferredCenter);
    }
}
