// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;
import net.minecraft.util.StringRepresentable;
public enum FrontAndTop implements StringRepresentable {
    DOWN_EAST(Direction.DOWN, Direction.EAST),
    DOWN_NORTH(Direction.DOWN, Direction.NORTH),
    DOWN_SOUTH(Direction.DOWN, Direction.SOUTH),
    DOWN_WEST(Direction.DOWN, Direction.WEST),
    UP_EAST(Direction.UP, Direction.EAST),
    UP_NORTH(Direction.UP, Direction.NORTH),
    UP_SOUTH(Direction.UP, Direction.SOUTH),
    UP_WEST(Direction.UP, Direction.WEST),
    WEST_UP(Direction.WEST, Direction.UP),
    EAST_UP(Direction.EAST, Direction.UP),
    NORTH_UP(Direction.NORTH, Direction.UP),
    SOUTH_UP(Direction.SOUTH, Direction.UP);
    private static final int kSides = Direction.values().length;
    private static final FrontAndTop[] kLookup = new FrontAndTop[kSides * kSides];
    private final String id;
    private final Direction top;
    private final Direction front;
    static {
        for (FrontAndTop v : values()) {
            kLookup[slot(v.front, v.top)] = v;
        }
    }
    FrontAndTop(Direction front, Direction top) {
        this.id = name().toLowerCase(java.util.Locale.ROOT);
        this.front = front;
        this.top = top;
    }
    private static int slot(Direction front, Direction top) {
        return front.ordinal() * kSides + top.ordinal();
    }
    public String getSerializedName() {
        return id;
    }
    public static FrontAndTop fromFrontAndTop(Direction front, Direction top) {
        return kLookup[slot(front, top)];
    }
    public Direction front() {
        return front;
    }
    public Direction top() {
        return top;
    }
}
