// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.codec;

import com.mojang.datafixers.util.Function10;
import com.mojang.datafixers.util.Function11;
import com.mojang.datafixers.util.Function12;
import com.mojang.datafixers.util.Function13;
import com.mojang.datafixers.util.Function14;
import com.mojang.datafixers.util.Function3;
import com.mojang.datafixers.util.Function4;
import com.mojang.datafixers.util.Function5;
import com.mojang.datafixers.util.Function6;
import com.mojang.datafixers.util.Function7;
import com.mojang.datafixers.util.Function8;
import com.mojang.datafixers.util.Function9;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.function.BiFunction;
import java.util.function.Function;
import java.util.function.UnaryOperator;

public interface StreamCodec<B, V> extends StreamDecoder<B, V>, StreamEncoder<B, V> {
    static <B, V> StreamCodec<B, V> of(StreamEncoder<B, V> encoder, StreamDecoder<B, V> decoder) {
        return new StreamCodec<>() {
            @Override
            public V decode(B input) {
                return decoder.decode(input);
            }

            @Override
            public void encode(B output, V value) {
                encoder.encode(output, value);
            }
        };
    }

    static <B, V> StreamCodec<B, V> ofMember(StreamMemberEncoder<B, V> encoder, StreamDecoder<B, V> decoder) {
        return of((output, value) -> encoder.encode(value, output), decoder);
    }

    static <B, V> StreamCodec<B, V> unit(V instance) {
        return of((output, value) -> {
            if (!value.equals(instance)) {
                throw new IllegalStateException("Can't encode '" + value + "', expected '" + instance + "'");
            }
        }, input -> instance);
    }

    default <O> StreamCodec<B, O> apply(CodecOperation<B, V, O> operation) {
        return operation.apply(this);
    }

    default <O> StreamCodec<B, O> map(Function<? super V, ? extends O> to, Function<? super O, ? extends V> from) {
        return of((output, value) -> this.encode(output, from.apply(value)), input -> to.apply(this.decode(input)));
    }

    default <O extends ByteBuf> StreamCodec<O, V> mapStream(Function<O, ? extends B> operation) {
        return of((output, value) -> this.encode(operation.apply(output), value), input -> this.decode(operation.apply(input)));
    }

    default <U> StreamCodec<B, U> dispatch(Function<? super U, ? extends V> type,
        Function<? super V, ? extends StreamCodec<? super B, ? extends U>> codec) {
        return of((output, value) -> {
            V key = type.apply(value);
            this.encode(output, key);
            @SuppressWarnings("unchecked")
            StreamCodec<B, U> valueCodec = (StreamCodec<B, U>) codec.apply(key);
            valueCodec.encode(output, value);
        }, input -> {
            V key = this.decode(input);
            @SuppressWarnings("unchecked")
            StreamCodec<B, U> valueCodec = (StreamCodec<B, U>) codec.apply(key);
            return valueCodec.decode(input);
        });
    }

    @FunctionalInterface
    interface CodecOperation<B, S, T> {
        StreamCodec<B, T> apply(StreamCodec<B, S> codec);
    }

    record Part<B, C, T>(StreamCodec<? super B, T> codec, Function<C, T> getter) {
        void write(B output, C value) {
            codec.encode(output, getter.apply(value));
        }

        Object read(B input) {
            return codec.decode(input);
        }
    }

