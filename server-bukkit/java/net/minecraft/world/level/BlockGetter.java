// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.util.Mth;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.function.BiFunction;
import java.util.function.Function;
import java.util.function.Supplier;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;
public interface BlockGetter extends LevelHeightAccessor {
    default BlockEntity getBlockEntity(BlockPos pos) {
        return null;
    }
    @SuppressWarnings("unchecked")
    default <T extends BlockEntity> Optional<T> getBlockEntity(BlockPos pos, BlockEntityType<T> type) {
        BlockEntity found = getBlockEntity(pos);
        return found != null && found.getType() == type ? Optional.of((T) found) : Optional.empty();
    }
    default BlockState getBlockState(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.BlockGetter#getBlockState");
    }
    default FluidState getFluidState(BlockPos pos) {
        return getBlockState(pos).getFluidState();
    }
    default int getLightEmission(BlockPos pos) {
        return getBlockState(pos).getLightEmission();
    }
    default Stream<BlockState> getBlockStates(AABB box) {
        int x0 = (int) Math.floor(box.minX);
        int y0 = (int) Math.floor(box.minY);
        int z0 = (int) Math.floor(box.minZ);
        int x1 = (int) Math.floor(box.maxX);
        int y1 = (int) Math.floor(box.maxY);
        int z1 = (int) Math.floor(box.maxZ);
        List<BlockState> found = new ArrayList<>();
        for (int y = y0; y <= y1; y++) {
            for (int z = z0; z <= z1; z++) {
                for (int x = x0; x <= x1; x++) {
                    found.add(getBlockState(new BlockPos(x, y, z)));
                }
            }
        }
        return found.stream();
    }
    default BlockHitResult isBlockInLine(ClipBlockStateContext c) {
        Vec3 from = c.getFrom();
        Vec3 to = c.getTo();
        net.minecraft.core.Direction away = net.minecraft.core.Direction.getApproximateNearest(from.subtract(to));
        return traverseBlocks(from, to, c, (context, pos) -> context.isTargetBlock().test(getBlockState(pos))
            ? new BlockHitResult(Vec3.atCenterOf(pos), away, pos, false) : null,
            context -> BlockHitResult.miss(context.getTo(), away, BlockPos.containing(context.getTo())));
    }
    default BlockHitResult clip(ClipContext c) {
        Vec3 from = c.getFrom();
        Vec3 to = c.getTo();
        Vec3 delta = to.subtract(from);
        double length = delta.length();
        net.minecraft.core.Direction nearest = net.minecraft.core.Direction.getApproximateNearest(delta.x, delta.y, delta.z);
        if (!(this instanceof net.minecraft.server.level.ServerLevel level) || length < 1.0E-7) {
            return BlockHitResult.miss(to, nearest, BlockPos.containing(to));
        }
        org.bukkit.FluidCollisionMode fluids = switch (c.fluidMode()) {
            case NONE -> org.bukkit.FluidCollisionMode.NEVER;
            case SOURCE_ONLY -> org.bukkit.FluidCollisionMode.SOURCE_ONLY;
            default -> org.bukkit.FluidCollisionMode.ALWAYS;
        };
        org.bukkit.util.RayTraceResult found = level.getWorld().rayTraceBlocks(new org.bukkit.Location(level.getWorld(), from.x, from.y, from.z),
            new org.bukkit.util.Vector(delta.x / length, delta.y / length, delta.z / length), length, fluids,
            c.blockMode() == ClipContext.Block.COLLIDER || c.blockMode() == ClipContext.Block.FALLDAMAGE_RESETTING);
        if (found == null || found.getHitBlock() == null) {
            return BlockHitResult.miss(to, nearest, BlockPos.containing(to));
        }
        net.minecraft.core.Direction side = nearest.getOpposite();
        if (found.getHitBlockFace() != null) {
            for (net.minecraft.core.Direction direction : net.minecraft.core.Direction.values()) {
                if (direction.getName().equalsIgnoreCase(found.getHitBlockFace().name())) {
                    side = direction;
                }
            }
        }
        org.bukkit.util.Vector at = found.getHitPosition();
        return new BlockHitResult(new Vec3(at.getX(), at.getY(), at.getZ()), side,
            new BlockPos(found.getHitBlock().getX(), found.getHitBlock().getY(), found.getHitBlock().getZ()), false);
    }
    default BlockHitResult clipWithInteractionOverride(Vec3 from, Vec3 to, BlockPos pos, VoxelShape blockShape, BlockState blockState) {
        return blockShape == null ? null : blockShape.clip(from, to, pos);
    }
    default double getBlockFloorHeight(VoxelShape blockShape, Supplier<VoxelShape> belowBlockShape) {
        return blockShape.isEmpty() ? belowBlockShape.get().max(net.minecraft.core.Direction.Axis.Y) - 1.0 : blockShape.max(net.minecraft.core.Direction.Axis.Y);
    }
    default double getBlockFloorHeight(BlockPos pos) {
        return getBlockFloorHeight(getBlockState(pos).getCollisionShape(this, pos), () -> getBlockState(pos.below()).getCollisionShape(this, pos.below()));
    }
    static <T, C> T traverseBlocks(Vec3 from, Vec3 to, C context, BiFunction<C, BlockPos, T> consumer, Function<C, T> missFactory) {
        if (from.equals(to)) {
            return missFactory.apply(context);
        }
        int x = Mth.floor(from.x);
        int y = Mth.floor(from.y);
        int z = Mth.floor(from.z);
        T first = consumer.apply(context, new BlockPos(x, y, z));
        if (first != null) {
            return first;
        }
        double dx = to.x - from.x;
        double dy = to.y - from.y;
        double dz = to.z - from.z;
        int sx = Mth.sign(dx);
        int sy = Mth.sign(dy);
        int sz = Mth.sign(dz);
        double tx = sx == 0 ? Double.MAX_VALUE : sx / dx;
        double ty = sy == 0 ? Double.MAX_VALUE : sy / dy;
        double tz = sz == 0 ? Double.MAX_VALUE : sz / dz;
        double nx = tx * (sx > 0 ? 1.0 - Mth.frac(from.x) : Mth.frac(from.x));
        double ny = ty * (sy > 0 ? 1.0 - Mth.frac(from.y) : Mth.frac(from.y));
        double nz = tz * (sz > 0 ? 1.0 - Mth.frac(from.z) : Mth.frac(from.z));
        while (nx <= 1.0 || ny <= 1.0 || nz <= 1.0) {
            if (nx < ny) {
                if (nx < nz) {
                    x += sx;
                    nx += tx;
                } else {
                    z += sz;
                    nz += tz;
                }
            } else if (ny < nz) {
                y += sy;
                ny += ty;
            } else {
                z += sz;
                nz += tz;
            }
            T found = consumer.apply(context, new BlockPos(x, y, z));
            if (found != null) {
                return found;
            }
        }
        return missFactory.apply(context);
    }
    static boolean forEachBlockIntersectedBetween(Vec3 from, Vec3 to, AABB aabbAtTarget, BlockStepVisitor visitor) {
        int iteration = 0;
        for (BlockPos pos : BlockPos.betweenClosed(BlockPos.containing(Math.min(from.x, to.x), Math.min(from.y, to.y), Math.min(from.z, to.z)),
                BlockPos.containing(Math.max(from.x, to.x), Math.max(from.y, to.y), Math.max(from.z, to.z)))) {
            if (aabbAtTarget.intersects(pos.getX(), pos.getY(), pos.getZ(), pos.getX() + 1, pos.getY() + 1, pos.getZ() + 1)
                    || iteration == 0) {
                if (!visitor.visit(pos.immutable(), iteration++)) {
                    return false;
                }
            }
        }
        return true;
    }
    public interface BlockStepVisitor {
        boolean visit(BlockPos pos, int iteration);
    }
}
