package net.minecraft.core;

import net.minecraft.util.StringRepresentable;
import org.basaltmc.bukkit.Unsupported;
public enum Direction implements Directional, StringRepresentable {
    DOWN, UP, NORTH, SOUTH, WEST, EAST;
    public static Direction from2DDataValue(int data) {
        throw Unsupported.hit("net.minecraft.core.Direction.from2DDataValue");
    }
    public int get2DDataValue() {
        throw Unsupported.hit("net.minecraft.core.Direction.get2DDataValue");
    }
    public Direction.Axis getAxis() {
        throw Unsupported.hit("net.minecraft.core.Direction.getAxis");
    }
    public Direction getOpposite() {
        throw Unsupported.hit("net.minecraft.core.Direction.getOpposite");
    }
    public int getStepX() {
        throw Unsupported.hit("net.minecraft.core.Direction.getStepX");
    }
    public int getStepY() {
        throw Unsupported.hit("net.minecraft.core.Direction.getStepY");
    }
    public int getStepZ() {
        throw Unsupported.hit("net.minecraft.core.Direction.getStepZ");
    }

    public enum Axis implements StringRepresentable {
        X, Y, Z;
    }
}
