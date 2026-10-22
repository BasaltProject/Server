package net.minecraft.world.phys;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;

public class BlockHitResult extends HitResult {
    private final Vec3 location;
    private final Direction direction;
    private final BlockPos pos;
    private final boolean inside;

    public BlockHitResult(Vec3 vec3, Direction direction, BlockPos blockPos, boolean flag) {
        this.location = vec3;
        this.direction = direction;
        this.pos = blockPos;
        this.inside = flag;
    }
    public BlockPos getBlockPos() {
        return pos;
    }
    public Direction getDirection() {
        return direction;
    }
    public Vec3 getLocation() {
        return location;
    }
    public boolean isInside() {
        return inside;
    }
}
