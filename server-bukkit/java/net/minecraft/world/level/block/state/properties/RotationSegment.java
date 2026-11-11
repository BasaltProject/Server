// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import net.minecraft.util.SegmentedAnglePrecision;
import java.util.Optional;
import net.minecraft.core.Direction;
public class RotationSegment {
    private static final SegmentedAnglePrecision SEGMENTED_ANGLE16 = new SegmentedAnglePrecision(4);
    public static int getMaxSegmentIndex() {
        return SEGMENTED_ANGLE16.getMask();
    }
    public static int convertToSegment(Direction direction) {
        return SEGMENTED_ANGLE16.fromDirection(direction);
    }
    public static int convertToSegment(float rotDegrees) {
        return SEGMENTED_ANGLE16.fromDegrees(rotDegrees);
    }
    public static Optional<Direction> convertToDirection(int segment) {
        return Optional.ofNullable(switch (segment) {
            case 0 -> Direction.NORTH;
            case 4 -> Direction.EAST;
            case 8 -> Direction.SOUTH;
            case 12 -> Direction.WEST;
            default -> null;
        });
    }
    public static float convertToDegrees(int segment) {
        return SEGMENTED_ANGLE16.toDegrees(segment);
    }
}
