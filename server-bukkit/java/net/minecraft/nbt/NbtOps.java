package net.minecraft.nbt;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapLike;
import java.nio.ByteBuffer;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.IntStream;
import java.util.stream.LongStream;
import java.util.stream.Stream;

public class NbtOps implements DynamicOps<Tag> {
    public static final NbtOps INSTANCE = new NbtOps();

    protected NbtOps() {}

    @Override
    public Tag empty() {
        return EndTag.INSTANCE;
    }

    @Override
    public Tag emptyMap() {
        return new CompoundTag();
    }

    @Override
    public Tag emptyList() {
        return new ListTag();
    }

    @Override
    public <U> U convertTo(DynamicOps<U> outOps, Tag input) {
        switch (input.getId()) {
            case Tag.TAG_END:
                return outOps.empty();
            case Tag.TAG_BYTE:
                return outOps.createByte(((NumericTag) input).byteValue());
            case Tag.TAG_SHORT:
                return outOps.createShort(((NumericTag) input).shortValue());
            case Tag.TAG_INT:
                return outOps.createInt(((NumericTag) input).intValue());
            case Tag.TAG_LONG:
                return outOps.createLong(((NumericTag) input).longValue());
            case Tag.TAG_FLOAT:
                return outOps.createFloat(((NumericTag) input).floatValue());
            case Tag.TAG_DOUBLE:
                return outOps.createDouble(((NumericTag) input).doubleValue());
            case Tag.TAG_STRING:
                return outOps.createString(((StringTag) input).value());
            case Tag.TAG_BYTE_ARRAY:
                return outOps.createByteList(ByteBuffer.wrap(((ByteArrayTag) input).getAsByteArray()));
            case Tag.TAG_INT_ARRAY:
                return outOps.createIntList(IntStream.of(((IntArrayTag) input).getAsIntArray()));
            case Tag.TAG_LONG_ARRAY:
                return outOps.createLongList(LongStream.of(((LongArrayTag) input).getAsLongArray()));
            case Tag.TAG_LIST: {
                List<U> converted = ((ListTag) input).stream().map(tag -> convertTo(outOps, tag)).toList();
                return outOps.createList(converted.stream());
            }
            default: {
                CompoundTag compound = (CompoundTag) input;
                Map<U, U> converted = new LinkedHashMap<>();
                for (String key : compound.keySet()) {
                    converted.put(outOps.createString(key), convertTo(outOps, compound.get(key)));
                }
                return outOps.createMap(converted);
            }
        }
    }

    @Override
    public DataResult<Number> getNumberValue(Tag input) {
        return input instanceof NumericTag tag ? DataResult.success(tag.box())
            : DataResult.error(() -> "Not a number");
    }

    @Override
    public Tag createNumeric(Number value) {
        return DoubleTag.valueOf(value.doubleValue());
    }

    @Override
    public Tag createByte(byte value) {
        return ByteTag.valueOf(value);
    }

    @Override
    public Tag createShort(short value) {
        return ShortTag.valueOf(value);
    }

    @Override
    public Tag createInt(int value) {
        return IntTag.valueOf(value);
    }

    @Override
    public Tag createLong(long value) {
        return LongTag.valueOf(value);
    }

    @Override
    public Tag createFloat(float value) {
        return FloatTag.valueOf(value);
    }

    @Override
    public Tag createDouble(double value) {
        return DoubleTag.valueOf(value);
    }

    @Override
    public Tag createBoolean(boolean value) {
        return ByteTag.valueOf((byte) (value ? 1 : 0));
    }

    @Override
    public DataResult<String> getStringValue(Tag input) {
        return input instanceof StringTag tag ? DataResult.success(tag.value())
            : DataResult.error(() -> "Not a string");
    }

    @Override
    public Tag createString(String value) {
        return StringTag.valueOf(value);
    }

    @Override
    public DataResult<Tag> mergeToList(Tag list, Tag value) {
        return mergeToList(list, List.of(value));
    }

    @Override
    public DataResult<Tag> mergeToList(Tag list, List<Tag> values) {
        if (!(list instanceof ListTag) && !(list instanceof EndTag)) {
            return DataResult.error(() -> "mergeToList called with not a list: " + list, list);
        }
        ListTag merged = new ListTag();
        if (list instanceof ListTag existing) {
            merged.addAll(existing);
        }
        merged.addAll(values);
        return DataResult.success(merged);
    }

