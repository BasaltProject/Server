// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import io.netty.buffer.ByteBuf;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.Set;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public enum Relative {
    X(0), Y(1), Z(2), Y_ROT(3), X_ROT(4), DELTA_X(5), DELTA_Y(6), DELTA_Z(7), ROTATE_DELTA(8);

    public static final Set<Relative> ALL = Set.of(values());
    public static final Set<Relative> ROTATION = Set.of(X_ROT, Y_ROT);
    public static final Set<Relative> DELTA = Set.of(DELTA_X, DELTA_Y, DELTA_Z, ROTATE_DELTA);
    public static final StreamCodec<ByteBuf, Set<Relative>> SET_STREAM_CODEC = ByteBufCodecs.INT.map(Relative::unpack, Relative::pack);

    private final int bit;

    Relative(int bit) {
        this.bit = bit;
    }

    @SafeVarargs
    public static Set<Relative> union(Set<Relative>... sets) {
        Set<Relative> out = new HashSet<>();
        for (Set<Relative> set : sets) {
            out.addAll(set);
        }
        return out;
    }

    public static Set<Relative> rotation(boolean relativeYRot, boolean relativeXRot) {
        Set<Relative> out = EnumSet.noneOf(Relative.class);
        if (relativeYRot) {
            out.add(Y_ROT);
        }
        if (relativeXRot) {
            out.add(X_ROT);
        }
        return out;
    }

    public static Set<Relative> position(boolean relativeX, boolean relativeY, boolean relativeZ) {
        Set<Relative> out = EnumSet.noneOf(Relative.class);
        if (relativeX) {
            out.add(X);
        }
        if (relativeY) {
            out.add(Y);
        }
        if (relativeZ) {
            out.add(Z);
        }
        return out;
    }

    public static Set<Relative> direction(boolean relativeX, boolean relativeY, boolean relativeZ) {
        Set<Relative> out = EnumSet.noneOf(Relative.class);
        if (relativeX) {
            out.add(DELTA_X);
        }
        if (relativeY) {
            out.add(DELTA_Y);
        }
        if (relativeZ) {
            out.add(DELTA_Z);
        }
        return out;
    }

    public static Set<Relative> unpack(int value) {
        Set<Relative> out = EnumSet.noneOf(Relative.class);
        for (Relative relative : values()) {
            if ((value & (1 << relative.bit)) != 0) {
                out.add(relative);
            }
        }
        return out;
    }

    public static int pack(Set<Relative> set) {
        int value = 0;
        for (Relative relative : set) {
            value |= 1 << relative.bit;
        }
        return value;
    }
}