    private static <B, C> StreamCodec<B, C> assemble(List<Part<B, C, ?>> parts, Function<Object[], C> build) {
        return of((output, value) -> {
            for (Part<B, C, ?> part : parts) {
                part.write(output, value);
            }
        }, input -> {
            Object[] fields = new Object[parts.size()];
            for (int slot = 0; slot < fields.length; slot++) {
                fields[slot] = parts.get(slot).read(input);
            }
            return build.apply(fields);
        });
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, Function<T1, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1)), f -> constructor.apply((T1) f[0]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, BiFunction<T1, T2, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2)), f -> constructor.apply((T1) f[0], (T2) f[1]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, Function3<T1, T2, T3, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, Function4<T1, T2, T3, T4, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, Function5<T1, T2, T3, T4, T5, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, Function6<T1, T2, T3, T4, T5, T6, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6, T7> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, StreamCodec<? super B, T7> codec7, Function<C, T7> getter7, Function7<T1, T2, T3, T4, T5, T6, T7, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6), new Part<B, C, T7>(codec7, getter7)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5], (T7) f[6]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6, T7, T8> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, StreamCodec<? super B, T7> codec7, Function<C, T7> getter7, StreamCodec<? super B, T8> codec8, Function<C, T8> getter8, Function8<T1, T2, T3, T4, T5, T6, T7, T8, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6), new Part<B, C, T7>(codec7, getter7), new Part<B, C, T8>(codec8, getter8)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5], (T7) f[6], (T8) f[7]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6, T7, T8, T9> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, StreamCodec<? super B, T7> codec7, Function<C, T7> getter7, StreamCodec<? super B, T8> codec8, Function<C, T8> getter8, StreamCodec<? super B, T9> codec9, Function<C, T9> getter9, Function9<T1, T2, T3, T4, T5, T6, T7, T8, T9, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6), new Part<B, C, T7>(codec7, getter7), new Part<B, C, T8>(codec8, getter8), new Part<B, C, T9>(codec9, getter9)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5], (T7) f[6], (T8) f[7], (T9) f[8]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, StreamCodec<? super B, T7> codec7, Function<C, T7> getter7, StreamCodec<? super B, T8> codec8, Function<C, T8> getter8, StreamCodec<? super B, T9> codec9, Function<C, T9> getter9, StreamCodec<? super B, T10> codec10, Function<C, T10> getter10, Function10<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6), new Part<B, C, T7>(codec7, getter7), new Part<B, C, T8>(codec8, getter8), new Part<B, C, T9>(codec9, getter9), new Part<B, C, T10>(codec10, getter10)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5], (T7) f[6], (T8) f[7], (T9) f[8], (T10) f[9]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, StreamCodec<? super B, T7> codec7, Function<C, T7> getter7, StreamCodec<? super B, T8> codec8, Function<C, T8> getter8, StreamCodec<? super B, T9> codec9, Function<C, T9> getter9, StreamCodec<? super B, T10> codec10, Function<C, T10> getter10, StreamCodec<? super B, T11> codec11, Function<C, T11> getter11, Function11<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6), new Part<B, C, T7>(codec7, getter7), new Part<B, C, T8>(codec8, getter8), new Part<B, C, T9>(codec9, getter9), new Part<B, C, T10>(codec10, getter10), new Part<B, C, T11>(codec11, getter11)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5], (T7) f[6], (T8) f[7], (T9) f[8], (T10) f[9], (T11) f[10]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, StreamCodec<? super B, T7> codec7, Function<C, T7> getter7, StreamCodec<? super B, T8> codec8, Function<C, T8> getter8, StreamCodec<? super B, T9> codec9, Function<C, T9> getter9, StreamCodec<? super B, T10> codec10, Function<C, T10> getter10, StreamCodec<? super B, T11> codec11, Function<C, T11> getter11, StreamCodec<? super B, T12> codec12, Function<C, T12> getter12, Function12<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6), new Part<B, C, T7>(codec7, getter7), new Part<B, C, T8>(codec8, getter8), new Part<B, C, T9>(codec9, getter9), new Part<B, C, T10>(codec10, getter10), new Part<B, C, T11>(codec11, getter11), new Part<B, C, T12>(codec12, getter12)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5], (T7) f[6], (T8) f[7], (T9) f[8], (T10) f[9], (T11) f[10], (T12) f[11]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, StreamCodec<? super B, T7> codec7, Function<C, T7> getter7, StreamCodec<? super B, T8> codec8, Function<C, T8> getter8, StreamCodec<? super B, T9> codec9, Function<C, T9> getter9, StreamCodec<? super B, T10> codec10, Function<C, T10> getter10, StreamCodec<? super B, T11> codec11, Function<C, T11> getter11, StreamCodec<? super B, T12> codec12, Function<C, T12> getter12, StreamCodec<? super B, T13> codec13, Function<C, T13> getter13, Function13<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6), new Part<B, C, T7>(codec7, getter7), new Part<B, C, T8>(codec8, getter8), new Part<B, C, T9>(codec9, getter9), new Part<B, C, T10>(codec10, getter10), new Part<B, C, T11>(codec11, getter11), new Part<B, C, T12>(codec12, getter12), new Part<B, C, T13>(codec13, getter13)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5], (T7) f[6], (T8) f[7], (T9) f[8], (T10) f[9], (T11) f[10], (T12) f[11], (T13) f[12]));
    }

    @SuppressWarnings("unchecked")
    static <B, C, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14> StreamCodec<B, C> composite(StreamCodec<? super B, T1> codec1, Function<C, T1> getter1, StreamCodec<? super B, T2> codec2, Function<C, T2> getter2, StreamCodec<? super B, T3> codec3, Function<C, T3> getter3, StreamCodec<? super B, T4> codec4, Function<C, T4> getter4, StreamCodec<? super B, T5> codec5, Function<C, T5> getter5, StreamCodec<? super B, T6> codec6, Function<C, T6> getter6, StreamCodec<? super B, T7> codec7, Function<C, T7> getter7, StreamCodec<? super B, T8> codec8, Function<C, T8> getter8, StreamCodec<? super B, T9> codec9, Function<C, T9> getter9, StreamCodec<? super B, T10> codec10, Function<C, T10> getter10, StreamCodec<? super B, T11> codec11, Function<C, T11> getter11, StreamCodec<? super B, T12> codec12, Function<C, T12> getter12, StreamCodec<? super B, T13> codec13, Function<C, T13> getter13, StreamCodec<? super B, T14> codec14, Function<C, T14> getter14, Function14<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, C> constructor) {
        return assemble(List.of(new Part<B, C, T1>(codec1, getter1), new Part<B, C, T2>(codec2, getter2), new Part<B, C, T3>(codec3, getter3), new Part<B, C, T4>(codec4, getter4), new Part<B, C, T5>(codec5, getter5), new Part<B, C, T6>(codec6, getter6), new Part<B, C, T7>(codec7, getter7), new Part<B, C, T8>(codec8, getter8), new Part<B, C, T9>(codec9, getter9), new Part<B, C, T10>(codec10, getter10), new Part<B, C, T11>(codec11, getter11), new Part<B, C, T12>(codec12, getter12), new Part<B, C, T13>(codec13, getter13), new Part<B, C, T14>(codec14, getter14)), f -> constructor.apply((T1) f[0], (T2) f[1], (T3) f[2], (T4) f[3], (T5) f[4], (T6) f[5], (T7) f[6], (T8) f[7], (T9) f[8], (T10) f[9], (T11) f[10], (T12) f[11], (T13) f[12], (T14) f[13]));
    }
}