    @Override
    public DataResult<Tag> mergeToMap(Tag map, Tag key, Tag value) {
        if (!(map instanceof CompoundTag) && !(map instanceof EndTag)) {
            return DataResult.error(() -> "mergeToMap called with not a map: " + map, map);
        }
        if (!(key instanceof StringTag name)) {
            return DataResult.error(() -> "key is not a string: " + key, map);
        }
        CompoundTag merged = map instanceof CompoundTag existing ? existing.copy() : new CompoundTag();
        merged.put(name.value(), value);
        return DataResult.success(merged);
    }

    @Override
    public DataResult<Tag> mergeToMap(Tag map, MapLike<Tag> values) {
        if (!(map instanceof CompoundTag) && !(map instanceof EndTag)) {
            return DataResult.error(() -> "mergeToMap called with not a map: " + map, map);
        }
        CompoundTag merged = map instanceof CompoundTag existing ? existing.copy() : new CompoundTag();
        values.entries().forEach(entry -> {
            if (entry.getFirst() instanceof StringTag name) {
                merged.put(name.value(), entry.getSecond());
            }
        });
        return DataResult.success(merged);
    }

    @Override
    public DataResult<Stream<Pair<Tag, Tag>>> getMapValues(Tag input) {
        if (!(input instanceof CompoundTag compound)) {
            return DataResult.error(() -> "Not a map: " + input);
        }
        return DataResult.success(compound.keySet().stream()
            .map(key -> Pair.of((Tag) StringTag.valueOf(key), compound.get(key))));
    }

    @Override
    public DataResult<MapLike<Tag>> getMap(Tag input) {
        if (!(input instanceof CompoundTag compound)) {
            return DataResult.error(() -> "Not a map: " + input);
        }
        Map<Tag, Tag> entries = new LinkedHashMap<>();
        for (String key : compound.keySet()) {
            entries.put(StringTag.valueOf(key), compound.get(key));
        }
        return DataResult.success(MapLike.forMap(entries, this));
    }

    @Override
    public Tag createMap(Stream<Pair<Tag, Tag>> map) {
        CompoundTag compound = new CompoundTag();
        map.forEach(entry -> {
            if (entry.getFirst() instanceof StringTag name) {
                compound.put(name.value(), entry.getSecond());
            }
        });
        return compound;
    }

    @Override
    public DataResult<Stream<Tag>> getStream(Tag input) {
        if (input instanceof ListTag list) {
            return DataResult.success(list.stream());
        }
        if (input instanceof ByteArrayTag bytes) {
            Tag[] boxed = new Tag[bytes.size()];
            for (int i = 0; i < boxed.length; i++) {
                boxed[i] = ByteTag.valueOf(bytes.getAsByteArray()[i]);
            }
            return DataResult.success(Stream.of(boxed));
        }
        if (input instanceof IntArrayTag ints) {
            return DataResult.success(IntStream.of(ints.getAsIntArray()).mapToObj(IntTag::valueOf));
        }
        if (input instanceof LongArrayTag longs) {
            return DataResult.success(LongStream.of(longs.getAsLongArray()).mapToObj(LongTag::valueOf));
        }
        return DataResult.error(() -> "Not a list: " + input);
    }

    @Override
    public Tag createList(Stream<Tag> input) {
        ListTag list = new ListTag();
        input.forEach(list::add);
        return list;
    }

    @Override
    public DataResult<ByteBuffer> getByteBuffer(Tag input) {
        return input instanceof ByteArrayTag bytes ? DataResult.success(ByteBuffer.wrap(bytes.getAsByteArray()))
            : DynamicOps.super.getByteBuffer(input);
    }

    @Override
    public Tag createByteList(ByteBuffer input) {
        ByteBuffer copy = input.duplicate().clear();
        byte[] data = new byte[copy.capacity()];
        copy.get(0, data, 0, data.length);
        return new ByteArrayTag(data);
    }

    @Override
    public DataResult<IntStream> getIntStream(Tag input) {
        return input instanceof IntArrayTag ints ? DataResult.success(IntStream.of(ints.getAsIntArray()))
            : DynamicOps.super.getIntStream(input);
    }

    @Override
    public Tag createIntList(IntStream input) {
        return new IntArrayTag(input.toArray());
    }

    @Override
    public DataResult<LongStream> getLongStream(Tag input) {
        return input instanceof LongArrayTag longs ? DataResult.success(LongStream.of(longs.getAsLongArray()))
            : DynamicOps.super.getLongStream(input);
    }

    @Override
    public Tag createLongList(LongStream input) {
        return new LongArrayTag(input.toArray());
    }

    @Override
    public Tag remove(Tag input, String key) {
        if (input instanceof CompoundTag compound) {
            CompoundTag copy = compound.copy();
            copy.remove(key);
            return copy;
        }
        return input;
    }

    @Override
    public String toString() {
        return "NBT";
    }
}
