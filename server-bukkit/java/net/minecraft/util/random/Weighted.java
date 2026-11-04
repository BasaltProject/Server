// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.random;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.util.function.Function;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.ExtraCodecs;

public record Weighted<T>(T value, int weight) {
    public Weighted {
        if (weight < 0) {
            throw new IllegalArgumentException("Weight should be >= 0");
        }
    }

    public static <E> Codec<Weighted<E>> codec(Codec<E> elementCodec) {
        return codec(elementCodec.fieldOf("data"));
    }

    public static <E> Codec<Weighted<E>> codec(MapCodec<E> elementCodec) {
        return RecordCodecBuilder.create(instance -> instance.group(
            elementCodec.forGetter(Weighted::value),
            ExtraCodecs.NON_NEGATIVE_INT.fieldOf("weight").forGetter(Weighted::weight)).apply(instance, Weighted::new));
    }

    public static <B extends ByteBuf, T> StreamCodec<B, Weighted<T>> streamCodec(StreamCodec<B, T> valueCodec) {
        return StreamCodec.composite(valueCodec, Weighted::value, ByteBufCodecs.VAR_INT, Weighted::weight, Weighted::new);
    }

    public <U> Weighted<U> map(Function<T, U> function) {
        return new Weighted<>(function.apply(value), weight);
    }
}
