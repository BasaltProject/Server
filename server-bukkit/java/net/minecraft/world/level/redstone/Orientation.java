// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.redstone;

import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.core.Direction;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.RandomSource;

public class Orientation {
    private static final Orientation[] kAll = build();
    public static final StreamCodec<ByteBuf, Orientation> STREAM_CODEC = ByteBufCodecs.idMapper(
        Orientation::fromIndex, Orientation::getIndex);

    private final Direction up;
    private final Direction front;
    private final Direction side;
    private final SideBias sideBias;
    private final int index;
    private final List<Direction> neighbors;
    private final List<Direction> horizontal;
    private final List<Direction> vertical;

    protected Orientation() {
        this(Direction.UP, Direction.NORTH, SideBias.LEFT);
    }

    private Orientation(Direction up, Direction front, SideBias sideBias) {
        this.up = up;
        this.front = front;
        this.sideBias = sideBias;
        this.index = generateIndex(up, front, sideBias);
        Direction right = Direction.getNearest(front.getUnitVec3i().cross(up.getUnitVec3i()), null);
        this.side = sideBias == SideBias.RIGHT ? right : right.getOpposite();
        this.neighbors = List.of(front.getOpposite(), front, side, side.getOpposite(), up.getOpposite(), up);
        this.horizontal = neighbors.stream().filter(d -> d.getAxis() != up.getAxis()).toList();
        this.vertical = neighbors.stream().filter(d -> d.getAxis() == up.getAxis()).toList();
    }

    private static Orientation[] build() {
        Orientation[] all = new Orientation[48];
        for (Direction up : Direction.values()) {
            for (Direction front : Direction.values()) {
                if (up.getAxis() == front.getAxis()) {
                    continue;
                }
                for (SideBias bias : SideBias.values()) {
                    all[generateIndex(up, front, bias)] = new Orientation(up, front, bias);
                }
            }
        }
        return all;
    }

    public static Orientation of(Direction up, Direction front, SideBias sideBias) {
        return kAll[generateIndex(up, front, sideBias)];
    }

    public Orientation withUp(Direction up) {
        Direction newFront = front;
        if (up == front) {
            newFront = this.up.getOpposite();
        } else if (up == front.getOpposite()) {
            newFront = this.up;
        }
        return of(up, newFront, sideBias);
    }

    public Orientation withFront(Direction front) {
        Direction newUp = up;
        if (front == up) {
            newUp = this.front.getOpposite();
        } else if (front == up.getOpposite()) {
            newUp = this.front;
        }
        return of(newUp, front, sideBias);
    }

    public Orientation withFrontPreserveUp(Direction front) {
        return front.getAxis() == up.getAxis() ? this : withFront(front);
    }

    public Orientation withFrontAdjustSideBias(Direction front) {
        Orientation turned = withFront(front);
        return this.front == turned.side ? turned.withMirror() : turned;
    }

    public Orientation withSideBias(SideBias sideBias) {
        return of(up, front, sideBias);
    }

    public Orientation withMirror() {
        return withSideBias(sideBias.getOpposite());
    }

    public Direction getFront() {
        return front;
    }

    public Direction getUp() {
        return up;
    }

    public Direction getSide() {
        return side;
    }

    public SideBias getSideBias() {
        return sideBias;
    }

    public List<Direction> getDirections() {
        return neighbors;
    }

    public List<Direction> getHorizontalDirections() {
        return horizontal;
    }

    public List<Direction> getVerticalDirections() {
        return vertical;
    }

    @Override
    public String toString() {
        return "[up=" + up + ",front=" + front + ",sideBias=" + sideBias + "]";
    }

    public int getIndex() {
        return index;
    }

    public static Orientation fromIndex(int index) {
        return kAll[index];
    }

    public static Orientation random(RandomSource rand) {
        Orientation picked;
        do {
            picked = kAll[rand.nextInt(kAll.length)];
        } while (picked == null);
        return picked;
    }

    protected static int generateIndex(Direction up, Direction front, SideBias sideBias) {
        if (up.getAxis() == front.getAxis()) {
            throw new IllegalStateException("Up-vector and front-vector can not be on the same axis");
        }
        int axisKey;
        if (up.getAxis() == Direction.Axis.Y) {
            axisKey = front.getAxis() == Direction.Axis.X ? 1 : 0;
        } else {
            axisKey = front.getAxis() == Direction.Axis.Y ? 1 : 0;
        }
        int frontKey = axisKey << 1 | front.getAxisDirection().ordinal();
        return ((up.ordinal() << 2) + frontKey << 1) + sideBias.ordinal();
    }

    public static enum SideBias {
        LEFT, RIGHT;

        public SideBias getOpposite() {
            return this == LEFT ? RIGHT : LEFT;
        }

        @Override
        public String toString() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
