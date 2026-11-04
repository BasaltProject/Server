// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.redstone;

import net.minecraft.core.Direction;
import net.minecraft.world.level.Level;
public class ExperimentalRedstoneUtils {
    protected ExperimentalRedstoneUtils() {
    }
    public static Orientation initialOrientation(Level level, Direction front, Direction up) {
        return null;
    }
    public static Orientation withFront(Orientation orientation, Direction front) {
        return orientation == null ? null : orientation.withFront(front);
    }
}
