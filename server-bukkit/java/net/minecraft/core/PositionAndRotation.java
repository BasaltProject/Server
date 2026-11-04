// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import io.netty.buffer.ByteBuf;
import java.util.Objects;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.phys.Vec3;

public interface PositionAndRotation {
    StreamCodec<ByteBuf, PositionAndRotation> STREAM_CODEC = StreamCodec.composite(Vec3.STREAM_CODEC, PositionAndRotation::position,
        ByteBufCodecs.FLOAT, PositionAndRotation::yRot, ByteBufCodecs.FLOAT, PositionAndRotation::xRot, PositionAndRotation::of);

    Vec3 position();

    float yRot();

    float xRot();

    PositionAndRotation immutable();

    default boolean is(Vec3 position, float yRot, float xRot) {
        return yRot() == yRot && xRot() == xRot && Objects.equals(position(), position);
    }

    static PositionAndRotation of(Vec3 position, float yRot, float xRot) {
        return new Immutable(position, yRot, xRot);
    }

    public static record Immutable(Vec3 position, float yRot, float xRot) implements PositionAndRotation {
        public PositionAndRotation immutable() {
            return this;
        }
    }

    public static class Mutable implements PositionAndRotation {
        private Vec3 position = Vec3.ZERO;
        private float yRot;
        private float xRot;

        public Mutable() {}

        public Vec3 position() {
            return position;
        }

        public float yRot() {
            return yRot;
        }

        public float xRot() {
            return xRot;
        }

        public PositionAndRotation immutable() {
            return PositionAndRotation.of(position, yRot, xRot);
        }

        public void set(Vec3 position, float yRot, float xRot) {
            this.position = position;
            this.yRot = yRot;
            this.xRot = xRot;
        }

        public void addDelta(Vec3 delta) {
            position = position.add(delta);
        }

        public void addRotation(float yRot, float xRot) {
            this.yRot += yRot;
            this.xRot += xRot;
        }
    }
}
