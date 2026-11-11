// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.google.common.hash.HashCode;
import com.google.common.hash.HashFunction;
import com.google.common.hash.Hasher;
import com.google.common.hash.Hashing;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.ListBuilder;
import com.mojang.serialization.MapLike;
import com.mojang.serialization.RecordBuilder;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.stream.IntStream;
import java.util.stream.LongStream;
import java.util.stream.Stream;

public class HashOps implements DynamicOps<HashCode> {
    private static final byte kEmpty = 1;
    private static final byte kMapStart = 2;
    private static final byte kMapEnd = 3;
    private static final byte kListStart = 4;
    private static final byte kListEnd = 5;
    private static final byte kByte = 6;
    private static final byte kShort = 7;
    private static final byte kInt = 8;
    private static final byte kLong = 9;
    private static final byte kFloat = 10;
    private static final byte kDouble = 11;
    private static final byte kString = 12;
    private static final byte kBoolean = 13;
    private static final byte kByteArrayStart = 14;
    private static final byte kByteArrayEnd = 15;
    private static final byte kIntArrayStart = 16;
    private static final byte kIntArrayEnd = 17;
    private static final byte kLongArrayStart = 18;
    private static final byte kLongArrayEnd = 19;
    public static final byte[] EMPTY_MAP_PAYLOAD = {kMapStart, kMapEnd};
    public static final byte[] EMPTY_LIST_PAYLOAD = {kListStart, kListEnd};
    private static final Comparator<HashCode> kByHash = Comparator.comparingLong(HashCode::padToLong);
    private static final Comparator<Pair<HashCode, HashCode>> kPairOrder = Comparator.comparing(Pair<HashCode, HashCode>::getFirst, kByHash)
        .thenComparing(Pair::getSecond, kByHash);
    public static final HashOps CRC32C_INSTANCE = new HashOps(Hashing.crc32c());
    private final HashFunction hashFunction;
    private final HashCode empty;
    private final HashCode emptyMap;
    private final HashCode emptyList;
    private final HashCode trueHash;
    private final HashCode falseHash;

    public HashOps(HashFunction hashFunction) {
        this.hashFunction = hashFunction;
        this.empty = hashFunction.hashBytes(new byte[] {kEmpty});
        this.emptyMap = hashFunction.hashBytes(EMPTY_MAP_PAYLOAD);
        this.emptyList = hashFunction.hashBytes(EMPTY_LIST_PAYLOAD);
        this.falseHash = hashFunction.hashBytes(new byte[] {kBoolean, 0});
        this.trueHash = hashFunction.hashBytes(new byte[] {kBoolean, 1});
    }

    @Override
    public HashCode empty() {
        return empty;
    }

    @Override
    public HashCode emptyMap() {
        return emptyMap;
    }

    @Override
    public HashCode emptyList() {
        return emptyList;
    }

    @Override
    public HashCode createNumeric(Number value) {
        if (value instanceof Byte v) {
            return createByte(v);
        }
        if (value instanceof Short v) {
            return createShort(v);
        }
        if (value instanceof Integer v) {
            return createInt(v);
        }
        if (value instanceof Long v) {
            return createLong(v);
        }
        if (value instanceof Float v) {
            return createFloat(v);
        }
        return createDouble(value.doubleValue());
    }

    @Override
    public HashCode createByte(byte value) {
        return hashFunction.newHasher(2).putByte(kByte).putByte(value).hash();
    }

    @Override
    public HashCode createShort(short value) {
        return hashFunction.newHasher(3).putByte(kShort).putShort(value).hash();
    }

    @Override
    public HashCode createInt(int value) {
        return hashFunction.newHasher(5).putByte(kInt).putInt(value).hash();
    }

    @Override
    public HashCode createLong(long value) {
        return hashFunction.newHasher(9).putByte(kLong).putLong(value).hash();
    }

    @Override
    public HashCode createFloat(float value) {
        return hashFunction.newHasher(5).putByte(kFloat).putFloat(value).hash();
    }

    @Override
    public HashCode createDouble(double value) {
        return hashFunction.newHasher(9).putByte(kDouble).putDouble(value).hash();
    }

    @Override
    public HashCode createString(String value) {
        return hashFunction.newHasher().putByte(kString).putInt(value.length()).putUnencodedChars(value).hash();
    }

    @Override
    public HashCode createBoolean(boolean value) {
        return value ? trueHash : falseHash;
    }

    private Hasher hashPairs(Stream<Pair<HashCode, HashCode>> pairs) {
        Hasher hasher = hashFunction.newHasher().putByte(kMapStart);
        pairs.sorted(kPairOrder).forEach(pair -> hasher.putBytes(pair.getFirst().asBytes()).putBytes(pair.getSecond().asBytes()));
        return hasher.putByte(kMapEnd);
    }

    @Override
    public HashCode createMap(Stream<Pair<HashCode, HashCode>> map) {
        return hashPairs(map).hash();
    }

    @Override
    public HashCode createMap(Map<HashCode, HashCode> map) {
        return hashPairs(map.entrySet().stream().map(e -> Pair.of(e.getKey(), e.getValue()))).hash();
    }

