package net.minecraft.world.phys;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import org.basaltmc.bukkit.Unsupported;
public class BlockHitResult extends HitResult {
    public BlockHitResult(Vec3 vec3, Direction direction, BlockPos blockPos, boolean flag) {
    }
    public BlockPos getBlockPos() {
        throw Unsupported.hit("net.minecraft.world.phys.BlockHitResult.getBlockPos");
    }
    public Direction getDirection() {
        throw Unsupported.hit("net.minecraft.world.phys.BlockHitResult.getDirection");
    }
    public Vec3 getLocation() {
        throw Unsupported.hit("net.minecraft.world.phys.BlockHitResult.getLocation");
    }
    public boolean isInside() {
        throw Unsupported.hit("net.minecraft.world.phys.BlockHitResult.isInside");
    }
}
