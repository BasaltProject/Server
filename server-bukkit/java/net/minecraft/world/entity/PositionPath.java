// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.function.IntFunction;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.ByIdMap;
import net.minecraft.world.phys.Vec3;

public interface PositionPath {
    StreamCodec<ByteBuf, PositionPath> STREAM_CODEC = Type.STREAM_CODEC.dispatch(PositionPath::type, Type::streamCodec);

    Vec3 endPosition();

    Type type();

    static PositionPath of(Vec3 position) {
        return new Linear(position);
    }

    static PositionPath stepped(List<PositionStep> steps) {
        return new Stepped(steps);
    }

    public static record Linear(Vec3 endPosition) implements PositionPath {
        public static final StreamCodec<ByteBuf, Linear> STREAM_CODEC = Vec3.STREAM_CODEC.map(Linear::new, Linear::endPosition);

        public Type type() {
            return Type.LINEAR;
        }
    }

    public static record Stepped(Vec3 endPosition, List<PositionStep> steps) implements PositionPath {
        public static final StreamCodec<ByteBuf, Stepped> STREAM_CODEC = PositionStep.STREAM_CODEC.apply(ByteBufCodecs.list()).map(Stepped::new, Stepped::steps);

        public Stepped(List<PositionStep> steps) {
            this(steps.getLast().position(), steps);
        }

        public Type type() {
            return Type.STEPPED;
        }
    }

    public static enum Type {
        LINEAR, STEPPED;

        public static final IntFunction<Type> BY_ID = ByIdMap.continuous(Enum::ordinal, values(), ByIdMap.OutOfBoundsStrategy.ZERO);
        public static final StreamCodec<ByteBuf, Type> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, Enum::ordinal);

        public StreamCodec<ByteBuf, ? extends PositionPath> streamCodec() {
            return this == LINEAR ? Linear.STREAM_CODEC : Stepped.STREAM_CODEC;
        }
    }
}
