// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.EnumSet;
import java.util.Set;

public record CompositeDirection(Set<Direction> directions, Vec3i step) implements Directional {
    public static final CompositeDirection NORTH = new CompositeDirection(Direction.NORTH);
    public static final CompositeDirection EAST = new CompositeDirection(Direction.EAST);
    public static final CompositeDirection SOUTH = new CompositeDirection(Direction.SOUTH);
    public static final CompositeDirection WEST = new CompositeDirection(Direction.WEST);
    public static final CompositeDirection NORTH_EAST = new CompositeDirection(Direction.NORTH, Direction.EAST);
    public static final CompositeDirection SOUTH_EAST = new CompositeDirection(Direction.SOUTH, Direction.EAST);
    public static final CompositeDirection SOUTH_WEST = new CompositeDirection(Direction.SOUTH, Direction.WEST);
    public static final CompositeDirection NORTH_WEST = new CompositeDirection(Direction.NORTH, Direction.WEST);
    public static final CompositeDirection UP = new CompositeDirection(Direction.UP);
    public static final CompositeDirection NORTH_UP = new CompositeDirection(Direction.NORTH, Direction.UP);
    public static final CompositeDirection EAST_UP = new CompositeDirection(Direction.EAST, Direction.UP);
    public static final CompositeDirection SOUTH_UP = new CompositeDirection(Direction.SOUTH, Direction.UP);
    public static final CompositeDirection WEST_UP = new CompositeDirection(Direction.WEST, Direction.UP);
    public static final CompositeDirection NORTH_EAST_UP = new CompositeDirection(Direction.NORTH, Direction.EAST, Direction.UP);
    public static final CompositeDirection SOUTH_EAST_UP = new CompositeDirection(Direction.SOUTH, Direction.EAST, Direction.UP);
    public static final CompositeDirection SOUTH_WEST_UP = new CompositeDirection(Direction.SOUTH, Direction.WEST, Direction.UP);
    public static final CompositeDirection NORTH_WEST_UP = new CompositeDirection(Direction.NORTH, Direction.WEST, Direction.UP);
    public static final CompositeDirection DOWN = new CompositeDirection(Direction.DOWN);
    public static final CompositeDirection NORTH_DOWN = new CompositeDirection(Direction.NORTH, Direction.DOWN);
    public static final CompositeDirection EAST_DOWN = new CompositeDirection(Direction.EAST, Direction.DOWN);
    public static final CompositeDirection SOUTH_DOWN = new CompositeDirection(Direction.SOUTH, Direction.DOWN);
    public static final CompositeDirection WEST_DOWN = new CompositeDirection(Direction.WEST, Direction.DOWN);
    public static final CompositeDirection NORTH_EAST_DOWN = new CompositeDirection(Direction.NORTH, Direction.EAST, Direction.DOWN);
    public static final CompositeDirection SOUTH_EAST_DOWN = new CompositeDirection(Direction.SOUTH, Direction.EAST, Direction.DOWN);
    public static final CompositeDirection SOUTH_WEST_DOWN = new CompositeDirection(Direction.SOUTH, Direction.WEST, Direction.DOWN);
    public static final CompositeDirection NORTH_WEST_DOWN = new CompositeDirection(Direction.NORTH, Direction.WEST, Direction.DOWN);

    CompositeDirection(Direction... directions) {
        this(setOf(directions), stepOf(directions));
    }

    private static Set<Direction> setOf(Direction... directions) {
        Set<Direction> set = EnumSet.noneOf(Direction.class);
        set.addAll(java.util.Arrays.asList(directions));
        return set;
    }

    private static Vec3i stepOf(Direction... directions) {
        int x = 0;
        int y = 0;
        int z = 0;
        for (Direction direction : setOf(directions)) {
            x += direction.getStepX();
            y += direction.getStepY();
            z += direction.getStepZ();
        }
        return new Vec3i(x, y, z);
    }

    public CompositeDirection compose(CompositeDirection other) {
        Set<Direction> merged = EnumSet.copyOf(directions);
        merged.addAll(other.directions);
        return new CompositeDirection(merged.toArray(new Direction[0]));
    }

    public int getStepX() {
        return step.getX();
    }

    public int getStepY() {
        return step.getY();
    }

    public int getStepZ() {
        return step.getZ();
    }

    public Vec3i getStep() {
        return step;
    }

    public static enum Direction8 implements Directional {
        NORTH(CompositeDirection.NORTH), NORTH_EAST(CompositeDirection.NORTH_EAST), EAST(CompositeDirection.EAST),
        SOUTH_EAST(CompositeDirection.SOUTH_EAST), SOUTH(CompositeDirection.SOUTH), SOUTH_WEST(CompositeDirection.SOUTH_WEST),
        WEST(CompositeDirection.WEST), NORTH_WEST(CompositeDirection.NORTH_WEST);

        private final CompositeDirection composite;

        Direction8(CompositeDirection composite) {
            this.composite = composite;
        }

        public Set<Direction> getDirections() {
            return composite.directions();
        }

        public int getStepX() {
            return composite.getStepX();
        }

        public int getStepY() {
            return composite.getStepY();
        }

        public int getStepZ() {
            return composite.getStepZ();
        }

        public Vec3i getStep() {
            return composite.getStep();
        }
    }
}
