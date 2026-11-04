// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.network.codec.StreamCodec;

public record Filterable<T>(T raw, Optional<T> filtered) {
    public static <T> Codec<Filterable<T>> codec(Codec<T> valueCodec) {
        Codec<Filterable<T>> full = RecordCodecBuilder.create(instance -> instance.group(
            valueCodec.fieldOf("raw").forGetter(Filterable::raw),
            valueCodec.optionalFieldOf("filtered").forGetter(Filterable::filtered)).apply(instance, Filterable::new));
        Codec<Filterable<T>> plain = valueCodec.xmap(Filterable::passThrough, Filterable::raw);
        return Codec.withAlternative(full, plain);
    }

    public static <B extends ByteBuf, T> StreamCodec<B, Filterable<T>> streamCodec(StreamCodec<B, T> valueCodec) {
        return net.minecraft.network.codec.StreamCodec.composite(valueCodec, Filterable::raw, net.minecraft.network.codec.ByteBufCodecs.optional(valueCodec), Filterable::filtered, Filterable::new);
    }

    public static <T> Filterable<T> passThrough(T value) {
        return new Filterable<>(value, Optional.empty());
    }

    public static Filterable<String> from(FilteredText text) {
        return new Filterable<>(text.raw(), text.isFiltered() ? Optional.of(text.filteredOrEmpty()) : Optional.empty());
    }

    public T get(boolean filterEnabled) {
        return filterEnabled ? filtered.orElse(raw) : raw;
    }

    public <U> Filterable<U> map(Function<T, U> function) {
        return new Filterable<>(function.apply(raw), filtered.map(function));
    }

    public <U> Optional<Filterable<U>> resolve(Function<T, Optional<U>> function) {
        Optional<U> rawResult = function.apply(raw);
        if (rawResult.isEmpty()) {
            return Optional.empty();
        }
        if (filtered.isPresent()) {
            Optional<U> filteredResult = function.apply(filtered.get());
            return filteredResult.isEmpty() ? Optional.empty() : Optional.of(new Filterable<>(rawResult.get(), filteredResult));
        }
        return Optional.of(new Filterable<>(rawResult.get(), Optional.empty()));
    }
}
