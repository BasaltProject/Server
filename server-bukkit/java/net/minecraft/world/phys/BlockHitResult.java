// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;

public class BlockHitResult extends HitResult {
    private final Direction direction;
    private final BlockPos pos;
    private final boolean inside;
    private final boolean missed;
    private final boolean worldBorderHit;

    public BlockHitResult(Vec3 vec3, Direction direction, BlockPos blockPos, boolean flag) {
        this(false, vec3, direction, blockPos, flag, false);
    }
    public BlockPos getBlockPos() {
        return pos;
    }
    public Direction getDirection() {
        return direction;
    }
    public boolean isInside() {
        return inside;
    }
    public static final StreamCodec<ByteBuf, BlockHitResult> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.phys.BlockHitResult.class, "STREAM_CODEC");
    public static BlockHitResult miss(Vec3 location, Direction direction, BlockPos pos) {
        return new BlockHitResult(true, location, direction, pos, false, false);
    }
    public BlockHitResult(Vec3 location, Direction direction, BlockPos pos, boolean inside, boolean worldBorderHit) {
        this(false, location, direction, pos, inside, worldBorderHit);
    }
    private BlockHitResult(boolean miss, Vec3 location, Direction direction, BlockPos blockPos, boolean inside, boolean worldBorderHit) {
        super(location);
        this.missed = miss;
        this.direction = direction;
        this.pos = blockPos;
        this.inside = inside;
        this.worldBorderHit = worldBorderHit;
    }
    public BlockHitResult withDirection(Direction direction) {
        return new BlockHitResult(missed, location, direction, pos, inside, worldBorderHit);
    }
    public BlockHitResult withPosition(BlockPos blockPos) {
        return new BlockHitResult(missed, location, direction, blockPos, inside, worldBorderHit);
    }
    public BlockHitResult hitBorder() {
        return new BlockHitResult(missed, location, direction, pos, inside, true);
    }
    public HitResult.Type getType() {
        return missed ? HitResult.Type.MISS : HitResult.Type.BLOCK;
    }
    public boolean isWorldBorderHit() {
        return worldBorderHit;
    }
}
