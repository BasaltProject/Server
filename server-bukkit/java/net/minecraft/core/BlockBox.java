// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import io.netty.buffer.ByteBuf;
import java.util.Iterator;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.phys.AABB;

public record BlockBox(BlockPos min, BlockPos max) implements Iterable<BlockPos> {
    public static final StreamCodec<ByteBuf, BlockBox> STREAM_CODEC = StreamCodec.of(
        (buf, box) -> {
            FriendlyByteBuf.writeBlockPos(buf, box.min);
            FriendlyByteBuf.writeBlockPos(buf, box.max);
        },
        buf -> new BlockBox(FriendlyByteBuf.readBlockPos(buf), FriendlyByteBuf.readBlockPos(buf)));

    public BlockBox(BlockPos min, BlockPos max) {
        this.min = BlockPos.min(min, max);
        this.max = BlockPos.max(min, max);
    }

    public static BlockBox of(BlockPos pos) {
        return new BlockBox(pos, pos);
    }

    public static BlockBox of(BlockPos a, BlockPos b) {
        return new BlockBox(a, b);
    }

    public BlockBox include(BlockPos pos) {
        return new BlockBox(BlockPos.min(min, pos), BlockPos.max(max, pos));
    }

    public boolean isBlock() {
        return min.equals(max);
    }

    public boolean contains(BlockPos pos) {
        return inRange(pos.getX(), min.getX(), max.getX()) && inRange(pos.getY(), min.getY(), max.getY())
            && inRange(pos.getZ(), min.getZ(), max.getZ());
    }

    private static boolean inRange(int value, int low, int high) {
        return value >= low && value <= high;
    }

    public AABB aabb() {
        return AABB.encapsulatingFullBlocks(min, max);
    }

    public Iterator<BlockPos> iterator() {
        return BlockPos.betweenClosed(min, max).iterator();
    }

    public int sizeX() {
        return max.getX() - min.getX() + 1;
    }

    public int sizeY() {
        return max.getY() - min.getY() + 1;
    }

    public int sizeZ() {
        return max.getZ() - min.getZ() + 1;
    }

    public BlockBox extend(Direction direction, int amount) {
        if (amount == 0) {
            return this;
        }
        if (direction.getAxisDirection() == Direction.AxisDirection.POSITIVE) {
            return of(min, BlockPos.max(min, max.relative(direction, amount)));
        }
        return of(BlockPos.min(min.relative(direction, amount), max), max);
    }

    public BlockBox move(Direction direction, int amount) {
        return amount == 0 ? this : new BlockBox(min.relative(direction, amount), max.relative(direction, amount));
    }

    public BlockBox offset(Vec3i offset) {
        return new BlockBox(min.offset(offset), max.offset(offset));
    }
}
