// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import java.util.Optional;
import java.util.OptionalInt;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelSimulatedReader;
import net.minecraft.world.level.block.state.BlockState;

public abstract class Column {
    public static Range around(int lowest, int highest) {
        return new Range(lowest - 1, highest + 1);
    }

    public static Range inside(int floor, int ceiling) {
        return new Range(floor, ceiling);
    }

    public static Column below(int ceiling) {
        return new Ray(ceiling, false);
    }

    public static Column fromHighest(int highest) {
        return new Ray(highest + 1, false);
    }

    public static Column above(int floor) {
        return new Ray(floor, true);
    }

    public static Column fromLowest(int lowest) {
        return new Ray(lowest - 1, true);
    }

    public static Column line() {
        return Line.INSTANCE;
    }

    public static Column create(OptionalInt floor, OptionalInt ceiling) {
        if (floor.isPresent() && ceiling.isPresent()) {
            return inside(floor.getAsInt(), ceiling.getAsInt());
        }
        if (floor.isPresent()) {
            return above(floor.getAsInt());
        }
        return ceiling.isPresent() ? below(ceiling.getAsInt()) : line();
    }

    public abstract OptionalInt getCeiling();

    public abstract OptionalInt getFloor();

    public abstract OptionalInt getHeight();

    public Column withFloor(OptionalInt floor) {
        return create(floor, getCeiling());
    }

    public Column withCeiling(OptionalInt ceiling) {
        return create(getFloor(), ceiling);
    }

    public static Optional<Column> scan(LevelSimulatedReader level, BlockPos pos, int searchRange, Predicate<BlockState> insideColumn, Predicate<BlockState> validEdge) {
        if (!level.isStateAtPosition(pos, insideColumn)) {
            return Optional.empty();
        }
        BlockPos.MutableBlockPos cursor = pos.mutable();
        OptionalInt ceiling = walk(level, searchRange, insideColumn, validEdge, cursor, pos.getY(), Direction.UP);
        OptionalInt floor = walk(level, searchRange, insideColumn, validEdge, cursor, pos.getY(), Direction.DOWN);
        return Optional.of(create(floor, ceiling));
    }

    private static OptionalInt walk(LevelSimulatedReader level, int searchRange, Predicate<BlockState> insideColumn, Predicate<BlockState> validEdge, BlockPos.MutableBlockPos cursor, int startY, Direction direction) {
        cursor.setY(startY);
        for (int step = 1; step < searchRange && level.isStateAtPosition(cursor, insideColumn); step++) {
            cursor.move(direction);
        }
        return level.isStateAtPosition(cursor, validEdge) ? OptionalInt.of(cursor.getY()) : OptionalInt.empty();
    }

    public static final class Range extends Column {
        private final int floor;
        private final int ceiling;

        public Range(int floor, int ceiling) {
            this.floor = floor;
            this.ceiling = ceiling;
            if (height() < 0) {
                throw new IllegalArgumentException("Column of negative height: " + this);
            }
        }

        public OptionalInt getCeiling() {
            return OptionalInt.of(ceiling);
        }

        public OptionalInt getFloor() {
            return OptionalInt.of(floor);
        }

        public OptionalInt getHeight() {
            return OptionalInt.of(height());
        }

        public int ceiling() {
            return ceiling;
        }

        public int floor() {
            return floor;
        }

        public int height() {
            return ceiling - floor - 1;
        }

        public String toString() {
            return "C(" + ceiling + "-" + floor + ")";
        }
    }

    public static final class Line extends Column {
        static final Line INSTANCE = new Line();

        private Line() {
        }

        public OptionalInt getCeiling() {
            return OptionalInt.empty();
        }

        public OptionalInt getFloor() {
            return OptionalInt.empty();
        }

        public OptionalInt getHeight() {
            return OptionalInt.empty();
        }

        public String toString() {
            return "C(-)";
        }
    }

    public static final class Ray extends Column {
        private final int edge;
        private final boolean pointingUp;

        public Ray(int edge, boolean pointingUp) {
            this.edge = edge;
            this.pointingUp = pointingUp;
        }

        public OptionalInt getCeiling() {
            return pointingUp ? OptionalInt.empty() : OptionalInt.of(edge);
        }

        public OptionalInt getFloor() {
            return pointingUp ? OptionalInt.of(edge) : OptionalInt.empty();
        }

        public OptionalInt getHeight() {
            return OptionalInt.empty();
        }

        public String toString() {
            return pointingUp ? "C(" + edge + "-)" : "C(-" + edge + ")";
        }
    }
}
