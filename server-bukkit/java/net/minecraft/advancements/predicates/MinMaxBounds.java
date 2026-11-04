// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;

import com.google.common.collect.Range;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.Supplier;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public interface MinMaxBounds<T extends Number & Comparable<T>> {
    SimpleCommandExceptionType ERROR_EMPTY = new SimpleCommandExceptionType(Component.translatable("argument.range.empty"));
    SimpleCommandExceptionType ERROR_SWAPPED = new SimpleCommandExceptionType(Component.translatable("argument.range.swapped"));

    Bounds<T> bounds();

    default Optional<T> min() {
        return bounds().min();
    }

    default Optional<T> max() {
        return bounds().max();
    }

    default boolean isAny() {
        return bounds().isAny();
    }

    static <V extends Number & Comparable<V>, B extends MinMaxBounds<V>> Function<B, DataResult<B>> validateContainedInRange(MinMaxBounds<V> allowed) {
        Range<V> outer = allowed.bounds().asRange();
        return target -> {
            Range<V> inner = target.bounds().asRange();
            return outer.encloses(inner) ? DataResult.success(target)
                : DataResult.error(() -> "Range must be within " + outer + ", but was " + inner);
        };
    }

    private static <N extends Number & Comparable<N>> boolean within(Bounds<N> bounds, N value) {
        return (bounds.min().isEmpty() || bounds.min().get().compareTo(value) <= 0)
            && (bounds.max().isEmpty() || bounds.max().get().compareTo(value) >= 0);
    }

    private static long squareLong(Integer value) {
        long wide = value.longValue();
        return wide * wide;
    }

    public static record Ints(Bounds<Integer> bounds, Bounds<Long> boundsSqr) implements MinMaxBounds<Integer> {
        public static final Ints ANY = new Ints(Bounds.any());
        public static final Codec<Ints> CODEC = Bounds.createCodec(Codec.INT).validate(Bounds::validateSwappedBoundsInCodec).xmap(Ints::new, Ints::bounds);
        public static final StreamCodec<ByteBuf, Ints> STREAM_CODEC = Bounds.createStreamCodec(ByteBufCodecs.INT).map(Ints::new, Ints::bounds);

        private Ints(Bounds<Integer> bounds) {
            this(bounds, bounds.map(MinMaxBounds::squareLong));
        }

        public static Ints exactly(int value) {
            return new Ints(Bounds.exactly(value));
        }

        public static Ints between(int min, int max) {
            return new Ints(Bounds.between(min, max));
        }

        public static Ints atLeast(int value) {
            return new Ints(Bounds.atLeast(value));
        }

        public static Ints atMost(int value) {
            return new Ints(Bounds.atMost(value));
        }

        public boolean matches(int value) {
            return within(bounds, value);
        }

        public boolean matchesSqr(long valueSqr) {
            return within(boundsSqr, valueSqr);
        }

        public static Ints fromReader(StringReader reader) throws CommandSyntaxException {
            int start = reader.getCursor();
            Bounds<Integer> read = Bounds.fromReader(reader, Integer::parseInt, CommandSyntaxException.BUILT_IN_EXCEPTIONS::readerInvalidInt);
            if (read.areSwapped()) {
                reader.setCursor(start);
                throw ERROR_SWAPPED.createWithContext(reader);
            }
            return new Ints(read);
        }
    }

    public static record Doubles(Bounds<Double> bounds, Bounds<Double> boundsSqr) implements MinMaxBounds<Double> {
        public static final Doubles ANY = new Doubles(Bounds.any());
        public static final Codec<Doubles> CODEC = Bounds.createCodec(Codec.DOUBLE).validate(Bounds::validateSwappedBoundsInCodec).xmap(Doubles::new, Doubles::bounds);
        public static final StreamCodec<ByteBuf, Doubles> STREAM_CODEC = Bounds.createStreamCodec(ByteBufCodecs.DOUBLE).map(Doubles::new, Doubles::bounds);

        private Doubles(Bounds<Double> bounds) {
            this(bounds, bounds.map(value -> value * value));
        }

        public static Doubles exactly(double value) {
            return new Doubles(Bounds.exactly(value));
        }

        public static Doubles between(double min, double max) {
            return new Doubles(Bounds.between(min, max));
        }

        public static Doubles atLeast(double value) {
            return new Doubles(Bounds.atLeast(value));
        }

        public static Doubles atMost(double value) {
            return new Doubles(Bounds.atMost(value));
        }

        public boolean matches(double value) {
            return within(bounds, value);
        }

        public boolean matchesSqr(double valueSqr) {
            return within(boundsSqr, valueSqr);
        }

        public static Doubles fromReader(StringReader reader) throws CommandSyntaxException {
            int start = reader.getCursor();
            Bounds<Double> read = Bounds.fromReader(reader, Double::parseDouble, CommandSyntaxException.BUILT_IN_EXCEPTIONS::readerInvalidDouble);
            if (read.areSwapped()) {
                reader.setCursor(start);
                throw ERROR_SWAPPED.createWithContext(reader);
            }
            return new Doubles(read);
        }
    }

    public static record FloatDegrees(Bounds<Float> bounds) implements MinMaxBounds<Float> {
        public static final FloatDegrees ANY = new FloatDegrees(Bounds.any());
        public static final Codec<FloatDegrees> CODEC = Bounds.createCodec(Codec.FLOAT).xmap(FloatDegrees::new, FloatDegrees::bounds);
        public static final StreamCodec<ByteBuf, FloatDegrees> STREAM_CODEC = Bounds.createStreamCodec(ByteBufCodecs.FLOAT).map(FloatDegrees::new, FloatDegrees::bounds);

        public static FloatDegrees fromReader(StringReader reader) throws CommandSyntaxException {
            return new FloatDegrees(Bounds.fromReader(reader, Float::parseFloat, CommandSyntaxException.BUILT_IN_EXCEPTIONS::readerInvalidFloat));
        }
    }

    public static record Bounds<T extends Number & Comparable<T>>(Optional<T> min, Optional<T> max) {
        public boolean isAny() {
            return min.isEmpty() && max.isEmpty();
        }

        public DataResult<Bounds<T>> validateSwappedBoundsInCodec() {
            return areSwapped() ? DataResult.error(() -> "Swapped bounds in range: " + min.get() + " is higher than " + max.get())
                : DataResult.success(this);
        }

        public boolean areSwapped() {
            return min.isPresent() && max.isPresent() && min.get().compareTo(max.get()) > 0;
        }

        public Range<T> asRange() {
            if (min.isPresent()) {
                return max.isPresent() ? Range.closed(min.get(), max.get()) : Range.atLeast(min.get());
            }
            return max.isPresent() ? Range.atMost(max.get()) : Range.all();
        }

        public Optional<T> asPoint() {
            return min.equals(max) ? min : Optional.empty();
        }

        public static <T extends Number & Comparable<T>> Bounds<T> any() {
            return new Bounds<T>(Optional.empty(), Optional.empty());
        }

        public static <T extends Number & Comparable<T>> Bounds<T> exactly(T value) {
            return new Bounds<>(Optional.of(value), Optional.of(value));
        }

        public static <T extends Number & Comparable<T>> Bounds<T> between(T min, T max) {
            return new Bounds<>(Optional.of(min), Optional.of(max));
        }

        public static <T extends Number & Comparable<T>> Bounds<T> atLeast(T value) {
            return new Bounds<>(Optional.of(value), Optional.empty());
        }

        public static <T extends Number & Comparable<T>> Bounds<T> atMost(T value) {
            return new Bounds<>(Optional.empty(), Optional.of(value));
        }

        public <U extends Number & Comparable<U>> Bounds<U> map(Function<T, U> mapper) {
            return new Bounds<>(min.map(mapper), max.map(mapper));
        }

        public static <T extends Number & Comparable<T>> Codec<Bounds<T>> createCodec(Codec<T> numberCodec) {
            Codec<Bounds<T>> object = RecordCodecBuilder.create(i -> i.group(
                numberCodec.optionalFieldOf("min").forGetter(Bounds::min),
                numberCodec.optionalFieldOf("max").forGetter(Bounds::max)).apply(i, Bounds::new));
            return Codec.either(object, numberCodec).xmap(either -> either.map(bounds -> bounds, Bounds::exactly), bounds -> {
                Optional<T> point = bounds.asPoint();
                return point.isPresent() ? Either.right(point.get()) : Either.left(bounds);
            });
        }

        public static <B extends ByteBuf, T extends Number & Comparable<T>> StreamCodec<B, Bounds<T>> createStreamCodec(StreamCodec<B, T> numberCodec) {
            return new StreamCodec<B, Bounds<T>>() {
                @Override
                public Bounds<T> decode(B input) {
                    byte flags = input.readByte();
                    Optional<T> low = (flags & 1) != 0 ? Optional.of(numberCodec.decode(input)) : Optional.empty();
                    Optional<T> high = (flags & 2) != 0 ? Optional.of(numberCodec.decode(input)) : Optional.empty();
                    return new Bounds<>(low, high);
                }

                @Override
                public void encode(B output, Bounds<T> value) {
                    output.writeByte((value.min().isPresent() ? 1 : 0) | (value.max().isPresent() ? 2 : 0));
                    value.min().ifPresent(v -> numberCodec.encode(output, v));
                    value.max().ifPresent(v -> numberCodec.encode(output, v));
                }
            };
        }

        public static <T extends Number & Comparable<T>> Bounds<T> fromReader(StringReader reader, Function<String, T> converter, Supplier<DynamicCommandExceptionType> parseExc) throws CommandSyntaxException {
            if (!reader.canRead()) {
                throw ERROR_EMPTY.createWithContext(reader);
            }
            int start = reader.getCursor();
            try {
                Optional<T> low = readNumber(reader, converter, parseExc);
                Optional<T> high = low;
                if (reader.canRead(2) && reader.peek() == '.' && reader.peek(1) == '.') {
                    reader.skip();
                    reader.skip();
                    high = readNumber(reader, converter, parseExc);
                }
                if (low.isEmpty() && high.isEmpty()) {
                    throw ERROR_EMPTY.createWithContext(reader);
                }
                return new Bounds<>(low, high);
            } catch (CommandSyntaxException failure) {
                reader.setCursor(start);
                throw new CommandSyntaxException(failure.getType(), failure.getRawMessage(), failure.getInput(), start);
            }
        }

        private static <T extends Number> Optional<T> readNumber(StringReader reader, Function<String, T> converter, Supplier<DynamicCommandExceptionType> parseExc) throws CommandSyntaxException {
            int start = reader.getCursor();
            while (reader.canRead() && numberChar(reader)) {
                reader.skip();
            }
            String text = reader.getString().substring(start, reader.getCursor());
            if (text.isEmpty()) {
                return Optional.empty();
            }
            try {
                return Optional.of(converter.apply(text));
            } catch (NumberFormatException failure) {
                throw parseExc.get().createWithContext(reader, text);
            }
        }

        private static boolean numberChar(StringReader reader) {
            char c = reader.peek();
            if (c >= '0' && c <= '9' || c == '-') {
                return true;
            }
            return c == '.' && (!reader.canRead(2) || reader.peek(1) != '.');
        }
    }
}