    @Override
    public HashCode createList(Stream<HashCode> input) {
        Hasher hasher = hashFunction.newHasher().putByte(kListStart);
        input.forEach(value -> hasher.putBytes(value.asBytes()));
        return hasher.putByte(kListEnd).hash();
    }

    @Override
    public HashCode createByteList(ByteBuffer input) {
        return hashFunction.newHasher().putByte(kByteArrayStart).putBytes(input).putByte(kByteArrayEnd).hash();
    }

    @Override
    public HashCode createIntList(IntStream input) {
        Hasher hasher = hashFunction.newHasher().putByte(kIntArrayStart);
        input.forEach(hasher::putInt);
        return hasher.putByte(kIntArrayEnd).hash();
    }

    @Override
    public HashCode createLongList(LongStream input) {
        Hasher hasher = hashFunction.newHasher().putByte(kLongArrayStart);
        input.forEach(hasher::putLong);
        return hasher.putByte(kLongArrayEnd).hash();
    }

    @Override
    public HashCode remove(HashCode input, String key) {
        return input;
    }

    @Override
    public RecordBuilder<HashCode> mapBuilder() {
        return new MapHashBuilder();
    }

    @Override
    public ListBuilder<HashCode> listBuilder() {
        return new ListHashBuilder();
    }

    @Override
    public String toString() {
        return "Hash " + hashFunction;
    }

    @Override
    public <U> U convertTo(DynamicOps<U> outOps, HashCode input) {
        throw new UnsupportedOperationException("Can't convert from this type");
    }

    @Override
    public Number getNumberValue(HashCode input, Number defaultValue) {
        return defaultValue;
    }

    @Override
    public HashCode set(HashCode input, String key, HashCode value) {
        return input;
    }

    @Override
    public HashCode update(HashCode input, String key, Function<HashCode, HashCode> function) {
        return input;
    }

    @Override
    public HashCode updateGeneric(HashCode input, HashCode key, Function<HashCode, HashCode> function) {
        return input;
    }

    private static <T> DataResult<T> unsupported() {
        return DataResult.error(() -> "Unsupported operation");
    }

    @Override
    public DataResult<HashCode> get(HashCode input, String key) {
        return unsupported();
    }

    @Override
    public DataResult<HashCode> getGeneric(HashCode input, HashCode key) {
        return unsupported();
    }

    @Override
    public DataResult<Number> getNumberValue(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<Boolean> getBooleanValue(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<String> getStringValue(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<HashCode> mergeToList(HashCode prefix, HashCode value) {
        return prefix.equals(empty) ? DataResult.success(createList(Stream.of(value))) : unsupported();
    }

    @Override
    public DataResult<HashCode> mergeToList(HashCode prefix, List<HashCode> values) {
        return prefix.equals(empty) ? DataResult.success(createList(values.stream())) : unsupported();
    }

    @Override
    public DataResult<HashCode> mergeToMap(HashCode prefix, HashCode key, HashCode value) {
        return prefix.equals(empty) ? DataResult.success(createMap(Map.of(key, value))) : unsupported();
    }

    @Override
    public DataResult<HashCode> mergeToMap(HashCode prefix, Map<HashCode, HashCode> values) {
        return prefix.equals(empty) ? DataResult.success(createMap(values)) : unsupported();
    }

    @Override
    public DataResult<HashCode> mergeToMap(HashCode prefix, MapLike<HashCode> values) {
        return prefix.equals(empty) ? DataResult.success(createMap(values.entries())) : unsupported();
    }

    @Override
    public DataResult<Stream<Pair<HashCode, HashCode>>> getMapValues(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<Consumer<BiConsumer<HashCode, HashCode>>> getMapEntries(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<Stream<HashCode>> getStream(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<Consumer<Consumer<HashCode>>> getList(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<MapLike<HashCode>> getMap(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<ByteBuffer> getByteBuffer(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<IntStream> getIntStream(HashCode input) {
        return unsupported();
    }

    @Override
    public DataResult<LongStream> getLongStream(HashCode input) {
        return unsupported();
    }

    private final class MapHashBuilder extends RecordBuilder.AbstractUniversalBuilder<HashCode, List<Pair<HashCode, HashCode>>> {
        MapHashBuilder() {
            super(HashOps.this);
        }

        @Override
        protected List<Pair<HashCode, HashCode>> initBuilder() {
            return new ArrayList<>();
        }

        @Override
        protected List<Pair<HashCode, HashCode>> append(HashCode key, HashCode value, List<Pair<HashCode, HashCode>> builder) {
            builder.add(Pair.of(key, value));
            return builder;
        }

        @Override
        protected DataResult<HashCode> build(List<Pair<HashCode, HashCode>> builder, HashCode prefix) {
            return DataResult.success(hashPairs(builder.stream()).hash());
        }
    }

    private final class ListHashBuilder extends AbstractListBuilder<HashCode, Hasher> {
        ListHashBuilder() {
            super(HashOps.this);
        }

        @Override
        protected Hasher initBuilder() {
            return hashFunction.newHasher().putByte(kListStart);
        }

        @Override
        protected Hasher append(Hasher hasher, HashCode value) {
            return hasher.putBytes(value.asBytes());
        }

        @Override
        protected DataResult<HashCode> build(Hasher hasher, HashCode prefix) {
            return DataResult.success(hasher.putByte(kListEnd).hash());
        }
    }
}
