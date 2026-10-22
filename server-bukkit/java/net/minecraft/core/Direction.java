package net.minecraft.core;

import java.util.Locale;
import net.minecraft.util.StringRepresentable;

public enum Direction implements StringRepresentable, Directional {
    DOWN(0, -1, 0, -1),
    UP(0, 1, 0, -1),
    NORTH(0, 0, -1, 2),
    SOUTH(0, 0, 1, 0),
    WEST(-1, 0, 0, 1),
    EAST(1, 0, 0, 3);

    private final int stepX;
    private final int stepY;
    private final int stepZ;
    private final int data2d;
    private final String name = name().toLowerCase(Locale.ROOT);

    Direction(int stepX, int stepY, int stepZ, int data2d) {
        this.stepX = stepX;
        this.stepY = stepY;
        this.stepZ = stepZ;
        this.data2d = data2d;
    }

    public int getStepX() {
        return stepX;
    }

    public int getStepY() {
        return stepY;
    }

    public int getStepZ() {
        return stepZ;
    }

    public int get2DDataValue() {
        return data2d;
    }

    public static Direction from2DDataValue(int data) {
        return switch (Math.floorMod(data, 4)) {
            case 0 -> SOUTH;
            case 1 -> WEST;
            case 2 -> NORTH;
            default -> EAST;
        };
    }

    public Direction getOpposite() {
        return switch (this) {
            case DOWN -> UP;
            case UP -> DOWN;
            case NORTH -> SOUTH;
            case SOUTH -> NORTH;
            case WEST -> EAST;
            case EAST -> WEST;
        };
    }

    public Axis getAxis() {
        return stepX != 0 ? Axis.X : stepY != 0 ? Axis.Y : Axis.Z;
    }

    public AxisDirection getAxisDirection() {
        return stepX + stepY + stepZ > 0 ? AxisDirection.POSITIVE : AxisDirection.NEGATIVE;
    }

    public String getName() {
        return name;
    }

    @Override
    public String getSerializedName() {
        return name;
    }

    @Override
    public String toString() {
        return name;
    }

    public enum Axis implements StringRepresentable {
        X, Y, Z;

        public String getName() {
            return name().toLowerCase(Locale.ROOT);
        }

        @Override
        public String getSerializedName() {
            return getName();
        }
    }

    public enum AxisDirection {
        POSITIVE(1), NEGATIVE(-1);

        private final int step;

        AxisDirection(int step) {
            this.step = step;
        }

        public int getStep() {
            return step;
        }
    }
}
