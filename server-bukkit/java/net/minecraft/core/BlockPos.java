package net.minecraft.core;

import org.basaltmc.bukkit.Unsupported;
public class BlockPos extends Vec3i {
    public static BlockPos ZERO;
    public BlockPos(int value, int value2, int value3) {
    }
    public BlockPos above() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.above");
    }
    public BlockPos below() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.below");
    }
    public BlockPos east() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.east");
    }
    public int getX() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.getX");
    }
    public int getY() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.getY");
    }
    public int getZ() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.getZ");
    }
    public BlockPos north() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.north");
    }
    public BlockPos relative(Direction direction) {
        throw Unsupported.hit("net.minecraft.core.BlockPos.relative");
    }
    public BlockPos south() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.south");
    }
    public BlockPos west() {
        throw Unsupported.hit("net.minecraft.core.BlockPos.west");
    }
}
