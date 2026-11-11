// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import com.google.common.collect.AbstractIterator;
import java.util.function.BiFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.core.Cursor3D;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.util.Mth;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.phys.shapes.BooleanOp;
import net.minecraft.core.SectionPos;
import net.minecraft.world.phys.shapes.Shapes;
public class BlockCollisions<T> extends AbstractIterator<T> {
    private final AABB box;
    private final CollisionContext context;
    private final Cursor3D cursor;
    private final BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
    private final VoxelShape entityShape;
    private final CollisionGetter collisionGetter;
    private final boolean onlySuffocatingBlocks;
    private BlockGetter cachedGetter;
    private long cachedKey;
    private final BiFunction<BlockPos.MutableBlockPos, VoxelShape, T> resultProvider;
    public BlockCollisions(CollisionGetter collisionGetter, Entity source, AABB box, boolean onlySuffocatingBlocks, BiFunction<BlockPos.MutableBlockPos, VoxelShape, T> resultProvider) {
        this(collisionGetter, source == null ? CollisionContext.empty() : CollisionContext.of(source), box, onlySuffocatingBlocks, resultProvider);
    }
    public BlockCollisions(CollisionGetter collisionGetter, CollisionContext context, AABB box, boolean onlySuffocatingBlocks, BiFunction<BlockPos.MutableBlockPos, VoxelShape, T> resultProvider) {
        this.collisionGetter = collisionGetter;
        this.context = context;
        this.box = box;
        this.onlySuffocatingBlocks = onlySuffocatingBlocks;
        this.resultProvider = resultProvider;
        this.entityShape = Shapes.create(box);
        double eps = 1.0E-7;
        this.cursor = new Cursor3D(Mth.floor(box.minX - eps) - 1, Mth.floor(box.minY - eps) - 1, Mth.floor(box.minZ - eps) - 1,
            Mth.floor(box.maxX + eps) + 1, Mth.floor(box.maxY + eps) + 1, Mth.floor(box.maxZ + eps) + 1);
    }
    private BlockGetter chunkAt(int x, int z) {
        int cx = SectionPos.blockToSectionCoord(x);
        int cz = SectionPos.blockToSectionCoord(z);
        long key = ChunkPos.pack(cx, cz);
        if (cachedGetter == null || cachedKey != key) {
            cachedGetter = collisionGetter.getChunkForCollisions(cx, cz);
            cachedKey = key;
        }
        return cachedGetter;
    }
    @Override
    protected T computeNext() {
        while (cursor.advance()) {
            int x = cursor.nextX();
            int y = cursor.nextY();
            int z = cursor.nextZ();
            int faces = cursor.getNextType();
            BlockGetter chunk = faces == 3 ? null : chunkAt(x, z);
            if (chunk == null) {
                continue;
            }
            pos.set(x, y, z);
            BlockState state = chunk.getBlockState(pos);
            boolean skip = onlySuffocatingBlocks && !state.isSuffocating(chunk, pos);
            skip |= faces == 1 && !state.hasLargeCollisionShape();
            skip |= faces == 2 && !state.is(Blocks.MOVING_PISTON);
            if (skip) {
                continue;
            }
            VoxelShape local = context.getCollisionShape(state, collisionGetter, pos);
            if (local == Shapes.block()) {
                if (box.intersects(x, y, z, x + 1.0, y + 1.0, z + 1.0)) {
                    return resultProvider.apply(pos, local.move(pos));
                }
                continue;
            }
            VoxelShape moved = local.move(pos);
            if (!moved.isEmpty() && Shapes.joinIsNotEmpty(moved, entityShape, BooleanOp.AND)) {
                return resultProvider.apply(pos, moved);
            }
        }
        return endOfData();
    }
}
