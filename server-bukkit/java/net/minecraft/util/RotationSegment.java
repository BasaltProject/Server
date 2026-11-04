// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.Optional;
import net.minecraft.core.Direction;

public final class RotationSegment {
    private static final int kSegments = 16;
    private static final int kMask = kSegments - 1;

    private RotationSegment() {}

    public static int getMaxSegmentIndex() {
        return kMask;
    }

    public static int convertToSegment(float degrees) {
        return Mth.floor(degrees * kSegments / 360.0F + 0.5F) & kMask;
    }

    public static int convertToSegment(Direction direction) {
        return direction.getAxis().isVertical() ? 0 : direction.getOpposite().get2DDataValue() * 4;
    }

    public static float convertToDegrees(int segment) {
        return segment * 360.0F / kSegments;
    }

    public static Optional<Direction> convertToDirection(int segment) {
        Direction direction = switch (segment) {
            case 0 -> Direction.SOUTH;
            case 4 -> Direction.WEST;
            case 8 -> Direction.NORTH;
            case 12 -> Direction.EAST;
            default -> null;
        };
        return Optional.ofNullable(direction);
    }
}
